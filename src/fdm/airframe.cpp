#include "efis/fdm/airframe.hpp"

#include "efis/fdm/atmosphere.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace efis::fdm {
namespace {

constexpr double degrees_to_radians = std::numbers::pi / 180.0;

// Ampiezza del raccordo oltre lo stallo.
//
// Dieci gradi: oltre l'incidenza di stallo la portanza non crolla di colpo, e
// nemmeno resta appesa al massimo. In dieci gradi un'ala passa dal proprio
// massimo al comportamento di una lastra piana, ed e' la discesa in quei dieci
// gradi che un allarme di stallo deve vedere arrivare.
constexpr double stall_blend_rad = 10.0 * degrees_to_radians;

// Incidenza di portanza nulla. Non e' un parametro dichiarato perche' non e'
// indipendente: discende dal termine costante e dalla pendenza, e dichiararla
// a parte darebbe due numeri capaci di contraddirsi.
[[nodiscard]] double zero_lift_alpha_rad(const AeroCoefficients& aero) noexcept {
    return -aero.lift_zero / aero.lift_alpha;
}

// Portanza e resistenza di una lastra piana all'incidenza data. E' cio' che
// un'ala diventa quando il flusso si e' separato: la relazione non contiene
// nulla dell'ala, e proprio per questo descrive bene cio' che resta dopo lo
// stallo.
[[nodiscard]] double flat_plate_lift(double alpha_rad) noexcept {
    const double s = std::sin(alpha_rad);
    return 2.0 * s * std::abs(s) * std::cos(alpha_rad);
}

[[nodiscard]] double flat_plate_drag(double alpha_rad) noexcept {
    const double s = std::sin(alpha_rad);
    return 2.0 * s * s;
}

}  // namespace

double lift_coefficient(const AeroCoefficients& aero, double alpha_rad) noexcept {
    const double alpha_zero_lift = zero_lift_alpha_rad(aero);

    // Incidenza misurata dalla portanza nulla, non dall'asse corpo. Lo stallo
    // e' simmetrico rispetto a quella, non rispetto allo zero geometrico: un
    // profilo con curvatura stalla a incidenza negativa piu' in giu' di quanto
    // stalli in su', e la differenza e' esattamente l'incidenza di portanza
    // nulla.
    const double alpha_effective = alpha_rad - alpha_zero_lift;
    const double stall_effective = aero.stall_alpha_deg * degrees_to_radians -
                                   alpha_zero_lift;

    const double linear = aero.lift_alpha * alpha_effective;
    const double excess = std::abs(alpha_effective) - stall_effective;
    if (excess <= 0.0) {
        return linear;
    }

    // Oltre lo stallo: miscela fra la retta e la lastra piana. Il massimo
    // resta quello della retta all'incidenza di stallo, quindi il coefficiente
    // massimo del modello e' lo stesso che compare nel calcolo della velocita'
    // di stallo. Nessun raccordo che sposti il massimo: la velocita' dichiarata
    // sul nastro deve essere quella a cui l'aeroplano stalla davvero.
    const double blend = std::min(excess / stall_blend_rad, 1.0);
    const double peak = std::copysign(aero.lift_alpha * stall_effective,
                                      alpha_effective);
    return (1.0 - blend) * peak + blend * flat_plate_lift(alpha_rad);
}

double max_lift_coefficient(const AeroCoefficients& aero) noexcept {
    return aero.lift_zero + aero.lift_alpha * aero.stall_alpha_deg * degrees_to_radians;
}

FlapEffect flap_effect(const Airframe& airframe, double flap_deg) noexcept {
    FlapEffect effect{};
    if (airframe.flaps.empty()) {
        effect.limit_kcas = airframe.limits.normal_operating_kcas;
        return effect;
    }

    const double requested = std::clamp(flap_deg, airframe.flaps.front().angle_deg,
                                        airframe.flaps.back().angle_deg);
    effect.limit_kcas = airframe.flaps.front().limit_kcas;

    for (std::size_t index = 1; index < airframe.flaps.size(); ++index) {
        const FlapDetent& lower = airframe.flaps[index - 1];
        const FlapDetent& upper = airframe.flaps[index];
        if (requested > upper.angle_deg) {
            continue;
        }
        const double span = upper.angle_deg - lower.angle_deg;
        const double fraction = span > 0.0 ? (requested - lower.angle_deg) / span : 0.0;
        effect.delta_lift = lower.delta_lift +
                            fraction * (upper.delta_lift - lower.delta_lift);
        effect.delta_drag = lower.delta_drag +
                            fraction * (upper.delta_drag - lower.delta_drag);
        effect.delta_pitch = lower.delta_pitch +
                             fraction * (upper.delta_pitch - lower.delta_pitch);

        // Il limite di velocita' e' quello della tacca raggiunta, non
        // interpolato: una tacca a meta' corsa deve rispettare il limite della
        // posizione piu' estesa fra le due, perche' il flap e' meccanicamente
        // esposto a quel carico mentre si muove.
        effect.limit_kcas = fraction > 0.0 ? upper.limit_kcas : lower.limit_kcas;
        return effect;
    }

    const FlapDetent& last = airframe.flaps.back();
    effect.delta_lift = last.delta_lift;
    effect.delta_drag = last.delta_drag;
    effect.delta_pitch = last.delta_pitch;
    effect.limit_kcas = last.limit_kcas;
    return effect;
}

AeroLoads aerodynamic_loads(const Airframe& airframe,
                            const AeroState& state,
                            const Controls& controls) noexcept {
    AeroLoads loads{};

    const AeroCoefficients& aero = airframe.aero;
    const Geometry& geometry = airframe.geometry;
    const FlapEffect flap = flap_effect(airframe, controls.flap_deg);

    // Il trim non e' una superficie a parte: sposta la posizione neutra
    // dell'equilibratore. Sommarlo qui, e non duplicare la derivata, impedisce
    // che i due comandi abbiano efficacia diversa a parita' di escursione.
    const double elevator = std::clamp(controls.elevator + controls.elevator_trim,
                                       -1.0, 1.0);

    const double alpha = state.alpha_rad;
    const double stall_alpha = aero.stall_alpha_deg * degrees_to_radians;
    loads.stalled = alpha > stall_alpha;

    // Rateo adimensionale: la convenzione e' corda media per il beccheggio e
    // semiapertura per rollio e imbardata. Sotto una velocita' minima il
    // rapporto diverge, e a velocita' nulla non esiste smorzamento
    // aerodinamico perche' non esiste flusso: il denominatore e' protetto.
    const double airspeed = std::max(state.true_airspeed_m_s, 1.0);
    const double pitch_rate_hat = state.pitch_rate_rad_s * geometry.mean_chord_m /
                                  (2.0 * airspeed);
    const double roll_rate_hat = state.roll_rate_rad_s * geometry.wing_span_m /
                                 (2.0 * airspeed);
    const double yaw_rate_hat = state.yaw_rate_rad_s * geometry.wing_span_m /
                                (2.0 * airspeed);

    double lift = lift_coefficient(aero, alpha) + flap.delta_lift +
                  aero.lift_elevator * elevator + aero.lift_pitch_rate * pitch_rate_hat;

    const double aspect_ratio = geometry.aspect_ratio();
    const double induced_factor = 1.0 / (std::numbers::pi * aero.oswald_efficiency *
                                         aspect_ratio);
    double drag = aero.drag_zero + flap.delta_drag + induced_factor * lift * lift;

    // Resistenza del carrello esteso, solo se il carrello e' retrattile. Su una
    // cellula a carrello fisso la resistenza e' gia' dentro il termine
    // parassita, e aggiungerla di nuovo la conterebbe due volte.
    if (airframe.gear.retractable && controls.gear_down) {
        drag += airframe.gear.delta_drag_extended;
    }

    // Oltre lo stallo la resistenza cresce molto piu' di quanto preveda il
    // termine indotto, perche' il flusso separato non produce piu' portanza ma
    // continua a produrre scia. Senza questo termine un aeroplano stallato
    // accelererebbe.
    const double stall_excess = alpha - stall_alpha;
    if (stall_excess > 0.0) {
        const double blend = std::min(stall_excess / stall_blend_rad, 1.0);
        drag += blend * flat_plate_drag(alpha);
    }

    const double sideslip = state.sideslip_rad;
    const double side = aero.side_sideslip * sideslip + aero.side_rudder * controls.rudder;

    const double roll = aero.roll_sideslip * sideslip + aero.roll_rate * roll_rate_hat +
                        aero.roll_yaw_rate * yaw_rate_hat +
                        aero.roll_aileron * controls.aileron +
                        aero.roll_rudder * controls.rudder;

    const double pitch = aero.pitch_zero + flap.delta_pitch +
                         aero.pitch_alpha * (alpha - zero_lift_alpha_rad(aero)) +
                         aero.pitch_rate * pitch_rate_hat +
                         aero.pitch_elevator * elevator;

    const double yaw = aero.yaw_sideslip * sideslip + aero.yaw_roll_rate * roll_rate_hat +
                       aero.yaw_rate * yaw_rate_hat +
                       aero.yaw_aileron * controls.aileron +
                       aero.yaw_rudder * controls.rudder;

    const double scale = state.dynamic_pressure_pa * geometry.wing_area_m2;

    // Dalle assi vento alle assi corpo. Portanza e resistenza sono definite
    // rispetto al flusso, non rispetto all'aeroplano, e la rotazione di
    // incidenza fra i due sistemi e' cio' che fa salire un aeroplano cabrato
    // senza che la spinta sia aumentata.
    loads.force_x_n = scale * (lift * std::sin(alpha) - drag * std::cos(alpha));
    loads.force_z_n = scale * (-lift * std::cos(alpha) - drag * std::sin(alpha));
    loads.force_y_n = scale * side;
    loads.moment_roll_nm = scale * geometry.wing_span_m * roll;
    loads.moment_pitch_nm = scale * geometry.mean_chord_m * pitch;
    loads.moment_yaw_nm = scale * geometry.wing_span_m * yaw;
    loads.lift_coefficient = lift;
    loads.drag_coefficient = drag;
    return loads;
}

double stall_speed_kcas(const Airframe& airframe,
                        double mass_kg,
                        double flap_deg) noexcept {
    const double max_lift = max_lift_coefficient(airframe.aero) +
                            flap_effect(airframe, flap_deg).delta_lift;
    if (max_lift <= 0.0) {
        return 0.0;
    }

    // Al livello del mare standard la velocita' vera coincide con la calibrata,
    // e la velocita' di stallo si dichiara in calibrata proprio per questo: non
    // dipende dalla quota. In quota l'aeroplano stalla alla stessa velocita'
    // calibrata e a una velocita' vera maggiore, che e' il motivo per cui il
    // nastro dell'EFIS mostra la calibrata.
    const double speed_m_s = std::sqrt(2.0 * mass_kg * standard_gravity_m_s2 /
                                       (isa_sea_level_density_kg_m3 *
                                        airframe.geometry.wing_area_m2 * max_lift));
    return speed_m_s * knots_per_metre_per_second;
}

const char* category_name(Category category) noexcept {
    switch (category) {
        case Category::light_sport: return "LSA";
        case Category::professional: return "PROFESSIONAL";
    }
    return "UNKNOWN";
}

Airframe light_sport_airframe() {
    Airframe airframe{};
    airframe.name = "LSA 600 kg, motore a pistoni 100 hp";
    airframe.category = Category::light_sport;

    airframe.geometry.wing_area_m2 = 9.9;
    airframe.geometry.wing_span_m = 8.4;
    airframe.geometry.mean_chord_m = 1.21;

    airframe.mass.empty_mass_kg = 330.0;
    airframe.mass.max_mass_kg = 600.0;
    airframe.mass.fuel_capacity_kg = 70.0;
    airframe.mass.inertia_xx_kg_m2 = 950.0;
    airframe.mass.inertia_yy_kg_m2 = 760.0;
    airframe.mass.inertia_zz_kg_m2 = 1320.0;

    // L'incidenza di stallo e' il parametro con cui si tara la velocita' di
    // stallo, perche' il coefficiente massimo ne discende. A 13,5 gradi il
    // massimo vale 1,47, che alla massa massima da' 50 nodi puliti e 42 con
    // flap estesi: i due numeri che la definizione di LSA impone di rispettare.
    airframe.aero.lift_zero = 0.25;
    airframe.aero.lift_alpha = 5.2;
    airframe.aero.stall_alpha_deg = 13.5;
    airframe.aero.drag_zero = 0.028;
    airframe.aero.oswald_efficiency = 0.78;

    airframe.flaps = {
        FlapDetent{0.0, 0.0, 0.0, 0.0, 120.0},
        FlapDetent{15.0, 0.35, 0.012, -0.020, 70.0},
        FlapDetent{35.0, 0.65, 0.055, -0.045, 60.0},
    };

    airframe.limits.stall_clean_kcas = 50.0;
    airframe.limits.stall_landing_kcas = 42.0;
    airframe.limits.manoeuvring_kcas = 95.0;
    airframe.limits.normal_operating_kcas = 120.0;
    airframe.limits.never_exceed_kcas = 150.0;
    airframe.limits.best_climb_kcas = 65.0;
    airframe.limits.best_glide_kcas = 70.0;
    airframe.limits.max_mach = 0.0;

    airframe.propulsion.kind = EngineKind::piston_prop;
    airframe.propulsion.rated_power_kw = 73.5;
    airframe.propulsion.rated_engine_rpm = 5500.0;
    airframe.propulsion.gear_ratio = 2.43;
    airframe.propulsion.propeller_diameter_m = 1.72;
    airframe.propulsion.propeller_peak_efficiency = 0.82;
    airframe.propulsion.propeller_design_advance_ratio = 0.80;
    airframe.propulsion.bsfc_kg_kwh = 0.28;
    airframe.propulsion.rotating_inertia_kg_m2 = 0.6;
    airframe.propulsion.idle_rpm = 1400.0;

    // Carrello fisso: la sua resistenza sta nel termine parassita, quindi
    // l'incremento e' nullo e non c'e' nulla da retrarre.
    airframe.gear.retractable = false;
    airframe.gear.delta_drag_extended = 0.0;
    airframe.gear.nose_x_m = 1.30;
    airframe.gear.main_x_m = -0.35;
    airframe.gear.main_y_m = 0.90;
    airframe.gear.contact_z_m = 0.95;
    airframe.gear.spring_n_m = 30000.0;
    airframe.gear.damper_n_s_m = 3500.0;

    airframe.thrust_incidence_deg = 2.0;
    return airframe;
}

Airframe professional_airframe() {
    Airframe airframe{};
    airframe.name = "Turboelica pressurizzata 4700 kg";
    airframe.category = Category::professional;

    airframe.geometry.wing_area_m2 = 25.8;
    airframe.geometry.wing_span_m = 16.2;
    airframe.geometry.mean_chord_m = 1.70;

    airframe.mass.empty_mass_kg = 3100.0;
    airframe.mass.max_mass_kg = 4740.0;
    airframe.mass.fuel_capacity_kg = 1200.0;
    airframe.mass.inertia_xx_kg_m2 = 28000.0;
    airframe.mass.inertia_yy_kg_m2 = 30000.0;
    airframe.mass.inertia_zz_kg_m2 = 45000.0;

    airframe.aero.lift_zero = 0.22;
    airframe.aero.lift_alpha = 5.5;
    airframe.aero.stall_alpha_deg = 14.0;
    airframe.aero.drag_zero = 0.025;
    airframe.aero.oswald_efficiency = 0.82;
    airframe.aero.pitch_alpha = -0.95;
    airframe.aero.pitch_rate = -12.0;
    airframe.aero.pitch_elevator = 0.62;
    airframe.aero.roll_rate = -0.52;
    airframe.aero.yaw_sideslip = 0.085;
    airframe.aero.yaw_rate = -0.125;

    airframe.flaps = {
        FlapDetent{0.0, 0.0, 0.0, 0.0, 240.0},
        FlapDetent{15.0, 0.40, 0.020, -0.030, 165.0},
        FlapDetent{30.0, 0.70, 0.060, -0.060, 130.0},
        FlapDetent{40.0, 0.90, 0.110, -0.080, 120.0},
    };

    airframe.limits.stall_clean_kcas = 84.0;
    airframe.limits.stall_landing_kcas = 67.0;
    airframe.limits.manoeuvring_kcas = 150.0;
    airframe.limits.normal_operating_kcas = 236.0;
    airframe.limits.never_exceed_kcas = 240.0;
    airframe.limits.best_climb_kcas = 120.0;
    airframe.limits.best_glide_kcas = 125.0;

    // Limite di Mach reale: a 30.000 piedi 236 nodi calibrati valgono circa
    // Mach 0,48, quindi da quella quota in su e' il Mach a comandare il nastro.
    // E' la ragione principale per cui la cellula professionale esiste nel
    // banco: sotto i 10.000 piedi quel limite non si incontra mai.
    airframe.limits.max_mach = 0.48;

    airframe.propulsion.kind = EngineKind::turboprop;
    airframe.propulsion.rated_power_kw = 895.0;
    airframe.propulsion.rated_engine_rpm = 1700.0;
    airframe.propulsion.gear_ratio = 1.0;
    airframe.propulsion.propeller_diameter_m = 2.67;
    airframe.propulsion.propeller_peak_efficiency = 0.85;
    airframe.propulsion.propeller_design_advance_ratio = 1.40;
    airframe.propulsion.bsfc_kg_kwh = 0.36;
    airframe.propulsion.rotating_inertia_kg_m2 = 14.0;
    airframe.propulsion.idle_rpm = 1000.0;

    airframe.gear.retractable = true;
    airframe.gear.delta_drag_extended = 0.025;
    airframe.gear.nose_x_m = 3.80;
    airframe.gear.main_x_m = -0.90;
    airframe.gear.main_y_m = 2.10;
    airframe.gear.contact_z_m = 1.60;
    airframe.gear.spring_n_m = 400000.0;
    airframe.gear.damper_n_s_m = 40000.0;

    airframe.thrust_incidence_deg = 1.5;
    return airframe;
}

}  // namespace efis::fdm
