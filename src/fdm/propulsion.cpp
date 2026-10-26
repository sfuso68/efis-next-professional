#include "efis/fdm/propulsion.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace efis::fdm {
namespace {

constexpr double seconds_per_hour = 3600.0;
constexpr double watt_per_kilowatt = 1000.0;

// Costante di tempo dei transitori termici, in secondi.
//
// Tre valori diversi perche' tre masse diverse: lo scarico segue la potenza
// quasi subito, le teste in qualche decina di secondi, l'olio in minuti. Sono
// le costanti che rendono riconoscibile un guasto: una temperatura di scarico
// che crolla in un secondo e' un cilindro che ha smesso di bruciare, la stessa
// caduta in due minuti e' il motore che si raffredda in discesa.
constexpr double exhaust_time_constant_s = 4.0;
constexpr double head_time_constant_s = 25.0;
constexpr double oil_time_constant_s = 120.0;

// Primo ordine verso un valore di arrivo. La forma esponenziale e non lineare
// perche' con passo variabile la lineare dipende dal passo: lo stesso volo
// integrato a 50 Hz e a 200 Hz darebbe temperature diverse.
[[nodiscard]] double approach(double current, double target,
                              double time_constant_s, double dt_s) noexcept {
    if (time_constant_s <= 0.0) {
        return target;
    }
    const double factor = 1.0 - std::exp(-dt_s / time_constant_s);
    return current + (target - current) * factor;
}

// Caduta di potenza di un motore a pistoni aspirato con la densita'.
//
// Non e' proporzionale alla densita': la relazione di Gagg e Ferrar, usata da
// tutti i manuali di prestazioni, tiene conto del fatto che l'attrito interno
// non diminuisce con la quota. A 10.000 piedi un aspirato da' circa il 70 per
// cento della potenza, non il 74 che darebbe la proporzionalita' semplice, e
// la differenza si vede nel rateo di salita.
[[nodiscard]] double piston_power_lapse(double density_ratio) noexcept {
    return std::max((density_ratio - 0.117) / 0.883, 0.0);
}

}  // namespace

const char* propulsion_fault_name(PropulsionFault fault) noexcept {
    switch (fault) {
        case PropulsionFault::none: return "NONE";
        case PropulsionFault::fuel_pump: return "FUEL_PUMP";
        case PropulsionFault::oil_loss: return "OIL_LOSS";
        case PropulsionFault::alternator: return "ALTERNATOR";
        case PropulsionFault::single_cylinder: return "SINGLE_CYLINDER";
        case PropulsionFault::flameout: return "FLAMEOUT";
    }
    return "NONE";
}

Propulsion::Propulsion(PropulsionConfig config) noexcept : config_(config) {
    const double rated_propeller_rps = config_.rated_engine_rpm /
                                       std::max(config_.gear_ratio, 1.0e-3) / 60.0;
    const double diameter_fifth = std::pow(config_.propeller_diameter_m, 5.0);
    // Il punto di progetto comprende lo scarico dell'elica alla velocita' di
    // crociera: la stessa riduzione che absorbed_power_kw applica in volo
    // viene divisa qui, cosi' il regime di equilibrio in crociera e' quello
    // nominale e non uno piu' alto. Tarare il coefficiente sulla condizione
    // statica darebbe un motore che fuorigiri appena l'aeroplano accelera.
    const double design_unloading = 1.0 - 0.30 * std::min(
        config_.propeller_design_advance_ratio, 1.5);
    const double denominator = isa_sea_level_density_kg_m3 *
                               std::pow(rated_propeller_rps, 3.0) * diameter_fifth *
                               std::max(design_unloading, 0.1);
    power_coefficient_ = denominator > 0.0
                             ? config_.rated_power_kw * watt_per_kilowatt / denominator
                             : 0.0;
    reset(false, 15.0);
}

void Propulsion::reset(bool running, double ambient_temperature_c) noexcept {
    state_ = PropulsionState{};
    state_.running = running;
    if (running) {
        state_.engine_rpm = config_.rated_engine_rpm * 0.75;
        state_.propeller_rpm = state_.engine_rpm / std::max(config_.gear_ratio, 1.0e-3);
        state_.oil_temperature_c = ambient_temperature_c + 70.0;
        state_.cylinder_head_c.fill(ambient_temperature_c + 85.0);
        state_.exhaust_gas_c.fill(700.0);
        state_.bus_voltage_v = 14.2;
    } else {
        state_.oil_temperature_c = ambient_temperature_c;
        state_.cylinder_head_c.fill(ambient_temperature_c);
        state_.exhaust_gas_c.fill(ambient_temperature_c);
        state_.bus_voltage_v = 12.4;
    }
}

double Propulsion::available_power_kw(const Controls& controls,
                                      const AirState& air) const noexcept {
    if (!state_.running) {
        return 0.0;
    }

    const double density_ratio = air.density_kg_m3 / isa_sea_level_density_kg_m3;
    const double throttle = std::clamp(controls.throttle, 0.0, 1.0);

    double power = 0.0;
    if (config_.kind == EngineKind::piston_prop) {
        // La frazione di minimo non e' zero: un motore al minimo produce
        // potenza, ed e' quella che muove l'aeroplano al rullaggio.
        const double throttle_fraction = 0.08 + 0.92 * throttle;

        // La potenza disponibile dipende dal regime: a regime basso il motore
        // non da' la potenza di targa qualunque sia la manetta. Senza questo
        // termine l'elica riceverebbe tutta la potenza anche da fermo e il
        // modello dell'avviamento non avrebbe senso.
        const double rpm_fraction = std::clamp(
            0.25 + 0.75 * state_.engine_rpm / config_.rated_engine_rpm, 0.0, 1.0);
        const double mixture = 0.60 + 0.40 * std::clamp(controls.mixture, 0.0, 1.0);
        power = config_.rated_power_kw * piston_power_lapse(density_ratio) *
                throttle_fraction * rpm_fraction * mixture;
    } else {
        // Turbina: la caduta con la densita' e' meno severa che per un
        // aspirato, e la potenza non dipende dal regime dell'elica perche' il
        // generatore di gas gira per conto proprio. La quota di appiattimento
        // (flat rating) non e' modellata, e va saputo: sotto i 10.000 piedi
        // questo modello da' piu' potenza di una PT6 vera, che resta limitata
        // dalla coppia.
        const double throttle_fraction = 0.10 + 0.90 * throttle;
        power = config_.rated_power_kw * std::pow(std::max(density_ratio, 0.0), 0.8) *
                throttle_fraction;
    }

    switch (fault_) {
        case PropulsionFault::fuel_pump:
            // Pressione insufficiente: la portata non basta piu' alla potenza
            // richiesta. Il motore gira, male.
            power *= 0.55;
            break;
        case PropulsionFault::single_cylinder:
            // Un cilindro su quattro che non brucia: la potenza non cala di un
            // quarto, perche' il cilindro continua a pompare e a frenare.
            power *= 0.70;
            break;
        case PropulsionFault::oil_loss:
        case PropulsionFault::alternator:
        case PropulsionFault::flameout:
        case PropulsionFault::none:
            break;
    }
    return std::max(power, 0.0);
}

double Propulsion::throttle_for_shaft_power(double power_kw,
                                            const AirState& air) const noexcept {
    // Bisezione sulla curva di potenza disponibile. Venti passaggi bastano a
    // scendere sotto il millesimo di escursione, e la curva e' monotona nella
    // manetta per costruzione, quindi la bisezione converge sempre.
    Propulsion probe(config_);
    probe.state_ = state_;
    probe.state_.running = true;
    probe.fault_ = fault_;

    Controls trial{};
    trial.mixture = 1.0;
    double low = 0.0;
    double high = 1.0;
    for (int iteration = 0; iteration < 20; ++iteration) {
        const double middle = 0.5 * (low + high);
        trial.throttle = middle;
        if (probe.available_power_kw(trial, air) < power_kw) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return std::clamp(0.5 * (low + high), 0.0, 1.0);
}

double Propulsion::absorbed_power_kw(double propeller_rpm,
                                     const AirState& air,
                                     double true_airspeed_m_s,
                                     double pitch_command) const noexcept {
    const double revolutions_per_second = std::max(propeller_rpm, 0.0) / 60.0;
    if (revolutions_per_second <= 0.0) {
        return 0.0;
    }

    const double advance_ratio = true_airspeed_m_s /
                                 std::max(revolutions_per_second *
                                              config_.propeller_diameter_m,
                                          1.0e-3);

    // Elica a passo fisso: avanzando, le pale incontrano il flusso con
    // incidenza minore e assorbono meno, quindi il regime sale. E' il
    // comportamento che fa prendere giri in picchiata e che un limitatore di
    // sovragiri deve vedere.
    double coefficient = power_coefficient_ * (1.0 - 0.30 * std::min(advance_ratio, 1.5));

    // Elica a passo variabile: il regolatore cambia il passo per tenere il
    // regime, quindi e' il comando dell'elica a decidere quanta potenza
    // l'elica assorbe. Il comando a zero significa passo minimo, carico
    // minimo.
    if (config_.kind == EngineKind::turboprop) {
        coefficient = power_coefficient_ * (0.35 + 0.65 * std::clamp(pitch_command, 0.0, 1.0));
    }

    const double power_w = std::max(coefficient, 0.0) * air.density_kg_m3 *
                           std::pow(revolutions_per_second, 3.0) *
                           std::pow(config_.propeller_diameter_m, 5.0);
    return power_w / watt_per_kilowatt;
}

const PropulsionState& Propulsion::step(const Controls& controls,
                                        const AirState& air,
                                        double true_airspeed_m_s,
                                        double dt_s) noexcept {
    const double ambient_c = air.temperature_k - 273.15;

    if (fault_ == PropulsionFault::flameout) {
        state_.running = false;
    }

    const bool fuel_available = controls.fuel_pump_on ||
                                config_.kind == EngineKind::piston_prop;
    state_.starter_engaged = controls.starter && !state_.running;

    // Accensione. Il motore parte quando l'avviamento lo ha portato oltre il
    // regime di trascinamento e magneti e carburante sono presenti. Tre
    // condizioni e non una: il banco deve poter riprodurre l'avviamento
    // mancato, che e' il primo guasto che un pilota incontra.
    if (!state_.running && controls.starter && controls.magnetos_on && fuel_available &&
        fault_ != PropulsionFault::flameout && fault_ != PropulsionFault::fuel_pump &&
        state_.engine_rpm > config_.starter_rpm * 0.9) {
        state_.running = true;
    }
    if (state_.running && (!controls.magnetos_on || !fuel_available)) {
        state_.running = false;
    }

    const double available = available_power_kw(controls, air);

    if (config_.kind == EngineKind::turboprop && state_.running) {
        // Regolatore: il regime dell'elica segue il comando con un ritardo, e
        // la potenza all'albero e' quella disponibile. Non si integra la
        // dinamica rotazionale perche' il regolatore la annulla per
        // costruzione, e fingere di integrarla darebbe un transitorio che il
        // regolatore vero non ha.
        const double commanded = config_.idle_rpm +
                                 (config_.rated_engine_rpm - config_.idle_rpm) *
                                     std::clamp(controls.propeller, 0.0, 1.0);
        state_.engine_rpm = approach(state_.engine_rpm, commanded, 1.5, dt_s);
        state_.shaft_power_kw = available;
    } else {
        // Motore a pistoni: bilancio fra potenza disponibile e potenza
        // assorbita dall'elica, integrato sull'inerzia rotante. E' il modo in
        // cui il regime si stabilisce davvero, e fa uscire gratis due
        // comportamenti che una tabella dovrebbe elencare: il regime che cala
        // quando l'elica si carica in salita e quello che sale in picchiata.
        const double propeller_rpm = state_.engine_rpm /
                                     std::max(config_.gear_ratio, 1.0e-3);
        const double absorbed = absorbed_power_kw(propeller_rpm, air, true_airspeed_m_s,
                                                  controls.propeller);

        double net_power_w = (available - absorbed) * watt_per_kilowatt;

        // L'avviamento e' una coppia, non una potenza: a regime nullo la
        // potenza sarebbe nulla e il motore non si muoverebbe.
        double starter_torque_nm = 0.0;
        if (state_.starter_engaged && controls.magnetos_on) {
            starter_torque_nm = config_.rated_power_kw * watt_per_kilowatt /
                                (config_.rated_engine_rpm * 2.0 * std::numbers::pi / 60.0) *
                                0.45;
        }

        // Attrito interno quando il motore e' spento: senza di esso l'elica
        // continuerebbe a girare per sempre, e il modello dell'autorotazione
        // non e' questo.
        if (!state_.running) {
            net_power_w = -absorbed * watt_per_kilowatt;
        }

        const double omega = std::max(state_.engine_rpm * 2.0 * std::numbers::pi / 60.0,
                                      1.0);
        const double torque_nm = net_power_w / omega + starter_torque_nm;
        const double inertia = std::max(config_.rotating_inertia_kg_m2, 1.0e-3);
        const double new_omega = std::max(omega + torque_nm / inertia * dt_s, 0.0);
        state_.engine_rpm = new_omega * 60.0 / (2.0 * std::numbers::pi);

        if (state_.running) {
            state_.engine_rpm = std::max(state_.engine_rpm, config_.idle_rpm * 0.6);
        }
        state_.shaft_power_kw = std::max(available, 0.0);
    }

    if (!state_.running && !state_.starter_engaged) {
        state_.engine_rpm = std::max(state_.engine_rpm - 400.0 * dt_s, 0.0);
        state_.shaft_power_kw = 0.0;
    }
    state_.propeller_rpm = state_.engine_rpm / std::max(config_.gear_ratio, 1.0e-3);

    // --- Spinta ------------------------------------------------------------
    //
    // Teoria del disco attuatore in volo avanzato, risolta per la spinta:
    //   P = T V + T^(3/2) / sqrt(2 rho A)
    // Il primo termine e' il lavoro utile, il secondo la potenza spesa nella
    // scia. La relazione vale sia da fermo -- dove da' la spinta statica
    // (2 rho A P^2)^(1/3) -- sia in crociera, dove tende a eta P / V. E' il
    // motivo per cui non serve ne' una tabella di rendimento ne' un raccordo
    // fra i due regimi: sono lo stesso fenomeno, e un raccordo arbitrario
    // avrebbe prodotto una discontinuita' di accelerazione proprio durante la
    // corsa di decollo.
    const double disc_area = std::numbers::pi *
                             std::pow(config_.propeller_diameter_m * 0.5, 2.0);
    const double effective_power_w = std::max(state_.shaft_power_kw, 0.0) *
                                     watt_per_kilowatt *
                                     std::clamp(config_.propeller_peak_efficiency,
                                                0.0, 1.0);
    const double wake_factor = std::sqrt(2.0 * air.density_kg_m3 * disc_area);
    double thrust = std::cbrt(wake_factor * wake_factor * effective_power_w *
                              effective_power_w);
    const double speed = std::max(true_airspeed_m_s, 0.0);
    for (int iteration = 0; iteration < 4 && effective_power_w > 0.0; ++iteration) {
        const double residual = thrust * speed +
                                std::pow(std::max(thrust, 0.0), 1.5) / wake_factor -
                                effective_power_w;
        const double slope = speed + 1.5 * std::sqrt(std::max(thrust, 1.0e-6)) /
                                         wake_factor;
        thrust = std::max(thrust - residual / slope, 0.0);
    }
    state_.thrust_n = effective_power_w > 0.0 ? thrust : 0.0;
    state_.propeller_efficiency = state_.shaft_power_kw > 0.0
                                      ? state_.thrust_n * speed /
                                            (state_.shaft_power_kw * watt_per_kilowatt)
                                      : 0.0;
    state_.advance_ratio = state_.propeller_rpm > 0.0
                               ? speed / (state_.propeller_rpm / 60.0 *
                                          config_.propeller_diameter_m)
                               : 0.0;

    // --- Consumo -----------------------------------------------------------
    if (state_.running) {
        const double idle_floor = config_.bsfc_kg_kwh * config_.rated_power_kw * 0.06;
        const double hourly = std::max(config_.bsfc_kg_kwh * state_.shaft_power_kw,
                                       idle_floor);
        state_.fuel_flow_kg_s = hourly / seconds_per_hour;
    } else {
        state_.fuel_flow_kg_s = 0.0;
    }
    state_.fuel_used_kg += state_.fuel_flow_kg_s * dt_s;

    // Pressione di alimentazione, in pollici di mercurio: cio' che l'EMS
    // mostra. A motore spento e' la pressione ambiente, che e' giusto: il
    // collettore e' aperto all'atmosfera.
    const double ambient_inhg = air.pressure_pa / pascal_per_inch_mercury;
    state_.manifold_pressure_inhg =
        state_.running ? ambient_inhg * (0.32 + 0.68 * std::clamp(controls.throttle,
                                                                 0.0, 1.0))
                       : ambient_inhg;

    update_thermals(controls, air, dt_s);
    update_electrical(controls, dt_s);

    if (config_.kind == EngineKind::turboprop) {
        const double power_fraction = std::clamp(state_.shaft_power_kw /
                                                     config_.rated_power_kw,
                                                 0.0, 1.2);
        state_.gas_generator_pct = state_.running ? 52.0 + 45.0 * power_fraction : 0.0;
        state_.interstage_turbine_c = state_.running
                                          ? approach(state_.interstage_turbine_c,
                                                     ambient_c + 380.0 +
                                                         420.0 * power_fraction,
                                                     exhaust_time_constant_s, dt_s)
                                          : approach(state_.interstage_turbine_c,
                                                     ambient_c, 60.0, dt_s);
        state_.torque_pct = power_fraction * 100.0;
    }

    return state_;
}

void Propulsion::update_thermals(const Controls& controls,
                                 const AirState& air,
                                 double dt_s) noexcept {
    const double ambient_c = air.temperature_k - 273.15;
    const double power_fraction = std::clamp(state_.shaft_power_kw /
                                                 std::max(config_.rated_power_kw, 1.0),
                                             0.0, 1.2);

    // Teste: temperatura ambiente piu' un contributo della potenza. I valori
    // sono tarati su un motore di classe Rotax 912, teste raffreddate a
    // liquido: 105 gradi al 75 per cento di potenza con aria a 15, 70 al
    // minimo. Un motore raffreddato ad aria sta cento gradi piu' in alto, e
    // quando il banco ne modellera' uno questi due numeri saranno i suoi.
    double head_target = ambient_c + 50.0 + 55.0 * power_fraction;
    double oil_target = ambient_c + 60.0 + 30.0 * power_fraction;

    // Miscela povera: la combustione finisce piu' tardi e la temperatura di
    // scarico sale. La relazione vera ha un massimo attorno al rapporto
    // stechiometrico e ricade smagrendo oltre; qui e' monotona, ed e' una
    // semplificazione dichiarata: serve a provare la pagina EMS, non a
    // insegnare a smagrire.
    const double lean_rise = 90.0 * (1.0 - std::clamp(controls.mixture, 0.0, 1.0));
    double exhaust_target = state_.running ? 430.0 + 400.0 * std::pow(power_fraction, 0.7) +
                                                 lean_rise
                                           : ambient_c;

    if (fault_ == PropulsionFault::oil_loss) {
        // L'olio che manca non raffredda: la temperatura sale e ci mette meno
        // del normale, perche' la massa che la frenava e' in parte andata.
        oil_target += 45.0;
        head_target += 20.0;
    }

    state_.oil_temperature_c = approach(state_.oil_temperature_c, oil_target,
                                        state_.running ? oil_time_constant_s : 200.0,
                                        dt_s);

    for (std::size_t index = 0; index < state_.cylinder_head_c.size(); ++index) {
        // Dispersione fra cilindri: due gradi per cilindro. Non e' rumore
        // estetico: un EMS che mostra quattro teste identiche non fa vedere
        // all'operatore cosa sia una dispersione normale, e quindi non gli fa
        // riconoscere quella anomala.
        const double spread = static_cast<double>(index) * 2.0;
        double target = head_target + spread;
        double exhaust = exhaust_target + static_cast<double>(index) * 6.0;

        if (fault_ == PropulsionFault::single_cylinder && index == 1) {
            // Il cilindro che non brucia: lo scarico crolla perche' non ci
            // passa piu' combustione, la testa scende perche' non la scalda
            // piu' nulla. I due vanno insieme, ed e' la loro combinazione a
            // identificare il guasto.
            exhaust = ambient_c + 120.0;
            target = ambient_c + 25.0;
        }

        state_.cylinder_head_c[index] = approach(state_.cylinder_head_c[index], target,
                                                 head_time_constant_s, dt_s);
        state_.exhaust_gas_c[index] = approach(state_.exhaust_gas_c[index], exhaust,
                                               exhaust_time_constant_s, dt_s);
    }

    // Pressione dell'olio: cresce con il regime. A motore spento e' zero, e
    // deve essere zero: una pressione plausibile a motore fermo e' esattamente
    // il valore falso che un EMS non deve mostrare.
    double oil_pressure = 0.0;
    if (state_.running) {
        oil_pressure = 25.0 + 35.0 * std::clamp(state_.engine_rpm /
                                                    config_.rated_engine_rpm,
                                                0.0, 1.2);
        if (state_.oil_temperature_c < 50.0) {
            // Olio freddo: piu' viscoso, pressione piu' alta. E' il motivo per
            // cui il manuale vieta di dare potenza prima dei cinquanta gradi.
            oil_pressure += 12.0;
        }
        if (fault_ == PropulsionFault::oil_loss) {
            oil_pressure *= 0.32;
        }
    } else if (state_.starter_engaged) {
        oil_pressure = 8.0;
    }
    state_.oil_pressure_psi = approach(state_.oil_pressure_psi, oil_pressure, 1.5, dt_s);

    double fuel_pressure = 0.0;
    if (fault_ == PropulsionFault::fuel_pump) {
        fuel_pressure = 0.8;
    } else if (controls.fuel_pump_on) {
        fuel_pressure = config_.kind == EngineKind::turboprop ? 22.0 : 4.6;
    } else if (state_.running) {
        // Pompa elettrica spenta ma motore in moto: resta la pompa meccanica, a
        // pressione piu' bassa. Sull'LSA e' la differenza fra decollo
        // consentito e non consentito.
        fuel_pressure = config_.kind == EngineKind::turboprop ? 12.0 : 2.4;
    }
    state_.fuel_pressure_psi = approach(state_.fuel_pressure_psi, fuel_pressure, 2.0,
                                        dt_s);
    (void)air;
}

void Propulsion::update_electrical(const Controls& controls, double dt_s) noexcept {
    const bool alternator_online = state_.running &&
                                   fault_ != PropulsionFault::alternator &&
                                   state_.engine_rpm > config_.idle_rpm * 0.8;

    if (alternator_online) {
        state_.bus_voltage_v = approach(state_.bus_voltage_v, 14.2, 0.8, dt_s);

        // Con l'alternatore in servizio la batteria si ricarica. Non si
        // modella la chimica: si riporta la tensione a riposo verso il valore
        // di una batteria carica, con la costante di tempo di una ricarica.
        battery_voltage_ = approach(battery_voltage_, 12.6, 600.0, dt_s);

        // Corrente erogata: il carico di bordo piu' la ricarica della
        // batteria, che decresce man mano che la batteria si riprende. Qui e'
        // un valore fisso di carico piu' un contributo del riscaldamento del
        // Pitot, che e' l'utenza piu' pesante dell'LSA.
        const double load = 9.0 + (controls.fuel_pump_on ? 2.5 : 0.0);
        state_.alternator_current_a = approach(state_.alternator_current_a, load, 1.0,
                                               dt_s);
    } else {
        // Sulla sola batteria, e in due tempi perche' sono due fenomeni.
        //
        // Primo: la sbarra cade subito alla tensione della batteria. Il
        // regolatore smette di erogare e resta solo lei, in meno di un
        // secondo. Modellarlo come una discesa lenta da quattordici volt era
        // sbagliato in modo pratico: dopo dieci minuti di guasto la tensione
        // era ancora dodici e otto, cioe' un valore che nessun allarme di
        // bassa tensione considera anomalo, e il guasto non si poteva provare.
        //
        // Secondo: la batteria si scarica, e lentamente. Questa e' la deriva
        // che fa scattare l'allarme, e il banco deve poterla mostrare in tempo
        // utile a provarlo.
        battery_voltage_ = approach(battery_voltage_, 10.8, 1200.0, dt_s);
        state_.bus_voltage_v = approach(state_.bus_voltage_v, battery_voltage_, 0.8,
                                        dt_s);
        state_.alternator_current_a = approach(state_.alternator_current_a, 0.0, 1.0,
                                               dt_s);
    }
}

}  // namespace efis::fdm
