#include "efis/fdm/sensors.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace efis::fdm {
namespace {

constexpr double radians_to_degrees = 180.0 / std::numbers::pi;
constexpr double degrees_to_radians = std::numbers::pi / 180.0;
constexpr double earth_radius_m = 6371000.0;
constexpr double kelvin_offset = 273.15;

// Costante di tempo del variometro barometrico.
//
// Un secondo: e' l'ordine di grandezza del capillare di un variometro vero, ed
// e' anche cio' che serve a derivare una quota quantizzata al piede senza
// produrre un valore che salta. Senza filtro la velocita' verticale
// barometrica mostrerebbe gradini di centinaia di piedi al minuto a ogni
// quantizzazione, e il confronto con quella satellitare dichiarerebbe
// disaccordi che sono soltanto rumore di derivazione.
constexpr double vertical_speed_time_constant_s = 1.0;

[[nodiscard]] double approach(double current, double target,
                              double time_constant_s, double dt_s) noexcept {
    if (time_constant_s <= 0.0) {
        return target;
    }
    return current + (target - current) * (1.0 - std::exp(-dt_s / time_constant_s));
}

[[nodiscard]] double quantise(double value, double resolution) noexcept {
    if (resolution <= 0.0) {
        return value;
    }
    return std::round(value / resolution) * resolution;
}

[[nodiscard]] double wrap_degrees(double value) noexcept {
    double out = std::fmod(value, 360.0);
    if (out < 0.0) {
        out += 360.0;
    }
    return out;
}

}  // namespace

const char* sensor_fault_name(SensorFault fault) noexcept {
    switch (fault) {
        case SensorFault::none: return "NONE";
        case SensorFault::pitot_blocked: return "PITOT_BLOCKED";
        case SensorFault::pitot_leak: return "PITOT_LEAK";
        case SensorFault::static_blocked: return "STATIC_BLOCKED";
        case SensorFault::ahrs_latched: return "AHRS_LATCHED";
        case SensorFault::ahrs_drift: return "AHRS_DRIFT";
        case SensorFault::gnss_outage: return "GNSS_OUTAGE";
        case SensorFault::gnss_degraded: return "GNSS_DEGRADED";
        case SensorFault::magnetometer_bias: return "MAGNETOMETER_BIAS";
        case SensorFault::oat_stuck: return "OAT_STUCK";
    }
    return "NONE";
}

SensorSuite::SensorSuite(SensorPolicy policy, std::uint64_t seed) noexcept
    : policy_(policy), random_state_(seed == 0 ? 1 : seed) {}

double SensorSuite::uniform() noexcept {
    // xorshift64*: deterministico, ripetibile e sufficiente per del rumore di
    // sensore. Un generatore della libreria standard avrebbe dato garanzie
    // statistiche migliori e nessuna garanzia di riprodurre la stessa sequenza
    // fra implementazioni diverse, che qui e' la proprieta' che serve: una
    // prova deve dare lo stesso risultato su Windows e sul Raspberry.
    random_state_ ^= random_state_ >> 12;
    random_state_ ^= random_state_ << 25;
    random_state_ ^= random_state_ >> 27;
    const std::uint64_t value = random_state_ * 0x2545F4914F6CDD1DULL;
    return static_cast<double>(value >> 11) / 9007199254740992.0;
}

double SensorSuite::gaussian() noexcept {
    // Box-Muller. Due uniformi per una normale, senza tenere in memoria la
    // seconda: costa una radice in piu' per campione e tiene il generatore
    // senza stato nascosto, che e' cio' che rende ripetibile anche un banco
    // interrotto a meta'.
    const double u1 = std::max(uniform(), 1.0e-12);
    const double u2 = uniform();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * std::numbers::pi * u2);
}

void SensorSuite::set_fault(SensorFault fault) noexcept {
    fault_ = fault;

    // Il momento in cui il guasto comincia conta: un Pitot ostruito
    // intrappola la pressione che c'era in quell'istante, non una pressione di
    // riferimento. E' questa memoria a produrre la velocita' che cresce in
    // salita con l'anemometro guasto.
    switch (fault) {
        case SensorFault::pitot_blocked:
            trapped_total_pa_ = filtered_total_pa_;
            break;
        case SensorFault::static_blocked:
            trapped_static_pa_ = filtered_static_pa_;
            break;
        case SensorFault::ahrs_latched:
            latched_roll_deg_ = indicated_.roll_deg;
            latched_pitch_deg_ = indicated_.pitch_deg;
            latched_heading_deg_ = indicated_.magnetic_heading_deg;
            latched_ = true;
            break;
        case SensorFault::oat_stuck:
            stuck_temperature_c_ = indicated_.total_air_temperature_c;
            break;
        case SensorFault::none:
            latched_ = false;
            heading_drift_deg_ = 0.0;
            break;
        case SensorFault::pitot_leak:
        case SensorFault::ahrs_drift:
        case SensorFault::gnss_outage:
        case SensorFault::gnss_degraded:
        case SensorFault::magnetometer_bias:
            break;
    }
}

void SensorSuite::align(const VehicleState& truth, const AirState& air) noexcept {
    filtered_static_pa_ = air.pressure_pa;
    filtered_total_pa_ = air.pressure_pa + truth.impact_pressure_pa;
    trapped_static_pa_ = filtered_static_pa_;
    trapped_total_pa_ = filtered_total_pa_;
    previous_pressure_altitude_ft_ = Atmosphere::pressure_altitude_m(filtered_static_pa_) *
                                     feet_per_metre;
    filtered_vertical_speed_fpm_ = truth.vertical_speed_m_s *
                                   feet_per_minute_per_metre_per_second;
    delayed_latitude_deg_ = truth.latitude_deg;
    delayed_longitude_deg_ = truth.longitude_deg;
    delayed_ground_speed_kt_ = truth.ground_speed_m_s * knots_per_metre_per_second;
    delayed_track_deg_ = wrap_degrees(truth.track_rad * radians_to_degrees);
    delayed_vertical_speed_fpm_ = filtered_vertical_speed_fpm_;
    delayed_altitude_ft_ = truth.altitude_m * feet_per_metre;
    gnss_hold_latitude_deg_ = delayed_latitude_deg_;
    gnss_hold_longitude_deg_ = delayed_longitude_deg_;
    gnss_hold_ground_speed_kt_ = delayed_ground_speed_kt_;
    gnss_hold_track_deg_ = delayed_track_deg_;
    gnss_hold_vertical_speed_fpm_ = delayed_vertical_speed_fpm_;
    gnss_hold_altitude_ft_ = delayed_altitude_ft_;
    gnss_timer_s_ = 0.0;
    heading_drift_deg_ = 0.0;
    latched_ = false;
    initialised_ = true;
}

const IndicatedState& SensorSuite::update(const VehicleState& truth,
                                          const AirState& air,
                                          double dt_s) noexcept {
    if (!initialised_) {
        align(truth, air);
    }

    // --- Pressioni ---------------------------------------------------------
    //
    // L'errore di posizione agisce sulla presa statica, che e' dove avviene:
    // la presa e' su una superficie attorno a cui l'aria accelera, e quanto
    // acceleri dipende dall'incidenza. Da qui la differenza fra velocita'
    // indicata e calibrata, che cresce avvicinandosi allo stallo.
    const double dynamic = dynamic_pressure_pa(truth.true_airspeed_m_s, air);
    const double position_error = dynamic * (policy_.static_position_error +
                                             policy_.static_position_error_alpha *
                                                 truth.alpha_rad);
    const double true_static = air.pressure_pa + position_error;
    const double true_total = air.pressure_pa + truth.impact_pressure_pa;

    filtered_static_pa_ = approach(filtered_static_pa_, true_static, policy_.static_lag_s,
                                   dt_s);
    filtered_total_pa_ = approach(filtered_total_pa_, true_total, policy_.pitot_lag_s,
                                  dt_s);

    double measured_static = filtered_static_pa_;
    double measured_total = filtered_total_pa_;

    switch (fault_) {
        case SensorFault::pitot_blocked:
            // La totale resta intrappolata. Il rumore del sensore continua: un
            // tubo ostruito non ferma l'elettronica, e se il rumore si
            // fermasse la stasi di ADR-004 si rileverebbe dall'immobilita' del
            // bit meno significativo invece che dal contenuto. Sarebbe un
            // banco che conferma il rilevatore invece di metterlo alla prova.
            measured_total = trapped_total_pa_;
            break;
        case SensorFault::pitot_leak:
            // Perdita: parte della pressione d'impatto si disperde. La
            // velocita' legge meno, e nient'altro cambia.
            measured_total = measured_static + (measured_total - measured_static) * 0.60;
            break;
        case SensorFault::static_blocked:
            measured_static = trapped_static_pa_;
            break;
        default:
            break;
    }

    const double impact = std::max(measured_total - measured_static, 0.0);
    const double indicated_airspeed_kt = cas_from_impact_pressure_m_s(impact) *
                                         knots_per_metre_per_second;
    const double pressure_altitude_ft = Atmosphere::pressure_altitude_m(measured_static) *
                                        feet_per_metre;

    indicated_.differential_pressure_pa = impact + gaussian() * 2.0;
    indicated_.static_pressure_pa = measured_static;
    indicated_.indicated_airspeed_kt = std::max(
        quantise(indicated_airspeed_kt + gaussian() * policy_.airspeed_noise_kt,
                 policy_.airspeed_resolution_kt),
        0.0);
    indicated_.pressure_altitude_ft =
        quantise(pressure_altitude_ft + gaussian() * policy_.altitude_noise_ft,
                 policy_.altitude_resolution_ft);
    indicated_.baro_setting_hpa = baro_setting_hpa_;
    indicated_.indicated_altitude_ft =
        quantise(Atmosphere::indicated_altitude_m(measured_static,
                                                  baro_setting_hpa_ *
                                                      pascal_per_hectopascal) *
                     feet_per_metre +
                     gaussian() * policy_.altitude_noise_ft,
                 policy_.altitude_resolution_ft);

    // Velocita' verticale barometrica: derivata della quota di pressione
    // misurata, filtrata. Non si copia quella vera del modello, e la
    // differenza e' il punto: questa catena passa dalla presa statica, quindi
    // con la statica ostruita si ferma, mentre quella satellitare continua. E'
    // esattamente il confronto che ADR-004 ha introdotto.
    if (dt_s > 0.0) {
        const double raw = (indicated_.pressure_altitude_ft -
                            previous_pressure_altitude_ft_) / dt_s * 60.0;
        filtered_vertical_speed_fpm_ = approach(filtered_vertical_speed_fpm_, raw,
                                                vertical_speed_time_constant_s, dt_s);
    }
    previous_pressure_altitude_ft_ = indicated_.pressure_altitude_ft;
    indicated_.vertical_speed_fpm = std::round(filtered_vertical_speed_fpm_ / 10.0) * 10.0;

    // Numero di Mach dal rapporto fra pressione d'impatto e statica: e' il
    // calcolo del calcolatore dati aria, e non usa la temperatura. Per questo
    // un guasto della sonda non sposta il Mach ma sposta la velocita' vera.
    indicated_.mach = std::sqrt(std::max(5.0 * (std::pow(impact /
                                                             std::max(measured_static,
                                                                      1.0) +
                                                         1.0,
                                                     2.0 / 7.0) -
                                                1.0),
                                         0.0));

    // Temperatura totale: cio' che misura una sonda immersa nel flusso. La
    // statica si ricava togliendo il recupero, e l'EFIS deve farlo per avere la
    // velocita' vera.
    const double static_temperature_k = air.temperature_k;
    const double total_temperature_c = fault_ == SensorFault::oat_stuck
                                           ? stuck_temperature_c_
                                           : static_temperature_k *
                                                     (1.0 + 0.2 *
                                                                policy_.temperature_recovery_factor *
                                                                truth.mach * truth.mach) -
                                                 kelvin_offset;
    indicated_.total_air_temperature_c = std::round((total_temperature_c +
                                                     gaussian() * 0.1) * 4.0) / 4.0;
    indicated_.air_data_valid = true;

    // --- Assetto -----------------------------------------------------------
    const double roll_deg = truth.roll_rad * radians_to_degrees +
                            gaussian() * policy_.attitude_noise_deg;
    const double pitch_deg = truth.pitch_rad * radians_to_degrees +
                             gaussian() * policy_.attitude_noise_deg;

    if (fault_ == SensorFault::ahrs_drift) {
        heading_drift_deg_ += policy_.heading_drift_deg_min / 60.0 * dt_s;
    }

    const double true_heading_deg = wrap_degrees(truth.heading_rad * radians_to_degrees);
    double magnetic_heading_deg = wrap_degrees(true_heading_deg -
                                               magnetic_variation_deg_ +
                                               heading_drift_deg_ +
                                               gaussian() * policy_.attitude_noise_deg);
    if (fault_ == SensorFault::magnetometer_bias) {
        magnetic_heading_deg = wrap_degrees(magnetic_heading_deg + 14.0);
    }

    if (fault_ == SensorFault::ahrs_latched && latched_) {
        // Latchato: i valori restano quelli dell'istante del guasto, con il
        // proprio rumore. Nessuna validita' ridotta, nessun segno visibile:
        // l'inerziale continua a dichiararsi sano, perche' e' cosi' che si
        // guasta. Il solo modo di accorgersene e' guardare che non cambia.
        indicated_.roll_deg = latched_roll_deg_ + gaussian() * policy_.attitude_noise_deg;
        indicated_.pitch_deg = latched_pitch_deg_ +
                               gaussian() * policy_.attitude_noise_deg;
        indicated_.magnetic_heading_deg = wrap_degrees(latched_heading_deg_ +
                                                       gaussian() *
                                                           policy_.attitude_noise_deg);
        indicated_.turn_rate_deg_s = gaussian() * policy_.rate_noise_deg_s;
    } else {
        indicated_.roll_deg = roll_deg;
        indicated_.pitch_deg = pitch_deg;
        indicated_.magnetic_heading_deg = magnetic_heading_deg;

        // Rateo di virata: la velocita' angolare della prua ricavata dal
        // vettore dei ratei, non derivando la prua indicata. E' il calcolo che
        // un inerziale esegue, e la differenza conta: derivare una prua
        // quantizzata e filtrata darebbe un rateo in ritardo e rumoroso, e in
        // virata inclinata nemmeno uguale.
        const double cos_pitch = std::max(std::cos(truth.pitch_rad), 0.09);
        const double heading_rate = (truth.pitch_rate_rad_s * std::sin(truth.roll_rad) +
                                     truth.yaw_rate_rad_s * std::cos(truth.roll_rad)) /
                                    cos_pitch;
        indicated_.turn_rate_deg_s = heading_rate * radians_to_degrees +
                                     gaussian() * policy_.rate_noise_deg_s;
    }
    indicated_.true_heading_deg = true_heading_deg;
    indicated_.attitude_valid = true;
    indicated_.lateral_acceleration_g = truth.accel_y_g +
                                        gaussian() * policy_.acceleration_noise_g;
    indicated_.normal_acceleration_g = truth.load_factor +
                                       gaussian() * policy_.acceleration_noise_g;
    indicated_.angle_of_attack_deg = truth.alpha_rad * radians_to_degrees +
                                     gaussian() * 0.15;

    // --- Satellitare -------------------------------------------------------
    delayed_latitude_deg_ = approach(delayed_latitude_deg_, truth.latitude_deg,
                                     policy_.gnss_latency_s, dt_s);
    delayed_longitude_deg_ = approach(delayed_longitude_deg_, truth.longitude_deg,
                                      policy_.gnss_latency_s, dt_s);
    delayed_ground_speed_kt_ = approach(delayed_ground_speed_kt_,
                                        truth.ground_speed_m_s *
                                            knots_per_metre_per_second,
                                        policy_.gnss_latency_s, dt_s);
    delayed_vertical_speed_fpm_ = approach(delayed_vertical_speed_fpm_,
                                           truth.vertical_speed_m_s *
                                               feet_per_minute_per_metre_per_second,
                                           policy_.gnss_latency_s, dt_s);
    delayed_altitude_ft_ = approach(delayed_altitude_ft_,
                                    truth.altitude_m * feet_per_metre,
                                    policy_.gnss_latency_s, dt_s);

    // La rotta e' un angolo: filtrarla direttamente salterebbe di 360 gradi a
    // ogni passaggio per nord. Si filtra la differenza riportata nel giro
    // corto, che e' la stessa cosa senza il salto.
    const double track_deg = wrap_degrees(truth.track_rad * radians_to_degrees);
    double track_error = track_deg - delayed_track_deg_;
    while (track_error > 180.0) {
        track_error -= 360.0;
    }
    while (track_error < -180.0) {
        track_error += 360.0;
    }
    delayed_track_deg_ = wrap_degrees(delayed_track_deg_ +
                                      track_error * (1.0 - std::exp(-dt_s /
                                                                    std::max(policy_.gnss_latency_s,
                                                                             1.0e-3))));

    gnss_timer_s_ += dt_s;
    const double gnss_period_s = 1.0 / std::max(policy_.gnss_rate_hz, 0.1);
    if (gnss_timer_s_ >= gnss_period_s) {
        gnss_timer_s_ = std::fmod(gnss_timer_s_, gnss_period_s);

        const double sigma = policy_.gnss_position_sigma_m *
                             (fault_ == SensorFault::gnss_degraded ? 5.0 : 1.0);
        const double metres_per_degree_latitude = earth_radius_m * degrees_to_radians;
        const double metres_per_degree_longitude =
            metres_per_degree_latitude *
            std::max(std::cos(truth.latitude_deg * degrees_to_radians), 1.0e-4);

        gnss_hold_latitude_deg_ = delayed_latitude_deg_ +
                                  gaussian() * sigma / metres_per_degree_latitude;
        gnss_hold_longitude_deg_ = delayed_longitude_deg_ +
                                   gaussian() * sigma / metres_per_degree_longitude;
        gnss_hold_ground_speed_kt_ = std::max(delayed_ground_speed_kt_ +
                                                  gaussian() * 0.15,
                                              0.0);
        gnss_hold_track_deg_ = wrap_degrees(delayed_track_deg_ + gaussian() * 0.4);
        gnss_hold_vertical_speed_fpm_ = delayed_vertical_speed_fpm_ +
                                        gaussian() *
                                            policy_.gnss_vertical_speed_sigma_fpm;
        gnss_hold_altitude_ft_ = delayed_altitude_ft_ + gaussian() * sigma *
                                                            feet_per_metre * 1.5;
    }

    // Fra due aggiornamenti il valore resta quello: il ricevitore pubblica a
    // cinque hertz e non interpola. Un banco che interpolasse nasconderebbe
    // proprio il ritardo che l'EFIS deve gestire quando confronta la velocita'
    // al suolo con quella indicata in accelerazione.
    indicated_.latitude_deg = gnss_hold_latitude_deg_;
    indicated_.longitude_deg = gnss_hold_longitude_deg_;
    indicated_.ground_speed_kt = gnss_hold_ground_speed_kt_;
    indicated_.track_deg = gnss_hold_track_deg_;
    indicated_.gnss_vertical_speed_fpm = std::round(gnss_hold_vertical_speed_fpm_ /
                                                    10.0) * 10.0;
    indicated_.gnss_altitude_ft = gnss_hold_altitude_ft_;

    switch (fault_) {
        case SensorFault::gnss_outage:
            // Assente, non plausibile. Il ricevitore che perde il fix non
            // inventa una posizione: dichiara di non averla, e l'EFIS deve
            // togliere il dato. Questo e' il guasto onesto, e serve a provare
            // che il display sappia mostrare un'assenza.
            indicated_.gnss_valid = false;
            indicated_.satellites = 0;
            indicated_.horizontal_dilution = 99.9;
            break;
        case SensorFault::gnss_degraded:
            indicated_.gnss_valid = true;
            indicated_.satellites = 4;
            indicated_.horizontal_dilution = 4.8;
            break;
        default:
            indicated_.gnss_valid = true;
            indicated_.satellites = 12;
            indicated_.horizontal_dilution = 0.9;
            break;
    }

    return indicated_;
}

}  // namespace efis::fdm
