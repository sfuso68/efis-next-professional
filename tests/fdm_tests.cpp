// Prove del motore di volo.
//
// Hanno un eseguibile proprio perche' verificano una categoria di proprieta'
// diversa da quella del nucleo: non che un dato sia trasportato correttamente,
// ma che il moto, l'atmosfera e i sensori simulati si comportino come la fisica
// e come l'hardware. Un motore di volo sbagliato non fa cadere nessun test del
// nucleo: produce numeri plausibili, che e' il modo peggiore di sbagliare.
//
// Tre gruppi di prove sono quelli che contano, e vale la pena di dire perche'.
//
// Le velocita' dichiarate contro quelle prodotte. La cellula dichiara una
// velocita' di stallo che finisce sul nastro dell'EFIS; il modello ne produce
// una integrando il moto. Se le due divergono, il nastro mente e nessuno se ne
// accorge guardando il display.
//
// La virata contro la relazione del nucleo. `efis::coordinated_turn_rate_deg_s`
// calcola il rateo di virata da inclinazione e velocita', e il confronto
// incrociato di ADR-004 se ne servira' per giudicare un giroscopio. Qui il
// rateo arriva invece dall'integrazione del moto, per una via che non contiene
// quella formula. Le due vie devono concordare: se concordano, entrambe
// descrivono la stessa fisica.
//
// Il rumore dei sensori contro le soglie di ADR-004. Le risoluzioni dello
// StasisMonitor sono tarate sul rumore misurato del banco reale. Se il banco
// simulato fosse piu' silenzioso, una taratura inerte passerebbe le prove qui e
// fallirebbe in volo.

#include "efis/core/flight_dynamics.hpp"
#include "efis/fdm/airframe.hpp"
#include "efis/fdm/atmosphere.hpp"
#include "efis/fdm/flight_bench.hpp"
#include "efis/fdm/propulsion.hpp"
#include "efis/fdm/scenario_pilot.hpp"
#include "efis/fdm/sim_vars.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace {

using namespace efis;
using namespace efis::fdm;

constexpr double radians_to_degrees = 180.0 / std::numbers::pi;

int failures = 0;

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

void require_near(double value, double expected, double tolerance,
                  const char* message) {
    if (!(std::abs(value - expected) <= tolerance)) {
        std::cerr << "FAILED: " << message << " (atteso " << expected << " +/- "
                  << tolerance << ", ottenuto " << value << ")\n";
        ++failures;
    }
}

// --- Atmosfera ---------------------------------------------------------------

void test_atmosphere() {
    const Atmosphere standard{};

    const AirState sea_level = standard.at_altitude(0.0);
    require_near(sea_level.pressure_pa, 101325.0, 1.0, "pressione al livello del mare");
    require_near(sea_level.temperature_k, 288.15, 0.01, "temperatura al livello del mare");
    require_near(sea_level.density_kg_m3, 1.225, 0.001, "densita' al livello del mare");

    // Valori di tabella ISA. Non sono numeri scelti da me: sono quelli che
    // qualunque manuale riporta, e per questo sono una prova e non una
    // ripetizione del codice.
    const AirState at_5000 = standard.at_altitude(5000.0 * metres_per_foot);
    require_near(at_5000.pressure_pa, 84307.0, 60.0, "pressione a 5000 ft");
    require_near(at_5000.temperature_k, 278.24, 0.1, "temperatura a 5000 ft");

    const AirState at_10000 = standard.at_altitude(10000.0 * metres_per_foot);
    require_near(at_10000.pressure_pa, 69682.0, 80.0, "pressione a 10000 ft");
    require_near(at_10000.temperature_k, 268.34, 0.1, "temperatura a 10000 ft");

    // Tropopausa: 36089 piedi, dove la temperatura si ferma a -56,5 gradi.
    const AirState tropopause = standard.at_altitude(11000.0);
    require_near(tropopause.temperature_k, 216.65, 0.01, "temperatura alla tropopausa");
    require_near(tropopause.pressure_pa, 22632.0, 40.0, "pressione alla tropopausa");

    // Sopra la tropopausa l'andamento e' esponenziale, non piu' lineare: a
    // 45000 piedi la pressione ISA vale 14748 pascal.
    const AirState high = standard.at_altitude(45000.0 * metres_per_foot);
    require_near(high.pressure_pa, 14748.0, 120.0, "pressione a 45000 ft");
    require_near(high.temperature_k, 216.65, 0.01, "temperatura a 45000 ft");

    // Quota di pressione: invertire la relazione deve riportare alla quota di
    // partenza. E' la proprieta' che l'altimetro dell'EFIS sfrutta.
    for (const double altitude_ft : {0.0, 1500.0, 8000.0, 25000.0, 41000.0}) {
        const double altitude_m = altitude_ft * metres_per_foot;
        const double pressure = standard.at_altitude(altitude_m).pressure_pa;
        require_near(Atmosphere::pressure_altitude_m(pressure) * feet_per_metre,
                     altitude_ft, 1.0, "andata e ritorno della quota di pressione");
    }

    // Regolazione barometrica: dieci ettopascal sotto la standard spostano la
    // quota indicata di circa 270 piedi in basso. E' il numero che ogni pilota
    // conosce, e il modello deve produrlo senza che sia scritto da nessuna
    // parte.
    const double pressure_at_1000 = standard.at_altitude(1000.0 * metres_per_foot)
                                        .pressure_pa;
    const double indicated_with_low_qnh =
        Atmosphere::indicated_altitude_m(pressure_at_1000, 1003.25 *
                                                               pascal_per_hectopascal) *
        feet_per_metre;
    require_near(indicated_with_low_qnh, 1000.0 - 273.0, 8.0,
                 "ventisette piedi per ettopascal");

    // Giornata calda: la quota di densita' sta sopra quella geometrica. E' il
    // fenomeno che riduce le prestazioni al decollo d'estate.
    Atmosphere hot{};
    hot.temperature_offset_k = 20.0;
    const double field_m = 500.0 * metres_per_foot;
    require(hot.density_altitude_m(field_m) > field_m + 500.0 * metres_per_foot,
            "la quota di densita' sale in giornata calda");
}

void test_air_data_relations() {
    const Atmosphere standard{};

    // Al livello del mare standard la velocita' calibrata e la vera
    // coincidono: e' la definizione stessa di velocita' calibrata.
    const AirState sea_level = standard.at_altitude(0.0);
    const double cas_sea_level = 100.0 * metres_per_second_per_knot;
    require_near(tas_from_cas_m_s(cas_sea_level, sea_level) *
                     knots_per_metre_per_second,
                 100.0, 0.05, "al livello del mare CAS e TAS coincidono");

    // A diecimila piedi cento nodi calibrati valgono circa 116 veri. Il
    // rapporto delle radici delle densita' darebbe 116,2: la forma
    // compressibile usata qui ne da' poco meno, e la differenza e' reale.
    const AirState at_10000 = standard.at_altitude(10000.0 * metres_per_foot);
    const double tas_kt = tas_from_cas_m_s(cas_sea_level, at_10000) *
                          knots_per_metre_per_second;
    require(tas_kt > 114.0 && tas_kt < 117.0, "TAS a 10000 ft fra 114 e 117 nodi");

    // Andata e ritorno fra calibrata e vera, in quota, dove le due
    // differiscono molto: se la coppia di funzioni non si invertisse, un EFIS
    // tarato con una e provato con l'altra sbaglierebbe in modo silenzioso.
    const AirState at_25000 = standard.at_altitude(25000.0 * metres_per_foot);
    for (const double knots : {60.0, 120.0, 250.0}) {
        const double cas = knots * metres_per_second_per_knot;
        const double tas = tas_from_cas_m_s(cas, at_25000);
        require_near(cas_from_tas_m_s(tas, at_25000) * knots_per_metre_per_second,
                     knots, 0.05, "andata e ritorno fra CAS e TAS in quota");
    }

    // La pressione d'impatto si inverte nella velocita' calibrata che l'ha
    // prodotta: e' il calcolo che gira nel calcolatore dati aria dell'EFIS.
    for (const double knots : {40.0, 100.0, 200.0}) {
        const double cas = knots * metres_per_second_per_knot;
        require_near(cas_from_impact_pressure_m_s(impact_pressure_from_cas_pa(cas)) *
                         knots_per_metre_per_second,
                     knots, 0.01, "andata e ritorno della pressione d'impatto");
    }
}

// --- Cellula -----------------------------------------------------------------

void test_airframe_declared_speeds() {
    for (const Airframe& airframe : {light_sport_airframe(), professional_airframe()}) {
        const double mass = airframe.mass.max_mass_kg;
        const double clean = stall_speed_kcas(airframe, mass, 0.0);
        const double landing = stall_speed_kcas(airframe, mass,
                                                airframe.flaps.back().angle_deg);

        // La velocita' sul nastro e quella del modello devono essere la stessa
        // cosa. Un nodo di tolleranza e' l'arrotondamento con cui si scrive un
        // limite, non un margine.
        require_near(clean, airframe.limits.stall_clean_kcas, 1.0,
                     "velocita' di stallo pulita dichiarata e prodotta");
        require_near(landing, airframe.limits.stall_landing_kcas, 1.0,
                     "velocita' di stallo con flap dichiarata e prodotta");
        require(landing < clean, "i flap abbassano la velocita' di stallo");

        // Un LSA ha per definizione una velocita' di stallo in configurazione
        // di atterraggio non superiore a 45 nodi calibrati. La cellula di
        // riferimento deve rispettarla, altrimenti non e' una cellula LSA e il
        // banco proverebbe il nastro sbagliato.
        if (airframe.category == Category::light_sport) {
            require(landing <= 45.0, "lo stallo dell'LSA resta entro 45 nodi");
        }
    }
}

void test_lift_curve() {
    const Airframe airframe = light_sport_airframe();
    const AeroCoefficients& aero = airframe.aero;
    const double stall_alpha_rad = aero.stall_alpha_deg / radians_to_degrees;

    const double peak = lift_coefficient(aero, stall_alpha_rad);
    require_near(peak, max_lift_coefficient(aero), 0.005,
                 "il massimo del modello e' quello dichiarato dalle derivate");

    // Prima dello stallo la portanza cresce; dopo cala. La seconda meta' e'
    // quella che conta: un modello che tronca al massimo da' un aeroplano che
    // non stalla, e l'allarme di stallo non si potrebbe provare.
    require(lift_coefficient(aero, stall_alpha_rad * 0.5) < peak,
            "sotto lo stallo la portanza e' minore del massimo");
    require(lift_coefficient(aero, stall_alpha_rad + 8.0 / radians_to_degrees) <
                peak * 0.8,
            "oltre lo stallo la portanza cade");

    // Portanza nulla a incidenza negativa: il profilo ha curvatura, quindi
    // l'incidenza di portanza nulla e' negativa e non zero.
    require(lift_coefficient(aero, 0.0) > 0.0,
            "a incidenza nulla la portanza e' positiva");
    require_near(lift_coefficient(aero, -aero.lift_zero / aero.lift_alpha), 0.0, 1.0e-9,
                 "portanza nulla all'incidenza di portanza nulla");

    // Interpolazione fra tacche di flap: il flap vero si muove, e un salto fra
    // tacche produrrebbe uno scalino di accelerazione che l'EFIS mostrerebbe
    // come un transitorio inesistente.
    const FlapEffect half = flap_effect(airframe, 7.5);
    const FlapEffect full = flap_effect(airframe, 15.0);
    require(half.delta_lift > 0.0 && half.delta_lift < full.delta_lift,
            "l'effetto del flap e' interpolato fra le tacche");
    require_near(full.limit_kcas, airframe.flaps[1].limit_kcas, 0.01,
                 "il limite di velocita' e' quello della tacca");
}

// --- Propulsione -------------------------------------------------------------

void test_propulsion_start_and_power() {
    const Airframe airframe = light_sport_airframe();
    const Atmosphere atmosphere{};
    const AirState air = atmosphere.at_altitude(0.0);

    Propulsion propulsion(airframe.propulsion);
    propulsion.reset(false, 15.0);

    Controls controls{};
    controls.magnetos_on = false;
    controls.starter = true;
    controls.fuel_pump_on = true;
    for (int step = 0; step < 500; ++step) {
        propulsion.step(controls, air, 0.0, 0.02);
    }
    require(!propulsion.state().running,
            "senza magneti il motore non parte nemmeno girando");
    require_near(propulsion.state().oil_pressure_psi, 8.0, 3.0,
                 "sotto l'avviatore la pressione dell'olio e' bassa ma non nulla");

    controls.magnetos_on = true;
    for (int step = 0; step < 500; ++step) {
        propulsion.step(controls, air, 0.0, 0.02);
    }
    require(propulsion.state().running, "con magneti, carburante e avviatore parte");

    controls.starter = false;
    controls.throttle = 0.0;
    for (int step = 0; step < 1500; ++step) {
        propulsion.step(controls, air, 0.0, 0.02);
    }
    const PropulsionState& idle = propulsion.state();
    require(idle.running && idle.engine_rpm > 500.0,
            "al minimo il motore resta in moto");
    require(idle.oil_pressure_psi > 20.0, "al minimo la pressione dell'olio c'e'");
    require(idle.fuel_flow_kg_s > 0.0, "un motore in moto consuma");

    // Piena potenza in crociera: il regime di equilibrio non deve superare
    // quello nominale. Un'elica tarata sulla condizione statica lo supera, e
    // il contagiri dell'EMS lo mostrerebbe come un fuorigiri permanente.
    controls.throttle = 1.0;
    const double cruise_tas = 110.0 * metres_per_second_per_knot;
    for (int step = 0; step < 3000; ++step) {
        propulsion.step(controls, air, cruise_tas, 0.02);
    }
    const PropulsionState& cruise = propulsion.state();
    require(cruise.engine_rpm < airframe.propulsion.rated_engine_rpm * 1.05,
            "a piena potenza in crociera il regime resta entro il nominale");
    require(cruise.engine_rpm > airframe.propulsion.rated_engine_rpm * 0.85,
            "a piena potenza il regime e' vicino al nominale");
    require(cruise.thrust_n > 300.0, "a piena potenza c'e' spinta");
    require(cruise.propeller_efficiency > 0.5 && cruise.propeller_efficiency < 0.9,
            "il rendimento dell'elica in crociera e' plausibile");

    // Consumo di un centinaio di cavalli al settanta per cento: fra dieci e
    // venti chili l'ora. Fuori da questa forchetta non e' un motore di questa
    // classe, e l'EMS mostrerebbe un'autonomia che non esiste.
    const double hourly_kg = cruise.fuel_flow_kg_s * 3600.0;
    require(hourly_kg > 10.0 && hourly_kg < 22.0,
            "il consumo a piena potenza e' quello di un cento cavalli");

    // Spinta statica: a velocita' nulla l'elica spinge di piu' che in
    // crociera, perche' tutta la potenza va nella scia. E' cio' che permette
    // il decollo.
    Propulsion static_propulsion(airframe.propulsion);
    static_propulsion.reset(true, 15.0);
    for (int step = 0; step < 2000; ++step) {
        static_propulsion.step(controls, air, 0.0, 0.02);
    }
    require(static_propulsion.state().thrust_n > cruise.thrust_n,
            "la spinta statica supera quella in crociera");
}

void test_propulsion_faults() {
    const Airframe airframe = light_sport_airframe();
    const AirState air = Atmosphere{}.at_altitude(0.0);

    Controls controls{};
    controls.throttle = 0.75;
    controls.fuel_pump_on = true;

    auto run = [&](PropulsionFault fault, double seconds) {
        Propulsion propulsion(airframe.propulsion);
        propulsion.reset(true, 15.0);
        propulsion.set_fault(fault);
        const int steps = static_cast<int>(seconds / 0.02);
        for (int step = 0; step < steps; ++step) {
            propulsion.step(controls, air, 100.0 * metres_per_second_per_knot, 0.02);
        }
        return propulsion.state();
    };

    const PropulsionState healthy = run(PropulsionFault::none, 600.0);
    require(healthy.cylinder_head_c[0] > 90.0 && healthy.cylinder_head_c[0] < 125.0,
            "in crociera le teste stanno attorno ai cento gradi");
    require(healthy.exhaust_gas_c[0] > 650.0 && healthy.exhaust_gas_c[0] < 900.0,
            "in crociera lo scarico sta fra 650 e 900 gradi");
    require(healthy.bus_voltage_v > 13.5, "con l'alternatore in servizio la sbarra e' a 14");

    const PropulsionState oil = run(PropulsionFault::oil_loss, 600.0);
    require(oil.oil_pressure_psi < healthy.oil_pressure_psi * 0.5,
            "la perdita d'olio fa cadere la pressione");
    require(oil.oil_temperature_c > healthy.oil_temperature_c + 20.0,
            "la perdita d'olio fa salire la temperatura");

    const PropulsionState alternator = run(PropulsionFault::alternator, 600.0);
    require(alternator.running, "l'alternatore guasto non ferma il motore");
    require(alternator.bus_voltage_v < 12.6, "senza alternatore la sbarra cala");
    require(alternator.alternator_current_a < 0.5, "senza alternatore non c'e' corrente");

    // Il cilindro muto: lo scarico di quel cilindro crolla e gli altri no. E'
    // l'unico guasto che si vede soltanto guardando i quattro scarichi
    // separati, ed e' il motivo per cui l'EMS li mostra separati.
    const PropulsionState cylinder = run(PropulsionFault::single_cylinder, 300.0);
    require(cylinder.exhaust_gas_c[1] < cylinder.exhaust_gas_c[0] - 300.0,
            "il cilindro muto ha lo scarico freddo");
    require(cylinder.exhaust_gas_c[2] > cylinder.exhaust_gas_c[1] + 300.0,
            "gli altri cilindri restano caldi");

    const PropulsionState flameout = run(PropulsionFault::flameout, 60.0);
    require(!flameout.running, "lo spegnimento ferma il motore");
    require_near(flameout.oil_pressure_psi, 0.0, 1.0,
                 "a motore fermo la pressione dell'olio e' zero, non un valore plausibile");
}

// --- Dinamica ----------------------------------------------------------------

// Esegue un volo livellato con il pilota di scenario e restituisce il banco.
// Il passo di controllo e' quello del banco vero, venti millisecondi.
std::shared_ptr<FlightBench> fly_level(const Airframe& airframe, double seconds,
                                       double altitude_ft, double airspeed_kcas,
                                       double heading_deg = 90.0,
                                       SensorFault fault = SensorFault::none,
                                       double fault_at_s = -1.0) {
    Environment environment{};
    environment.field_elevation_m = 18.0;
    auto bench = std::make_shared<FlightBench>(airframe, environment);
    bench->vehicle().place_in_flight(45.65, 12.19, altitude_ft, heading_deg,
                                     airspeed_kcas);
    bench->align_sensors();

    ScenarioPilot pilot(bench->vehicle().airframe());
    PilotTargets targets{};
    targets.altitude_ft = altitude_ft;
    targets.heading_deg = heading_deg;
    targets.airspeed_kcas = airspeed_kcas;

    // Carrello retratto: e' un volo di crociera. Dimenticarlo non da' un
    // errore, da' una crociera piu' lenta di quaranta nodi, che su una
    // turboelica a sedicimila piedi sposta il numero di Mach sotto lo zero
    // virgola tre e fa fallire una prova che riguardava altro.
    targets.gear_down = false;
    pilot.set_targets(targets);

    const double step = 0.02;
    const int steps = static_cast<int>(seconds / step);
    for (int index = 0; index < steps; ++index) {
        if (fault_at_s >= 0.0 &&
            std::abs(static_cast<double>(index) * step - fault_at_s) < step * 0.5) {
            bench->sensors().set_fault(fault);
        }
        pilot.fly(bench->vehicle(), step);
        bench->step_seconds(step);
    }
    return bench;
}

void test_level_flight() {
    const Airframe airframe = light_sport_airframe();
    auto bench = fly_level(airframe, 180.0, 3000.0, 100.0);
    const VehicleState& state = bench->vehicle().state();

    // Centocinquanta piedi in tre minuti. La tolleranza non e' generosa: e'
    // quella che distingue un pilota di scenario che tiene la quota da uno che
    // oscilla, e la prima versione del banco perdeva quattrocento piedi al
    // minuto.
    require_near(state.altitude_m * feet_per_metre, 3000.0, 150.0,
                 "il volo livellato tiene la quota");
    require_near(state.calibrated_airspeed_m_s * knots_per_metre_per_second, 100.0, 5.0,
                 "il volo livellato tiene la velocita'");
    require_near(state.load_factor, 1.0, 0.05,
                 "in volo livellato il fattore di carico e' uno");
    require(!state.on_ground, "in volo livellato l'aeroplano non e' a terra");
    require(!state.stalled, "in volo livellato l'aeroplano non e' stallato");

    // La portanza deve equilibrare il peso: il coefficiente prodotto e quello
    // che il peso richiede devono coincidere. E' il controllo piu' semplice
    // che il modello sia coerente con se stesso.
    const AirState air = bench->vehicle().environment().atmosphere.at_altitude(
        state.altitude_m);
    const double required = state.mass_kg * standard_gravity_m_s2 /
                            (dynamic_pressure_pa(state.true_airspeed_m_s, air) *
                             airframe.geometry.wing_area_m2);
    require_near(state.lift_coefficient, required, 0.03,
                 "la portanza prodotta equilibra il peso");
}

void test_coordinated_turn_matches_core() {
    const Airframe airframe = light_sport_airframe();

    Environment environment{};
    environment.field_elevation_m = 18.0;
    FlightBench bench(airframe, environment);
    bench.vehicle().place_in_flight(45.65, 12.19, 4000.0, 0.0, 100.0);
    bench.align_sensors();

    ScenarioPilot pilot(bench.vehicle().airframe());
    PilotTargets targets{};
    targets.altitude_ft = 4000.0;
    targets.airspeed_kcas = 100.0;

    // Prua obiettivo a novanta gradi: il pilota inclina e la virata si
    // stabilizza. Si misura a meta' virata, quando l'inclinazione e' costante.
    targets.heading_deg = 150.0;
    pilot.set_targets(targets);

    const double step = 0.02;
    double worst_difference = 0.0;
    double worst_lateral = 0.0;
    int samples = 0;
    for (int index = 0; index < 3000; ++index) {
        pilot.fly(bench.vehicle(), step);
        bench.step_seconds(step);

        const VehicleState& state = bench.vehicle().state();

        // Solo virata stabilizzata: dieci secondi dall'inizio della manovra,
        // inclinazione apprezzabile, rollio quasi fermo. I tre vincoli non
        // sono comodita': la relazione del nucleo descrive l'equilibrio di una
        // virata, e l'equilibrio arriva qualche secondo dopo l'inclinazione,
        // perche' il rateo di imbardata si stabilisce quando la derapata ha
        // finito di assestarsi. Misurato durante la rollata, lo stesso modello
        // corretto darebbe uno scarto di piu' di un grado al secondo, e la
        // prova fallirebbe per il motivo sbagliato.
        if (index < 500 ||
            std::abs(state.roll_rad) * radians_to_degrees < 8.0 ||
            std::abs(state.roll_rate_rad_s) * radians_to_degrees > 1.5) {
            continue;
        }

        // Il rateo prodotto dall'integrazione del moto, misurato come lo
        // misura l'inerziale, contro quello che la relazione del nucleo
        // calcola da inclinazione e velocita'. Due vie indipendenti.
        const double modelled = bench.sensors().indicated().turn_rate_deg_s;
        const double from_core = coordinated_turn_rate_deg_s(
            state.roll_rad * radians_to_degrees,
            state.true_airspeed_m_s * knots_per_metre_per_second);
        worst_difference = std::max(worst_difference,
                                    std::abs(modelled - from_core));
        worst_lateral = std::max(worst_lateral, std::abs(state.accel_y_g));
        ++samples;
    }

    require(samples > 100, "la virata si e' stabilizzata per abbastanza campioni");
    require(worst_difference < 0.5,
            "il rateo di virata concorda con la relazione del nucleo");

    // Pallina al centro: in virata coordinata l'accelerazione laterale resta
    // piccola. E' cio' che il pilota di scenario ottiene con il piede, e cio'
    // che l'EFIS deve mostrare.
    require(worst_lateral < 0.06, "la virata resta coordinata");
}

void test_takeoff_and_ground() {
    const Airframe airframe = light_sport_airframe();
    Environment environment{};
    environment.field_elevation_m = 18.0;

    FlightBench bench(airframe, environment);
    bench.vehicle().place_on_ground(45.65, 12.19, 40.0);
    bench.align_sensors();

    // A terra fermo, prima di toccare nulla: non deve esserci ne' stallo ne'
    // incidenza. Un aeroplano al parcheggio con l'allarme di stallo acceso e'
    // cio' che faceva la prima versione, perche' l'incidenza era il rapporto
    // fra due componenti di velocita' entrambe nulle.
    require(bench.vehicle().state().on_ground, "posato a terra e' a terra");
    require(!bench.vehicle().state().stalled, "fermo al parcheggio non e' stallato");
    require_near(bench.vehicle().state().alpha_rad, 0.0, 1.0e-9,
                 "fermo al parcheggio l'incidenza non esiste");

    ScenarioPilot pilot(bench.vehicle().airframe());
    PilotTargets targets{};
    targets.altitude_ft = environment.field_elevation_m * feet_per_metre;
    targets.heading_deg = 40.0;
    targets.airspeed_kcas = 0.0;
    targets.on_runway = true;

    const double step = 0.02;
    bool started = false;
    bool airborne = false;
    double liftoff_kcas = 0.0;

    for (int index = 0; index < 7500; ++index) {  // 150 secondi
        const double time_s = static_cast<double>(index) * step;

        if (time_s > 8.0 && !airborne) {
            targets.on_runway = true;
            targets.airspeed_kcas = airframe.limits.stall_clean_kcas * 1.4;
            targets.altitude_ft = environment.field_elevation_m * feet_per_metre;
        }
        if (bench.vehicle().state().calibrated_airspeed_m_s *
                knots_per_metre_per_second >
            airframe.limits.stall_clean_kcas * 1.10) {
            targets.on_runway = false;
            targets.altitude_ft = environment.field_elevation_m * feet_per_metre +
                                  1500.0;
            targets.airspeed_kcas = airframe.limits.best_climb_kcas;
            targets.max_vertical_speed_fpm = 900.0;
        }
        pilot.set_targets(targets);
        pilot.fly(bench.vehicle(), step);

        Controls controls = bench.vehicle().controls();
        controls.magnetos_on = true;
        controls.fuel_pump_on = true;
        controls.starter = time_s < 8.0;
        controls.brake = time_s < 8.0 ? 1.0 : 0.0;
        bench.vehicle().set_controls(controls);

        bench.step_seconds(step);

        started = started || bench.vehicle().engine().running;
        if (!airborne && !bench.vehicle().state().on_ground) {
            airborne = true;
            liftoff_kcas = bench.vehicle().state().calibrated_airspeed_m_s *
                           knots_per_metre_per_second;
        }
    }

    require(started, "il motore parte");
    require(airborne, "l'aeroplano decolla");
    require(liftoff_kcas > airframe.limits.stall_clean_kcas,
            "il distacco avviene sopra la velocita' di stallo");
    require(liftoff_kcas < airframe.limits.stall_clean_kcas * 1.6,
            "il distacco non avviene a velocita' da crociera");

    const VehicleState& state = bench.vehicle().state();
    require(state.altitude_m * feet_per_metre >
                environment.field_elevation_m * feet_per_metre + 1000.0,
            "la salita porta oltre i mille piedi");
    require(bench.vehicle().state().fuel_kg <
                airframe.mass.fuel_capacity_kg * 0.75,
            "il volo consuma carburante");
}

void test_stall() {
    const Airframe airframe = light_sport_airframe();
    Environment environment{};
    environment.field_elevation_m = 18.0;

    FlightBench bench(airframe, environment);
    bench.vehicle().place_in_flight(45.65, 12.19, 5000.0, 90.0, 100.0);
    bench.align_sensors();

    ScenarioPilot pilot(bench.vehicle().airframe());
    PilotTargets targets{};
    targets.altitude_ft = 5000.0;
    targets.heading_deg = 90.0;
    targets.airspeed_kcas = 100.0;
    targets.stall_entry = true;
    pilot.set_targets(targets);

    const double step = 0.02;
    bool stalled = false;
    double stall_kcas = 0.0;
    double peak_lift = 0.0;

    for (int index = 0; index < 10000 && !stalled; ++index) {
        pilot.fly(bench.vehicle(), step);
        bench.step_seconds(step);
        const VehicleState& state = bench.vehicle().state();
        peak_lift = std::max(peak_lift, state.lift_coefficient);
        if (state.stalled) {
            stalled = true;
            stall_kcas = state.calibrated_airspeed_m_s * knots_per_metre_per_second;
        }
    }

    require(stalled, "la decelerazione a quota costante porta allo stallo");

    // La velocita' a cui stalla e quella dichiarata sul nastro devono stare
    // vicine. Non identiche: lo stallo avviene durante una discesa incipiente,
    // dove il fattore di carico e' poco sotto uno e l'ala sostiene un peso
    // apparente minore. Cinque nodi comprendono quell'effetto e non molto
    // altro.
    require_near(stall_kcas, airframe.limits.stall_clean_kcas, 5.0,
                 "lo stallo avviene alla velocita' dichiarata");
    require_near(peak_lift, max_lift_coefficient(airframe.aero), 0.08,
                 "lo stallo avviene al coefficiente di portanza massimo");
}

// --- Sensori -----------------------------------------------------------------

void test_sensor_noise_supports_adr004() {
    // Il rumore del banco deve stare sotto le risoluzioni dichiarate dallo
    // StasisMonitor, altrimenti un dato sano verrebbe dichiarato fermo, e deve
    // stare sopra lo zero, altrimenti un dato fermo si riconoscerebbe
    // dall'immobilita' del bit meno significativo invece che dal contenuto. Le
    // due disuguaglianze insieme sono la ragione per cui questo banco puo'
    // mettere alla prova ADR-004.
    const Airframe airframe = light_sport_airframe();
    Environment environment{};
    environment.field_elevation_m = 18.0;
    FlightBench bench(airframe, environment);
    bench.vehicle().place_in_flight(45.65, 12.19, 3000.0, 90.0, 100.0);
    bench.align_sensors();

    ScenarioPilot pilot(bench.vehicle().airframe());
    PilotTargets targets{};
    targets.altitude_ft = 3000.0;
    targets.heading_deg = 90.0;
    targets.airspeed_kcas = 100.0;
    pilot.set_targets(targets);

    // Trenta secondi per stabilizzare, poi si misura a dieci hertz come il
    // banco reale, che e' la cadenza su cui le soglie sono state tarate.
    for (int index = 0; index < 1500; ++index) {
        pilot.fly(bench.vehicle(), 0.02);
        bench.step_seconds(0.02);
    }

    std::vector<double> airspeed_steps;
    std::vector<double> altitude_steps;
    double previous_airspeed = bench.sensors().indicated().indicated_airspeed_kt;
    double previous_altitude = bench.sensors().indicated().pressure_altitude_ft;
    for (int index = 0; index < 600; ++index) {
        for (int inner = 0; inner < 5; ++inner) {  // 0,1 secondi
            pilot.fly(bench.vehicle(), 0.02);
            bench.step_seconds(0.02);
        }
        const IndicatedState& indicated = bench.sensors().indicated();
        airspeed_steps.push_back(std::abs(indicated.indicated_airspeed_kt -
                                          previous_airspeed));
        altitude_steps.push_back(std::abs(indicated.pressure_altitude_ft -
                                          previous_altitude));
        previous_airspeed = indicated.indicated_airspeed_kt;
        previous_altitude = indicated.pressure_altitude_ft;
    }

    auto percentile_99 = [](std::vector<double> values) {
        std::sort(values.begin(), values.end());
        const std::size_t index = static_cast<std::size_t>(
            static_cast<double>(values.size()) * 0.99);
        return values[std::min(index, values.size() - 1)];
    };

    const double airspeed_noise = percentile_99(airspeed_steps);
    const double altitude_noise = percentile_99(altitude_steps);

    // Le soglie sono quelle di FlightModelPolicy: un nodo e cinque piedi.
    require(airspeed_noise < 1.0,
            "il rumore della velocita' sta sotto la risoluzione dichiarata");
    require(altitude_noise < 5.0,
            "il rumore della quota sta sotto la risoluzione dichiarata");
    require(airspeed_noise > 0.0, "la velocita' indicata non e' priva di rumore");
    require(altitude_noise > 0.0, "la quota indicata non e' priva di rumore");
}

void test_pitot_blocked_in_climb() {
    // Il Pitot ostruito in salita: la velocita' indicata cresce mentre quella
    // vera cala. E' il guasto che si presenta come un anemometro in perfetta
    // salute, e il solo modo di accorgersene senza un secondo sensore e'
    // confrontarlo con qualcosa d'altro.
    const Airframe airframe = light_sport_airframe();
    Environment environment{};
    environment.field_elevation_m = 18.0;
    FlightBench bench(airframe, environment);
    bench.vehicle().place_in_flight(45.65, 12.19, 2000.0, 90.0, 90.0);
    bench.align_sensors();

    ScenarioPilot pilot(bench.vehicle().airframe());
    PilotTargets targets{};
    targets.altitude_ft = 2000.0;
    targets.heading_deg = 90.0;
    targets.airspeed_kcas = 90.0;
    pilot.set_targets(targets);
    for (int index = 0; index < 500; ++index) {
        pilot.fly(bench.vehicle(), 0.02);
        bench.step_seconds(0.02);
    }

    bench.sensors().set_fault(SensorFault::pitot_blocked);
    const double indicated_before = bench.sensors().indicated().indicated_airspeed_kt;

    targets.altitude_ft = 6000.0;
    targets.max_vertical_speed_fpm = 800.0;
    pilot.set_targets(targets);
    for (int index = 0; index < 15000; ++index) {
        pilot.fly(bench.vehicle(), 0.02);
        bench.step_seconds(0.02);
    }

    const VehicleState& truth = bench.vehicle().state();
    const IndicatedState& indicated = bench.sensors().indicated();
    require(truth.altitude_m * feet_per_metre > 3500.0,
            "la salita con Pitot ostruito avviene comunque");
    require(indicated.indicated_airspeed_kt > indicated_before + 5.0,
            "con il Pitot ostruito la velocita' indicata cresce in salita");
    require(indicated.indicated_airspeed_kt >
                truth.calibrated_airspeed_m_s * knots_per_metre_per_second + 5.0,
            "la velocita' indicata si allontana da quella vera");
}

void test_static_blocked_and_latched_ahrs() {
    const Airframe airframe = light_sport_airframe();

    // Statica ostruita: la quota si ferma mentre l'aeroplano sale. Il valore
    // continua ad arrivare fresco e valido, e questo e' il caso principale di
    // ADR-004.
    {
        Environment environment{};
        environment.field_elevation_m = 18.0;
        FlightBench bench(airframe, environment);
        bench.vehicle().place_in_flight(45.65, 12.19, 2000.0, 90.0, 90.0);
        bench.align_sensors();

        ScenarioPilot pilot(bench.vehicle().airframe());
        PilotTargets targets{};
        targets.altitude_ft = 2000.0;
        targets.heading_deg = 90.0;
        targets.airspeed_kcas = 90.0;
        pilot.set_targets(targets);
        for (int index = 0; index < 500; ++index) {
            pilot.fly(bench.vehicle(), 0.02);
            bench.step_seconds(0.02);
        }

        bench.sensors().set_fault(SensorFault::static_blocked);
        const double frozen = bench.sensors().indicated().pressure_altitude_ft;

        targets.altitude_ft = 5000.0;
        pilot.set_targets(targets);
        double maximum_deviation = 0.0;
        bool moved = false;
        for (int index = 0; index < 15000; ++index) {
            pilot.fly(bench.vehicle(), 0.02);
            bench.step_seconds(0.02);
            maximum_deviation = std::max(
                maximum_deviation,
                std::abs(bench.sensors().indicated().pressure_altitude_ft - frozen));
            moved = moved || bench.sensors().indicated().pressure_altitude_ft != frozen;
        }
        require(bench.vehicle().state().altitude_m * feet_per_metre > 3000.0,
                "con la statica ostruita l'aeroplano sale davvero");
        require(maximum_deviation < 5.0,
                "con la statica ostruita la quota indicata resta ferma");
        require(moved,
                "la quota ferma continua a ballare: il guasto e' nel contenuto, non nel flusso");
    }

    // Inerziale latchato: assetto fermo mentre l'aeroplano vira. Il rumore
    // continua, che e' la ragione per cui lo StasisMonitor deve confrontare
    // sulla risoluzione e non sull'uguaglianza.
    {
        Environment environment{};
        environment.field_elevation_m = 18.0;
        FlightBench bench(airframe, environment);
        bench.vehicle().place_in_flight(45.65, 12.19, 3000.0, 0.0, 100.0);
        bench.align_sensors();

        ScenarioPilot pilot(bench.vehicle().airframe());
        PilotTargets targets{};
        targets.altitude_ft = 3000.0;
        targets.heading_deg = 0.0;
        targets.airspeed_kcas = 100.0;
        pilot.set_targets(targets);
        for (int index = 0; index < 500; ++index) {
            pilot.fly(bench.vehicle(), 0.02);
            bench.step_seconds(0.02);
        }

        bench.sensors().set_fault(SensorFault::ahrs_latched);
        const double latched_roll = bench.sensors().indicated().roll_deg;

        targets.heading_deg = 180.0;
        pilot.set_targets(targets);
        double worst_indicated = 0.0;
        double worst_truth = 0.0;
        bool roll_moved = false;
        double previous = bench.sensors().indicated().roll_deg;
        for (int index = 0; index < 4000; ++index) {
            pilot.fly(bench.vehicle(), 0.02);
            bench.step_seconds(0.02);
            const double indicated_roll = bench.sensors().indicated().roll_deg;
            worst_indicated = std::max(worst_indicated,
                                       std::abs(indicated_roll - latched_roll));
            worst_truth = std::max(worst_truth,
                                   std::abs(bench.vehicle().state().roll_rad *
                                            radians_to_degrees));
            roll_moved = roll_moved || indicated_roll != previous;
            previous = indicated_roll;
        }
        require(worst_truth > 8.0, "l'aeroplano inclina davvero");
        require(worst_indicated < 0.5,
                "l'inerziale latchato continua a dichiarare l'assetto di prima");
        require(roll_moved, "il valore latchato conserva il proprio rumore");
    }
}

void test_gnss_outage_is_declared_absent() {
    const Airframe airframe = light_sport_airframe();
    auto bench = fly_level(airframe, 40.0, 3000.0, 100.0, 90.0,
                           SensorFault::gnss_outage, 20.0);

    const IndicatedState& indicated = bench->sensors().indicated();
    require(!indicated.gnss_valid, "la perdita del satellitare e' dichiarata");
    require(indicated.satellites == 0, "senza fix non ci sono satelliti");

    // E nel modello dati dell'EFIS il campione deve risultare indisponibile,
    // non plausibile. Uno zero sarebbe peggio del guasto: zero e zero sono le
    // coordinate di un punto nel golfo di Guinea.
    const FlightSnapshot snapshot = bench->flight_snapshot(MonoClock::now());
    require(snapshot.position.latitude_deg.validity == Validity::unavailable,
            "la latitudine senza fix e' indisponibile");
    require(snapshot.position.longitude_deg.validity == Validity::unavailable,
            "la longitudine senza fix e' indisponibile");
    require(snapshot.position.ground_speed_kt.validity == Validity::unavailable,
            "la velocita' al suolo senza fix e' indisponibile");
    require(!snapshot.position.latitude_deg.usable(),
            "un campione indisponibile non e' utilizzabile");

    // L'assetto e i dati aria non dipendono dal satellitare e devono restare
    // buoni: un guasto che spegnesse tutto sarebbe inutile come prova.
    require(snapshot.attitude.roll_deg.usable(),
            "la perdita del satellitare non tocca l'assetto");
    require(snapshot.air_data.indicated_airspeed_kt.usable(),
            "la perdita del satellitare non tocca i dati aria");
}

// --- Ponte verso l'EFIS ------------------------------------------------------

void test_every_sample_is_marked_simulated() {
    const Airframe airframe = light_sport_airframe();
    auto bench = fly_level(airframe, 20.0, 3000.0, 100.0);

    const MonoTime now = MonoClock::now();
    const FlightSnapshot flight = bench->flight_snapshot(now);
    const engine::EngineSnapshot engine_state = bench->engine_snapshot(now);

    std::vector<const Sample<double>*> samples{
        &flight.attitude.roll_deg, &flight.attitude.pitch_deg,
        &flight.attitude.heading_deg, &flight.attitude.turn_rate_deg_s,
        &flight.attitude.lateral_acceleration_g,
        &flight.air_data.indicated_airspeed_kt, &flight.air_data.pressure_altitude_ft,
        &flight.air_data.vertical_speed_fpm, &flight.air_data.baro_setting_hpa,
        &flight.air_data.indicated_altitude_ft, &flight.position.latitude_deg,
        &flight.position.longitude_deg, &flight.position.ground_speed_kt,
        &flight.position.gnss_vertical_speed_fpm, &engine_state.rpm,
        &engine_state.manifold_pressure_inhg, &engine_state.oil_pressure_psi,
        &engine_state.oil_temperature_c, &engine_state.fuel_pressure_psi,
        &engine_state.fuel_flow_gph, &engine_state.fuel_left_gal,
        &engine_state.fuel_right_gal, &engine_state.bus_voltage_v,
        &engine_state.alternator_current_a};
    for (const Sample<double>& cylinder : engine_state.cht_c) {
        samples.push_back(&cylinder);
    }
    for (const Sample<double>& exhaust : engine_state.egt_c) {
        samples.push_back(&exhaust);
    }

    // Il vincolo del paragrafo 19: un valore simulato non puo' presentarsi
    // come reale, in nessuna circostanza e qualunque validita' porti. Questo
    // motore lo rende piu' importante di prima, non meno, perche' i suoi numeri
    // sono plausibili in un modo che un copione non riesce a essere.
    for (const Sample<double>* sample : samples) {
        require(sample->is_simulated(), "ogni campione del banco e' marcato simulato");
        require(sample->origin.simulated, "il marcatore esplicito e' presente");
        require(sample->origin.unit != Unit::unknown, "ogni campione dichiara l'unita'");
        require(sample->origin.signal != SignalId::unknown,
                "ogni campione dichiara il segnale");
    }
    require(samples.size() == 32, "il conteggio dei canali verificati e' quello atteso");

    // La regolazione barometrica non e' una misura: la provenienza deve dirlo.
    require(flight.air_data.baro_setting_hpa.source == SourceId::pilot_setting,
            "la regolazione barometrica viene dal pilota, non da un sensore");

    // Guida e navigazione non esistono in questo motore: devono restare
    // indisponibili invece di portare valori inventati.
    require(!flight.guidance.selected_altitude_ft.usable(),
            "la guida di volo resta indisponibile");
    require(!flight.navigation.cdi_deviation_dots.usable(),
            "la navigazione resta indisponibile");
}

void test_sim_var_bus() {
    const Airframe lsa = light_sport_airframe();
    auto bench = fly_level(lsa, 20.0, 3000.0, 100.0);

    std::vector<SimVarSample> bus;
    bench->publish_bus(bus);
    require(bus.size() > 50, "il bus pubblica l'insieme completo");

    // Nomi unici: un bus con due variabili dello stesso nome e' un bus in cui
    // la ricerca per nome non significa niente.
    std::vector<std::string> names;
    names.reserve(bus.size());
    for (const SimVarSample& sample : bus) {
        names.emplace_back(sample.name);
    }
    std::sort(names.begin(), names.end());
    require(std::adjacent_find(names.begin(), names.end()) == names.end(),
            "i nomi del bus sono unici");

    const auto airspeed = find_sim_var(bus, "AIRSPEED INDICATED");
    require(airspeed.has_value(), "la velocita' indicata e' sul bus");
    require_near(*airspeed, bench->sensors().indicated().indicated_airspeed_kt, 0.001,
                 "il bus porta il valore misurato, non quello vero");

    const auto truth_speed = find_sim_var(bus, "BENCH TRUTH AIRSPEED CALIBRATED");
    require(truth_speed.has_value(), "la verita' del banco e' pubblicata a parte");
    require(std::abs(*truth_speed - *airspeed) < 10.0,
            "verita' e misura restano vicine in assenza di guasto");

    // Su un motore a pistoni le grandezze di turbina non esistono: devono
    // essere assenti, non nulle. Uno zero sarebbe un valore, e un quadro che
    // non distinguesse i due casi mostrerebbe una turbina spenta.
    require(!find_sim_var(bus, "TURB ENG ITT:1").has_value(),
            "la turbina non esiste su un motore a pistoni");
    require(find_sim_var(bus, "RECIP ENG CYLINDER HEAD TEMPERATURE:1").has_value(),
            "le teste esistono su un motore a pistoni");

    // E sulla turboelica il contrario.
    auto turboprop_bench = fly_level(professional_airframe(), 20.0, 16000.0, 200.0);
    std::vector<SimVarSample> turbine_bus;
    turboprop_bench->publish_bus(turbine_bus);
    require(find_sim_var(turbine_bus, "TURB ENG ITT:1").has_value(),
            "la turbina esiste sulla turboelica");
    require(!find_sim_var(turbine_bus, "RECIP ENG CYLINDER HEAD TEMPERATURE:1")
                 .has_value(),
            "le teste non esistono sulla turboelica");

    // In quota la velocita' vera supera di molto quella indicata: e' la
    // ragione principale per cui la cellula professionale esiste nel banco.
    const auto indicated_high = find_sim_var(turbine_bus, "AIRSPEED INDICATED");
    const auto true_high = find_sim_var(turbine_bus, "AIRSPEED TRUE");
    require(indicated_high.has_value() && true_high.has_value(),
            "le due velocita' sono sul bus");
    require(*true_high > *indicated_high + 40.0,
            "a sedicimila piedi la velocita' vera supera di molto l'indicata");

    const auto mach = find_sim_var(turbine_bus, "AIRSPEED MACH");
    require(mach.has_value() && *mach > 0.3 && *mach < 0.6,
            "il numero di Mach della turboelica in crociera e' plausibile");

    // Senza fix la posizione non e' sul bus: la ricerca per nome deve
    // restituire assenza, perche' e' il solo modo in cui un consumatore puo'
    // distinguere il dato mancante da un dato a zero.
    auto lost_bench = fly_level(lsa, 40.0, 3000.0, 100.0, 90.0,
                                SensorFault::gnss_outage, 20.0);
    std::vector<SimVarSample> lost_bus;
    lost_bench->publish_bus(lost_bus);
    require(!find_sim_var(lost_bus, "PLANE LATITUDE").has_value(),
            "senza fix la latitudine non e' disponibile sul bus");
    require(find_sim_var(lost_bus, "AIRSPEED INDICATED").has_value(),
            "senza fix i dati aria restano disponibili");
}

void test_professional_airframe_flies() {
    // La cellula professionale in quota: il banco deve poterla portare dove i
    // problemi che la riguardano esistono, cioe' dove calibrata e vera
    // divergono e dove il Mach conta.
    const Airframe airframe = professional_airframe();
    auto bench = fly_level(airframe, 180.0, 16000.0, 200.0);
    const VehicleState& state = bench->vehicle().state();

    require_near(state.altitude_m * feet_per_metre, 16000.0, 250.0,
                 "la turboelica tiene la quota di crociera");
    require(state.mach > 0.3 && state.mach < 0.6,
            "il numero di Mach in crociera e' plausibile");
    require(state.true_airspeed_m_s > state.calibrated_airspeed_m_s * 1.2,
            "in quota la velocita' vera supera di molto la calibrata");
    require(bench->vehicle().engine().interstage_turbine_c > 300.0,
            "la turbina ha una temperatura fra turbine");
    require(bench->vehicle().engine().gas_generator_pct > 50.0,
            "il generatore di gas gira");
    require_near(bench->vehicle().engine().propeller_rpm,
                 airframe.propulsion.rated_engine_rpm, 50.0,
                 "il regolatore tiene il regime dell'elica");
}

}  // namespace

int main() {
    test_atmosphere();
    test_air_data_relations();
    test_airframe_declared_speeds();
    test_lift_curve();
    test_propulsion_start_and_power();
    test_propulsion_faults();
    test_level_flight();
    test_coordinated_turn_matches_core();
    test_takeoff_and_ground();
    test_stall();
    test_sensor_noise_supports_adr004();
    test_pitot_blocked_in_climb();
    test_static_blocked_and_latched_ahrs();
    test_gnss_outage_is_declared_absent();
    test_every_sample_is_marked_simulated();
    test_sim_var_bus();
    test_professional_airframe_flies();

    if (failures != 0) {
        std::cerr << failures << " prove fallite\n";
        return EXIT_FAILURE;
    }
    std::cout << "fdm_tests: tutte le prove superate\n";
    return EXIT_SUCCESS;
}
