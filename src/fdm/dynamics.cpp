#include "efis/fdm/dynamics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <utility>

namespace efis::fdm {
namespace {

constexpr double degrees_to_radians = std::numbers::pi / 180.0;
constexpr double radians_to_degrees = 180.0 / std::numbers::pi;

// Raggio terrestre medio. La terra del banco e' sferica: a scala di volo locale
// la differenza con l'ellissoide e' sotto il metro di posizione, e il banco non
// serve a fare geodesia.
constexpr double earth_radius_m = 6371000.0;

// Passo massimo di integrazione.
//
// Un centesimo di secondo: la dinamica piu' rapida del modello e' lo
// smorzamento di rollio, con costante di tempo intorno al decimo di secondo
// sull'LSA, e dieci punti per costante di tempo sono il minimo per un
// integratore di Euler. Il passo di pubblicazione del display, cinquanta
// millisecondi, non basterebbe.
constexpr double max_substep_s = 0.01;

// Massa del carico pagante usata quando il banco non la specifica: due persone
// e un bagaglio leggero. Dichiararla serve perche' la massa decide la velocita'
// di stallo, e un banco che volasse sempre al peso a vuoto proverebbe il nastro
// nella sola condizione in cui l'aeroplano non vola mai.
constexpr double default_payload_kg = 170.0;

struct RotationMatrix final {
    // Righe: nord, est, basso. Colonne: assi corpo x, y, z.
    std::array<std::array<double, 3>, 3> row{};
};

[[nodiscard]] RotationMatrix body_to_ned(double roll, double pitch, double yaw) noexcept {
    const double sr = std::sin(roll);
    const double cr = std::cos(roll);
    const double sp = std::sin(pitch);
    const double cp = std::cos(pitch);
    const double sy = std::sin(yaw);
    const double cy = std::cos(yaw);

    RotationMatrix matrix{};
    matrix.row[0] = {cp * cy, sr * sp * cy - cr * sy, cr * sp * cy + sr * sy};
    matrix.row[1] = {cp * sy, sr * sp * sy + cr * cy, cr * sp * sy - sr * cy};
    matrix.row[2] = {-sp, sr * cp, cr * cp};
    return matrix;
}

[[nodiscard]] std::array<double, 3> apply(const RotationMatrix& matrix,
                                          const std::array<double, 3>& vector) noexcept {
    std::array<double, 3> out{};
    for (std::size_t i = 0; i < 3; ++i) {
        out[i] = matrix.row[i][0] * vector[0] + matrix.row[i][1] * vector[1] +
                 matrix.row[i][2] * vector[2];
    }
    return out;
}

// Trasposta: da assi terrestri ad assi corpo. La matrice di rotazione e'
// ortogonale, quindi l'inversa e' la trasposta e non serve invertire nulla.
[[nodiscard]] std::array<double, 3> apply_transposed(
    const RotationMatrix& matrix, const std::array<double, 3>& vector) noexcept {
    std::array<double, 3> out{};
    for (std::size_t j = 0; j < 3; ++j) {
        out[j] = matrix.row[0][j] * vector[0] + matrix.row[1][j] * vector[1] +
                 matrix.row[2][j] * vector[2];
    }
    return out;
}

[[nodiscard]] std::array<double, 3> cross(const std::array<double, 3>& a,
                                          const std::array<double, 3>& b) noexcept {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0]};
}

}  // namespace

Vehicle::Vehicle(Airframe airframe, Environment environment)
    : airframe_(std::move(airframe)),
      environment_(environment),
      propulsion_(airframe_.propulsion) {
    state_.mass_kg = airframe_.mass.empty_mass_kg;
    state_.altitude_m = environment_.field_elevation_m + airframe_.gear.contact_z_m;
}

AirState Vehicle::air_at_current_altitude() const {
    return environment_.atmosphere.at_altitude(state_.altitude_m);
}

void Vehicle::place_on_ground(double latitude_deg, double longitude_deg,
                              double heading_deg) {
    const double fuel = airframe_.mass.fuel_capacity_kg * 0.75;
    state_ = VehicleState{};
    state_.latitude_deg = latitude_deg;
    state_.longitude_deg = longitude_deg;
    state_.heading_rad = heading_deg * degrees_to_radians;
    state_.altitude_m = environment_.field_elevation_m + airframe_.gear.contact_z_m;
    state_.fuel_kg = fuel;
    state_.mass_kg = std::min(airframe_.mass.empty_mass_kg + fuel + default_payload_kg,
                              airframe_.mass.max_mass_kg);
    state_.on_ground = true;

    const AirState air = air_at_current_altitude();
    propulsion_.reset(false, air.temperature_k - 273.15);

    controls_ = Controls{};
    controls_.gear_down = true;
    controls_.brake = 1.0;
    controls_.magnetos_on = false;
    controls_.fuel_pump_on = false;
    controls_.throttle = 0.0;

    elapsed_s_ = 0.0;
    update_derived(air);
}

void Vehicle::place_in_flight(double latitude_deg, double longitude_deg,
                              double altitude_ft, double heading_deg,
                              double calibrated_airspeed_kt) {
    const double fuel = airframe_.mass.fuel_capacity_kg * 0.70;
    state_ = VehicleState{};
    state_.latitude_deg = latitude_deg;
    state_.longitude_deg = longitude_deg;
    state_.altitude_m = altitude_ft * metres_per_foot;
    state_.heading_rad = heading_deg * degrees_to_radians;
    state_.fuel_kg = fuel;
    state_.mass_kg = std::min(airframe_.mass.empty_mass_kg + fuel + default_payload_kg,
                              airframe_.mass.max_mass_kg);
    state_.on_ground = false;

    const AirState air = air_at_current_altitude();
    const double tas = tas_from_cas_m_s(calibrated_airspeed_kt * metres_per_second_per_knot,
                                        air);
    const double qbar = dynamic_pressure_pa(tas, air);
    const double weight = state_.mass_kg * standard_gravity_m_s2;

    controls_ = Controls{};
    controls_.gear_down = airframe_.gear.retractable ? false : true;

    // Incidenza di equilibrio: quella a cui la portanza equilibra il peso.
    // Si cerca per iterazione e non in forma chiusa perche' l'equilibratore
    // contribuisce alla portanza e la sua posizione dipende dall'incidenza: le
    // due condizioni si chiudono insieme, e cinque passaggi bastano perche' il
    // contributo dell'equilibratore e' piccolo.
    const double required_lift_coefficient = qbar > 0.0
                                                 ? weight / (qbar * airframe_.geometry.wing_area_m2)
                                                 : 0.0;
    double alpha = 0.0;
    double elevator = 0.0;
    for (int iteration = 0; iteration < 5; ++iteration) {
        const double target = required_lift_coefficient -
                              airframe_.aero.lift_elevator * elevator;
        alpha = (target - airframe_.aero.lift_zero) / airframe_.aero.lift_alpha;

        // Equilibratore che annulla il momento di beccheggio all'incidenza
        // trovata. E' il trim: senza di esso l'aeroplano partirebbe con un
        // momento residuo e cabrerebbe o picchierebbe immediatamente.
        const double zero_lift_alpha = -airframe_.aero.lift_zero /
                                       airframe_.aero.lift_alpha;
        const double moment_without_elevator =
            airframe_.aero.pitch_zero +
            airframe_.aero.pitch_alpha * (alpha - zero_lift_alpha);
        elevator = std::clamp(-moment_without_elevator / airframe_.aero.pitch_elevator,
                              -1.0, 1.0);
    }

    state_.pitch_rad = alpha;  // volo livellato: l'assetto coincide con l'incidenza
    state_.body_u_m_s = tas * std::cos(alpha);
    state_.body_w_m_s = tas * std::sin(alpha);
    controls_.elevator_trim = elevator;

    // Le grandezze derivate vanno popolate qui e non lasciate al primo passo di
    // integrazione. Non e' una formalita': i sensori si allineano su di esse, e
    // una velocita' vera nulla al momento dell'allineamento intrappola nelle
    // linee pneumatiche una pressione d'impatto nulla. L'anemometro parte da
    // zero e recupera nel tempo del filtro, cioe' l'aeroplano comincia ogni
    // prova con un guasto che nessuno ha chiesto.
    state_.true_airspeed_m_s = tas;
    state_.ground_speed_m_s = tas * std::cos(alpha);
    state_.track_rad = state_.heading_rad;
    state_.alpha_rad = alpha;

    // Manetta: la potenza necessaria e' quella che la spinta richiesta
    // consuma. La spinta richiesta in volo livellato e' la resistenza.
    AeroState aero_state{};
    aero_state.alpha_rad = alpha;
    aero_state.true_airspeed_m_s = tas;
    aero_state.dynamic_pressure_pa = qbar;
    const AeroLoads loads = aerodynamic_loads(airframe_, aero_state, controls_);
    const double drag = qbar * airframe_.geometry.wing_area_m2 * loads.drag_coefficient;
    const double required_power_kw = drag * tas /
                                     std::max(airframe_.propulsion.propeller_peak_efficiency,
                                              0.1) /
                                     1000.0;

    propulsion_.reset(true, air.temperature_k - 273.15);
    controls_.throttle = propulsion_.throttle_for_shaft_power(required_power_kw, air);
    controls_.fuel_pump_on = true;
    controls_.brake = 0.0;

    elapsed_s_ = 0.0;
    update_derived(air);
}

Vehicle::GroundLoads Vehicle::ground_loads(const AirState& air) const {
    GroundLoads out{};
    (void)air;

    const LandingGearConfig& gear = airframe_.gear;
    if (gear.retractable && !controls_.gear_down) {
        return out;
    }

    const RotationMatrix matrix = body_to_ned(state_.roll_rad, state_.pitch_rad,
                                              state_.heading_rad);
    const std::array<double, 3> body_velocity{state_.body_u_m_s, state_.body_v_m_s,
                                              state_.body_w_m_s};
    const std::array<double, 3> rates{state_.roll_rate_rad_s, state_.pitch_rate_rad_s,
                                      state_.yaw_rate_rad_s};

    // Tre punti: ruotino e due principali. Tre e non uno perche' con un solo
    // punto l'aeroplano non ruota al decollo: e' la coppia fra ruotino e
    // principali a far alzare il muso, e senza di essa la rotazione non si puo'
    // provare.
    const std::array<std::array<double, 3>, 3> contacts{
        std::array<double, 3>{gear.nose_x_m, 0.0, gear.contact_z_m},
        std::array<double, 3>{gear.main_x_m, gear.main_y_m, gear.contact_z_m},
        std::array<double, 3>{gear.main_x_m, -gear.main_y_m, gear.contact_z_m}};

    const double height_above_field = state_.altitude_m - environment_.field_elevation_m;
    const double friction = gear.rolling_friction +
                            std::clamp(controls_.brake, 0.0, 1.0) * gear.brake_friction;

    for (std::size_t index = 0; index < contacts.size(); ++index) {
        const std::array<double, 3>& arm = contacts[index];
        const std::array<double, 3> offset = apply(matrix, arm);
        const double contact_height = height_above_field - offset[2];
        if (contact_height >= 0.0) {
            continue;
        }

        // Velocita' del punto di contatto: quella del baricentro piu' il
        // contributo della rotazione. Serve allo smorzatore: un ammortizzatore
        // reagisce alla velocita' di compressione, non alla compressione.
        const std::array<double, 3> contact_body_velocity{
            body_velocity[0] + cross(rates, arm)[0],
            body_velocity[1] + cross(rates, arm)[1],
            body_velocity[2] + cross(rates, arm)[2]};
        const std::array<double, 3> contact_ned_velocity = apply(matrix,
                                                                 contact_body_velocity);

        const double compression = -contact_height;
        const double compression_rate = contact_ned_velocity[2];
        double normal = gear.spring_n_m * compression +
                        gear.damper_n_s_m * std::max(compression_rate, 0.0);
        normal = std::max(normal, 0.0);
        if (normal <= 0.0) {
            continue;
        }
        out.in_contact = true;

        // La reazione normale e' verticale nel mondo, non perpendicolare
        // all'aeroplano: una pista e' orizzontale anche quando l'aeroplano e'
        // inclinato, e applicarla lungo l'asse corpo z darebbe una componente
        // laterale inesistente.
        const std::array<double, 3> normal_ned{0.0, 0.0, -normal};
        std::array<double, 3> force_body = apply_transposed(matrix, normal_ned);

        // Attrito di rotolamento e freni, lungo l'asse corpo x e opposto al
        // moto. La tangente iperbolica evita che l'aeroplano fermo venga
        // spinto avanti e indietro dall'attrito stesso a ogni passo.
        const double longitudinal = -friction * normal *
                                    std::tanh(contact_body_velocity[0] / 0.4);
        force_body[0] += longitudinal;

        // Resistenza laterale degli pneumatici. E' cio' che tiene l'aeroplano
        // in pista: senza di essa il vento laterale lo porterebbe fuori senza
        // che il pilota possa fare nulla.
        double lateral = -0.80 * normal * std::tanh(contact_body_velocity[1] / 0.5);

        // Sterzo del ruotino, solo sul contatto anteriore. Il momento di
        // imbardata nasce dal braccio, non da un termine aggiunto a mano.
        if (index == 0) {
            lateral += 0.35 * normal * std::clamp(controls_.rudder, -1.0, 1.0);
        }
        force_body[1] += lateral;

        const std::array<double, 3> moment = cross(arm, force_body);
        out.force_x_n += force_body[0];
        out.force_y_n += force_body[1];
        out.force_z_n += force_body[2];
        out.moment_roll_nm += moment[0];
        out.moment_pitch_nm += moment[1];
        out.moment_yaw_nm += moment[2];
    }
    return out;
}

void Vehicle::integrate(double dt_s) {
    const AirState air = air_at_current_altitude();

    // Raffica deterministica: due sinusoidi incommensurabili, cosi' il segnale
    // non si ripete su un periodo corto e resta comunque identico fra due
    // esecuzioni. Un rumore pseudocasuale avrebbe dato lo stesso aspetto e la
    // stessa ripetibilita', ma non una banda di frequenze dichiarata.
    gust_phase_ += dt_s;
    const double gust = environment_.wind.gust_kt *
                        (0.6 * std::sin(gust_phase_ * 0.7) +
                         0.4 * std::sin(gust_phase_ * 1.9 + 1.1));

    const double wind_speed = (environment_.wind.speed_kt + gust) *
                              metres_per_second_per_knot;
    const double wind_from = environment_.wind.from_deg * degrees_to_radians;
    const std::array<double, 3> wind_ned{-wind_speed * std::cos(wind_from),
                                         -wind_speed * std::sin(wind_from),
                                         -environment_.wind.updraft_m_s};

    const RotationMatrix matrix = body_to_ned(state_.roll_rad, state_.pitch_rad,
                                              state_.heading_rad);
    const std::array<double, 3> wind_body = apply_transposed(matrix, wind_ned);

    const double u_air = state_.body_u_m_s - wind_body[0];
    const double v_air = state_.body_v_m_s - wind_body[1];
    const double w_air = state_.body_w_m_s - wind_body[2];
    const double airspeed = std::sqrt(u_air * u_air + v_air * v_air + w_air * w_air);

    // Sotto una velocita' minima incidenza e derapata non esistono.
    //
    // Non e' una protezione numerica: e' il fatto fisico. Senza flusso non c'e'
    // un angolo fra l'aeroplano e il flusso, e il rapporto fra due componenti
    // minuscole produce qualunque valore. Un aeroplano fermo in sosta con il
    // vento che gli muove l'aria addosso dichiarerebbe settantaquattro gradi
    // di incidenza e lo stallo inserito, che e' esattamente cio' che faceva la
    // prima versione: il banco partiva con l'allarme di stallo acceso al
    // parcheggio.
    constexpr double minimum_aero_speed_m_s = 5.0;
    const bool flow_present = airspeed >= minimum_aero_speed_m_s;

    AeroState aero_state{};
    aero_state.alpha_rad = flow_present ? std::atan2(w_air, std::max(u_air, 0.1)) : 0.0;
    aero_state.sideslip_rad = flow_present
                                  ? std::asin(std::clamp(v_air / airspeed, -1.0, 1.0))
                                  : 0.0;
    aero_state.roll_rate_rad_s = state_.roll_rate_rad_s;
    aero_state.pitch_rate_rad_s = state_.pitch_rate_rad_s;
    aero_state.yaw_rate_rad_s = state_.yaw_rate_rad_s;
    aero_state.true_airspeed_m_s = airspeed;
    aero_state.dynamic_pressure_pa = dynamic_pressure_pa(airspeed, air);

    const AeroLoads aero = aerodynamic_loads(airframe_, aero_state, controls_);
    const PropulsionState& engine = propulsion_.step(controls_, air, airspeed, dt_s);

    const double incidence = airframe_.thrust_incidence_deg * degrees_to_radians;
    const double thrust_x = engine.thrust_n * std::cos(incidence);
    const double thrust_z = -engine.thrust_n * std::sin(incidence);

    const GroundLoads ground = ground_loads(air);

    const double mass = std::max(state_.mass_kg, 1.0);
    const double force_x = aero.force_x_n + thrust_x + ground.force_x_n;
    const double force_y = aero.force_y_n + ground.force_y_n;
    const double force_z = aero.force_z_n + thrust_z + ground.force_z_n;

    // Accelerazione specifica: cio' che un accelerometro misura, cioe' tutte le
    // forze tranne la gravita'. Il fattore di carico e la pallina discendono da
    // qui, e per questo valgono anche in manovra non coordinata, dove la
    // formula della virata corretta darebbe zero.
    state_.accel_x_g = force_x / mass / standard_gravity_m_s2;
    state_.accel_y_g = force_y / mass / standard_gravity_m_s2;
    state_.accel_z_g = force_z / mass / standard_gravity_m_s2;
    state_.load_factor = -state_.accel_z_g;

    const double gravity_x = -standard_gravity_m_s2 * std::sin(state_.pitch_rad);
    const double gravity_y = standard_gravity_m_s2 * std::sin(state_.roll_rad) *
                             std::cos(state_.pitch_rad);
    const double gravity_z = standard_gravity_m_s2 * std::cos(state_.roll_rad) *
                             std::cos(state_.pitch_rad);

    const double p = state_.roll_rate_rad_s;
    const double q = state_.pitch_rate_rad_s;
    const double r = state_.yaw_rate_rad_s;

    const double du = force_x / mass + gravity_x + r * state_.body_v_m_s -
                      q * state_.body_w_m_s;
    const double dv = force_y / mass + gravity_y + p * state_.body_w_m_s -
                      r * state_.body_u_m_s;
    const double dw = force_z / mass + gravity_z + q * state_.body_u_m_s -
                      p * state_.body_v_m_s;

    const MassProperties& inertia = airframe_.mass;
    const double dp = (aero.moment_roll_nm + ground.moment_roll_nm +
                       (inertia.inertia_yy_kg_m2 - inertia.inertia_zz_kg_m2) * q * r) /
                      inertia.inertia_xx_kg_m2;
    const double dq = (aero.moment_pitch_nm + ground.moment_pitch_nm +
                       (inertia.inertia_zz_kg_m2 - inertia.inertia_xx_kg_m2) * p * r) /
                      inertia.inertia_yy_kg_m2;
    const double dr = (aero.moment_yaw_nm + ground.moment_yaw_nm +
                       (inertia.inertia_xx_kg_m2 - inertia.inertia_yy_kg_m2) * p * q) /
                      inertia.inertia_zz_kg_m2;

    state_.body_u_m_s += du * dt_s;
    state_.body_v_m_s += dv * dt_s;
    state_.body_w_m_s += dw * dt_s;
    state_.roll_rate_rad_s += dp * dt_s;
    state_.pitch_rate_rad_s += dq * dt_s;
    state_.yaw_rate_rad_s += dr * dt_s;

    const double cos_pitch = std::cos(state_.pitch_rad);
    const double tan_pitch = std::tan(std::clamp(state_.pitch_rad, -1.48, 1.48));
    const double sin_roll = std::sin(state_.roll_rad);
    const double cos_roll = std::cos(state_.roll_rad);

    state_.roll_rad += (p + (q * sin_roll + r * cos_roll) * tan_pitch) * dt_s;
    state_.pitch_rad += (q * cos_roll - r * sin_roll) * dt_s;
    state_.heading_rad += ((q * sin_roll + r * cos_roll) /
                           std::max(std::abs(cos_pitch), 0.09) *
                           (cos_pitch < 0.0 ? -1.0 : 1.0)) *
                          dt_s;

    // Beccheggio limitato a 85 gradi. Non e' un vincolo fisico: e' il confine
    // dichiarato degli angoli di Eulero, e fermarsi li' e' piu' onesto che
    // proseguire producendo un assetto che non significa nulla.
    state_.pitch_rad = std::clamp(state_.pitch_rad, -85.0 * degrees_to_radians,
                                  85.0 * degrees_to_radians);
    while (state_.heading_rad < 0.0) {
        state_.heading_rad += 2.0 * std::numbers::pi;
    }
    while (state_.heading_rad >= 2.0 * std::numbers::pi) {
        state_.heading_rad -= 2.0 * std::numbers::pi;
    }
    while (state_.roll_rad > std::numbers::pi) {
        state_.roll_rad -= 2.0 * std::numbers::pi;
    }
    while (state_.roll_rad < -std::numbers::pi) {
        state_.roll_rad += 2.0 * std::numbers::pi;
    }

    const RotationMatrix updated = body_to_ned(state_.roll_rad, state_.pitch_rad,
                                               state_.heading_rad);
    const std::array<double, 3> velocity_ned =
        apply(updated, {state_.body_u_m_s, state_.body_v_m_s, state_.body_w_m_s});

    state_.latitude_deg += velocity_ned[0] * dt_s / earth_radius_m * radians_to_degrees;
    const double cos_latitude = std::max(std::cos(state_.latitude_deg * degrees_to_radians),
                                         1.0e-4);
    state_.longitude_deg += velocity_ned[1] * dt_s / (earth_radius_m * cos_latitude) *
                            radians_to_degrees;
    state_.altitude_m -= velocity_ned[2] * dt_s;

    // Il terreno e' un fondo, non un suggerimento: con passo grande la molla
    // del carrello puo' non bastare a fermare la discesa in un solo passo, e
    // un aeroplano che attraversa la pista non e' un modo utile di sbagliare.
    const double floor_m = environment_.field_elevation_m +
                           airframe_.gear.contact_z_m * 0.25;
    if (state_.altitude_m < floor_m) {
        state_.altitude_m = floor_m;
        if (velocity_ned[2] > 0.0) {
            state_.body_w_m_s = 0.0;
        }
    }

    state_.on_ground = ground.in_contact;

    const double burned = engine.fuel_flow_kg_s * dt_s;
    state_.fuel_kg = std::max(state_.fuel_kg - burned, 0.0);
    state_.mass_kg = std::max(state_.mass_kg - burned, airframe_.mass.empty_mass_kg);

    state_.alpha_rad = aero_state.alpha_rad;
    state_.sideslip_rad = aero_state.sideslip_rad;
    state_.stalled = aero.stalled && flow_present;
    state_.lift_coefficient = aero.lift_coefficient;
    state_.true_airspeed_m_s = airspeed;
    state_.ground_speed_m_s = std::sqrt(velocity_ned[0] * velocity_ned[0] +
                                        velocity_ned[1] * velocity_ned[1]);
    state_.track_rad = std::atan2(velocity_ned[1], velocity_ned[0]);
    state_.vertical_speed_m_s = -velocity_ned[2];

    elapsed_s_ += dt_s;
}

void Vehicle::update_derived(const AirState& air) {
    state_.calibrated_airspeed_m_s = cas_from_tas_m_s(state_.true_airspeed_m_s, air);
    state_.mach = mach_from_tas(state_.true_airspeed_m_s, air);
    state_.static_pressure_pa = air.pressure_pa;
    state_.impact_pressure_pa = impact_pressure_from_cas_pa(state_.calibrated_airspeed_m_s);
    state_.outside_air_temperature_c = air.temperature_k - 273.15;
}

void Vehicle::step(double dt_s) {
    if (dt_s <= 0.0) {
        return;
    }

    // Suddivisione del passo. Il numero di sottopassi e' calcolato e non
    // fissato: il banco gira anche a venti volte il tempo reale, e un numero
    // fisso di sottopassi perderebbe stabilita' proprio nelle esecuzioni
    // accelerate, che sono quelle usate per le regressioni lunghe.
    const int substeps = static_cast<int>(std::ceil(dt_s / max_substep_s));
    const double substep = dt_s / static_cast<double>(substeps);
    for (int index = 0; index < substeps; ++index) {
        integrate(substep);
    }
    update_derived(air_at_current_altitude());
}

}  // namespace efis::fdm
