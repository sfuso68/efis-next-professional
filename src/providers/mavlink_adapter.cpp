#include "efis/providers/mavlink_adapter.hpp"

#include <algorithm>
#include <cmath>

namespace efis {
namespace {

constexpr double knots_per_meter_per_second = 1.943844;
constexpr double feet_per_meter = 3.280840;
constexpr double feet_per_minute_per_meter_per_second = 196.850394;
constexpr double degrees_per_radian = 57.295779513082321;

double normalise_heading(double degrees) noexcept {
    double value = std::fmod(degrees, 360.0);
    if (value < 0.0) value += 360.0;
    return value;
}

}  // namespace

MavlinkAdapter::MavlinkAdapter(NodeHealthPolicy policy) : health_(policy) {}

void MavlinkAdapter::consume(std::span<const std::uint8_t> bytes, MonoTime now) {
    parser_.push(bytes);
    while (const auto frame = parser_.next()) {
        track(*frame, now);
        apply(*frame, now);
    }
    snapshot_.timestamp = now;
}

void MavlinkAdapter::track(const mavlink::Frame& frame, MonoTime now) {
    const NodeId node = node_of(frame.system_id, frame.component_id);

    auto it = std::find_if(sequences_.begin(), sequences_.end(),
                           [node](const SequenceTracker& t) { return t.node == node; });
    if (it == sequences_.end()) {
        sequences_.push_back(SequenceTracker{node, frame.sequence, 0, true});
        it = std::prev(sequences_.end());
    } else {
        // Il contatore MAVLink e' a otto bit e si azzera ogni 256 messaggi.
        // Passarlo cosi' com'e' al monitor farebbe apparire una regressione a
        // ogni giro, cioe' un guasto inesistente ogni due secondi e mezzo a
        // cento messaggi al secondo.
        if (frame.sequence < it->last) {
            it->extended += 256;
            ++statistics_.sequence_wraps;
        }
        it->last = frame.sequence;
    }

    const std::uint64_t sequence = it->extended + frame.sequence;
    health_.heartbeat(node, now, sequence);
}

void MavlinkAdapter::update(MonoTime now) {
    auto changes = health_.update(now);
    node_events_.insert(node_events_.end(), changes.begin(), changes.end());
}

void MavlinkAdapter::apply(const mavlink::Frame& frame, MonoTime now) {
    // Il parser consegna soltanto frame verificati, e verificarli richiede la
    // definizione: qui non puo' essere assente.
    const mavlink::MessageDefinition* found = mavlink::find_message(frame.message_id);
    if (found == nullptr) {
        ++statistics_.frames_ignored;
        return;
    }
    const mavlink::MessageDefinition& definition = *found;
    const NodeId node = node_of(frame.system_id, frame.component_id);

    // Ogni campione prodotto qui descrive il mondo reale: proviene dal
    // controllo di volo, non dalla simulazione, e va marcato come tale.
    auto measured = [&](double value, double raw, SignalId signal, Unit unit,
                        Validity validity) {
        Sample<double> sample{};
        sample.value = value;
        sample.raw = raw;
        sample.source = SourceId::pixhawk_primary;
        sample.sampled_at = now;
        sample.received_at = now;
        sample.quality = Quality::nominal;
        sample.validity = validity;
        sample.sequence = frame.sequence;
        sample.origin.signal = signal;
        sample.origin.unit = unit;
        sample.origin.node = node;
        sample.origin.simulated = false;
        sample.utc = utc_;
        return sample;
    };

    switch (frame.message_id) {
        case 30: {  // ATTITUDE
            const auto roll = mavlink::read_number(definition, frame, "roll");
            const auto pitch = mavlink::read_number(definition, frame, "pitch");
            const auto yaw = mavlink::read_number(definition, frame, "yaw");
            if (!roll || !pitch || !yaw) break;
            // Il valore ingegneristico e' in gradi perche' e' cosi' che va
            // mostrato; il grezzo resta in radianti, come il sensore lo emette.
            snapshot_.attitude.roll_deg =
                measured(*roll * degrees_per_radian, *roll, SignalId::roll,
                         Unit::degree, Validity::valid);
            snapshot_.attitude.pitch_deg =
                measured(*pitch * degrees_per_radian, *pitch, SignalId::pitch,
                         Unit::degree, Validity::valid);
            snapshot_.attitude.heading_deg =
                measured(normalise_heading(*yaw * degrees_per_radian), *yaw,
                         SignalId::heading, Unit::degree, Validity::valid);
            ++statistics_.frames_applied;
            break;
        }

        case 74: {  // VFR_HUD
            const auto airspeed = mavlink::read_number(definition, frame, "airspeed");
            const auto groundspeed = mavlink::read_number(definition, frame, "groundspeed");
            const auto altitude = mavlink::read_number(definition, frame, "alt");
            const auto climb = mavlink::read_number(definition, frame, "climb");
            if (airspeed) {
                // La velocita' indicata vale soltanto se il controllo di volo
                // dichiara sano il sensore che la produce.
                air_data_.computed_airspeed_kt =
                    measured(*airspeed * knots_per_meter_per_second, *airspeed,
                             SignalId::indicated_airspeed, Unit::knot,
                             differential_pressure_healthy_ ? Validity::valid
                                                            : Validity::unavailable);
                snapshot_.air_data.indicated_airspeed_kt = air_data_.computed_airspeed_kt;
            }
            if (groundspeed) {
                snapshot_.position.ground_speed_kt =
                    measured(*groundspeed * knots_per_meter_per_second, *groundspeed,
                             SignalId::ground_speed, Unit::knot,
                             gnss_fix_type_ >= gnss_fix_3d ? Validity::valid
                                                           : Validity::unavailable);
            }
            if (altitude) {
                const bool estimator_healthy = !estimator_.reported ||
                    estimator_.vertical_position_healthy;
                snapshot_.air_data.pressure_altitude_ft =
                    measured(*altitude * feet_per_meter, *altitude,
                             SignalId::pressure_altitude, Unit::foot,
                             absolute_pressure_healthy_ && estimator_healthy
                                 ? Validity::valid : Validity::unavailable);
            }
            if (climb) {
                const bool estimator_healthy = !estimator_.reported ||
                    estimator_.vertical_velocity_healthy;
                snapshot_.air_data.vertical_speed_fpm =
                    measured(*climb * feet_per_minute_per_meter_per_second, *climb,
                             SignalId::vertical_speed, Unit::foot_per_minute,
                             absolute_pressure_healthy_ && estimator_healthy
                                 ? Validity::valid : Validity::unavailable);
            }
            ++statistics_.frames_applied;
            break;
        }

        case 24: {  // GPS_RAW_INT
            const auto fix = mavlink::read_integer(definition, frame, "fix_type");
            const auto latitude = mavlink::read_integer(definition, frame, "lat");
            const auto longitude = mavlink::read_integer(definition, frame, "lon");
            const auto satellites =
                mavlink::read_integer(definition, frame, "satellites_visible");
            if (fix) gnss_fix_type_ = *fix;
            // Il numero di satelliti non decide nulla, e non deve: la validita'
            // dipende dal tipo di fix. Serve pero' a dire perche' un fix manca,
            // che e' cio' che distingue un'antenna schermata da un ricevitore
            // guasto.
            if (satellites) gnss_satellites_ = *satellites;

            // Senza fix tridimensionale le coordinate ricevute non sono una
            // posizione: il paragrafo 13 dell'handoff impone di ignorarle. Il
            // valore grezzo viene comunque conservato, perche' serve alla
            // diagnosi del ricevitore, ma la validita' resta indisponibile e
            // nessun consumer puo' usarlo.
            const Validity validity = gnss_fix_type_ >= gnss_fix_3d
                                          ? Validity::valid
                                          : Validity::unavailable;
            if (latitude) {
                snapshot_.position.latitude_deg =
                    measured(static_cast<double>(*latitude) * 1e-7,
                             static_cast<double>(*latitude), SignalId::latitude,
                             Unit::degree, validity);
            }
            if (longitude) {
                snapshot_.position.longitude_deg =
                    measured(static_cast<double>(*longitude) * 1e-7,
                             static_cast<double>(*longitude), SignalId::longitude,
                             Unit::degree, validity);
            }
            revalidate();
            ++statistics_.frames_applied;
            break;
        }

        case 193: {  // EKF_STATUS_REPORT
            // I valori dei bit sono quelli dell'enumerazione EKF_STATUS_FLAGS
            // del protocollo. Un bit acceso significa stima sana e disponibile.
            constexpr std::uint32_t ekf_attitude = 1U;
            constexpr std::uint32_t ekf_velocity_horizontal = 2U;
            constexpr std::uint32_t ekf_velocity_vertical = 4U;
            constexpr std::uint32_t ekf_position_vertical_absolute = 8U;
            constexpr std::uint32_t ekf_position_horizontal_absolute = 16U;
            constexpr std::uint32_t ekf_constant_position_mode = 128U;
            constexpr std::uint32_t ekf_uninitialised = 1024U;
            constexpr std::uint32_t ekf_gnss_glitching = 32768U;

            const auto flags = mavlink::read_unsigned(definition, frame, "flags");
            if (flags) {
                const auto bits = static_cast<std::uint32_t>(*flags);
                estimator_.reported = true;
                estimator_.flags = bits;
                estimator_.attitude_healthy = (bits & ekf_attitude) != 0;
                estimator_.horizontal_velocity_healthy =
                    (bits & ekf_velocity_horizontal) != 0;
                estimator_.vertical_velocity_healthy =
                    (bits & ekf_velocity_vertical) != 0;
                estimator_.vertical_position_healthy =
                    (bits & ekf_position_vertical_absolute) != 0;
                estimator_.horizontal_position_healthy =
                    (bits & ekf_position_horizontal_absolute) != 0;
                estimator_.uninitialised = (bits & ekf_uninitialised) != 0;
                estimator_.dead_reckoning = (bits & ekf_constant_position_mode) != 0;
                estimator_.gnss_glitching = (bits & ekf_gnss_glitching) != 0;
            }

            auto variance = [&](const char* name, double& destination) {
                if (const auto value = mavlink::read_number(definition, frame, name)) {
                    destination = *value;
                }
            };
            variance("velocity_variance", estimator_.velocity_variance);
            variance("pos_horiz_variance", estimator_.horizontal_position_variance);
            variance("pos_vert_variance", estimator_.vertical_position_variance);
            variance("compass_variance", estimator_.compass_variance);
            variance("airspeed_variance", estimator_.airspeed_variance);

            revalidate();
            ++statistics_.frames_applied;
            break;
        }

        case 241: {  // VIBRATION
            auto axis = [&](const char* name, double& destination) {
                if (const auto value = mavlink::read_number(definition, frame, name)) {
                    destination = *value;
                }
            };
            axis("vibration_x", vibration_.x_axis);
            axis("vibration_y", vibration_.y_axis);
            axis("vibration_z", vibration_.z_axis);

            // I contatori di saturazione non tornano indietro. Interessa il
            // momento in cui crescono, non il fatto che in passato l'aeromobile
            // abbia vibrato: quello resta vero per tutto il volo e non dice piu'
            // nulla su adesso.
            bool grown = false;
            auto clipping = [&](const char* name, std::uint32_t& destination) {
                const auto value = mavlink::read_unsigned(definition, frame, name);
                if (!value) return;
                const auto counted = static_cast<std::uint32_t>(*value);
                if (vibration_.reported && counted > destination) grown = true;
                destination = counted;
            };
            clipping("clipping_0", vibration_.clipping_0);
            clipping("clipping_1", vibration_.clipping_1);
            clipping("clipping_2", vibration_.clipping_2);
            vibration_.clipping_increasing = grown;
            vibration_.reported = true;

            ++statistics_.frames_applied;
            break;
        }

        case 33: {  // GLOBAL_POSITION_INT
            // Se ne prende soltanto la velocita' verticale. Posizione e prua
            // arrivano gia' da GPS_RAW_INT e ATTITUDE, e prenderle due volte
            // significherebbe farle dipendere da quale messaggio arriva per
            // ultimo invece che dall'arbitrato.
            //
            // `vz` e' in centimetri al secondo e positiva verso il basso, come
            // vuole la convenzione NED. La quota sale quando vz e' negativa,
            // quindi il segno va invertito: una salita deve leggersi positiva
            // sullo strumento, come la velocita' verticale barometrica accanto
            // a cui verra' confrontata.
            const auto down = mavlink::read_integer(definition, frame, "vz");
            if (down) {
                constexpr double feet_per_minute_per_metre_per_second = 196.850393700787;
                const double metres_per_second = static_cast<double>(*down) / 100.0;
                snapshot_.position.gnss_vertical_speed_fpm = measured(
                    -metres_per_second * feet_per_minute_per_metre_per_second,
                    static_cast<double>(*down), SignalId::gnss_vertical_speed,
                    Unit::foot_per_minute,
                    gnss_fix_type_ >= gnss_fix_3d ? Validity::valid
                                                  : Validity::unavailable);
            }
            ++statistics_.frames_applied;
            break;
        }

        case 295: {  // AIRSPEED
            const auto pressure = mavlink::read_number(definition, frame, "raw_press");
            const auto temperature = mavlink::read_integer(definition, frame, "temperature");
            if (pressure) {
                // La pressione differenziale e' la grandezza che il sensore
                // misura davvero, e resta valida anche quando la velocita' che
                // se ne ricava non lo e': al banco e' esattamente il dato che
                // serve confrontare con un riferimento.
                air_data_.differential_pressure_hpa =
                    measured(*pressure, *pressure, SignalId::differential_pressure,
                             Unit::hectopascal, Validity::valid);
            }
            if (temperature) {
                air_data_.element_temperature_c =
                    measured(static_cast<double>(*temperature) / 100.0,
                             static_cast<double>(*temperature),
                             SignalId::air_data_temperature, Unit::celsius,
                             Validity::valid);
            }
            ++statistics_.frames_applied;
            break;
        }

        case 1: {  // SYS_STATUS
            const auto present =
                mavlink::read_unsigned(definition, frame, "onboard_control_sensors_present");
            const auto healthy =
                mavlink::read_unsigned(definition, frame, "onboard_control_sensors_health");
            if (present) {
                air_data_.declared_present =
                    (*present & sensor_bit_differential_pressure) != 0;
                air_data_.absolute_pressure_present =
                    (*present & sensor_bit_absolute_pressure) != 0;
            }
            if (healthy) {
                differential_pressure_healthy_ =
                    (*healthy & sensor_bit_differential_pressure) != 0;
                air_data_.declared_healthy = differential_pressure_healthy_;
                absolute_pressure_healthy_ =
                    (*healthy & sensor_bit_absolute_pressure) != 0;
                air_data_.absolute_pressure_healthy = absolute_pressure_healthy_;
            }
            revalidate();
            ++statistics_.frames_applied;
            break;
        }

        case 2: {  // SYSTEM_TIME
            const auto unix_us = mavlink::read_unsigned(definition, frame, "time_unix_usec");
            // Zero non e' l'istante zero: e' il modo in cui il controllo di volo
            // dichiara di non avere ancora una sorgente di tempo affidabile,
            // condizione normale prima del fix. Propagarlo come istante
            // significherebbe datare le registrazioni al 1970.
            utc_ = unix_us.value_or(utc_unavailable);
            ++statistics_.frames_applied;
            break;
        }

        case 253: {  // STATUSTEXT
            last_status_text_ = mavlink::read_text(definition, frame, "text");
            ++statistics_.frames_applied;
            break;
        }

        default:
            ++statistics_.frames_ignored;
            break;
    }
}

void MavlinkAdapter::revalidate() {
    // Lo stato del satellitare e quello del sensore differenziale arrivano in
    // messaggi diversi da quelli che portano i valori. Quando cambiano, i
    // campioni gia' pubblicati vanno rivisti: un campione diventato non
    // attendibile non puo' restare valido soltanto perche' era arrivato prima
    // della notizia.
    const Validity position = gnss_fix_type_ >= gnss_fix_3d ? Validity::valid
                                                            : Validity::unavailable;
    for (Sample<double>* sample : {&snapshot_.position.latitude_deg,
                                   &snapshot_.position.longitude_deg,
                                   &snapshot_.position.ground_speed_kt,
                                   &snapshot_.position.gnss_vertical_speed_fpm}) {
        if (sample->origin.signal != SignalId::unknown) sample->validity = position;
    }

    // L'autopilota dichiara sane le proprie stime. Quando dice di no, il valore
    // continua a essere mostrato ma marcato degradato, non rimosso: una stima
    // che l'autopilota giudica cattiva puo' essere ancora approssimativamente
    // giusta, e toglierla al pilota nel momento in cui gli serve sarebbe peggio
    // che mostrargliela dichiarando che non e' affidabile.
    //
    // Vale anche per il filtro non inizializzato, che pure sembrerebbe il caso
    // piu' grave. I bit descrivono lo stato dell'EKF, non quello del messaggio
    // di assetto: ArduPilot pubblica l'assetto anche quando l'EKF non e' pronto,
    // ricavandolo altrimenti. Dedurre da un bit dell'EKF che l'assetto non
    // esista sarebbe un'affermazione sugli interni dell'autopilota che qui non
    // e' verificata, e produrrebbe uno strumento vuoto dove ce n'e' uno incerto.
    // La condizione resta distinta e leggibile in EstimatorState::uninitialised.
    if (estimator_.reported) {
        const Validity attitude =
            estimator_.attitude_healthy ? Validity::valid : Validity::degraded;
        for (Sample<double>* sample : {&snapshot_.attitude.roll_deg,
                                       &snapshot_.attitude.pitch_deg,
                                       &snapshot_.attitude.heading_deg}) {
            if (sample->origin.signal != SignalId::unknown) sample->validity = attitude;
        }

        if (snapshot_.position.gnss_vertical_speed_fpm.origin.signal !=
                SignalId::unknown &&
            snapshot_.position.gnss_vertical_speed_fpm.validity == Validity::valid &&
            !estimator_.vertical_velocity_healthy) {
            snapshot_.position.gnss_vertical_speed_fpm.validity = Validity::degraded;
        }

        // Navigazione a stima e satellitare che salta: in entrambi i casi il fix
        // resta dichiarato valido mentre la posizione non descrive piu' dove si
        // trova l'aeromobile. Sono le sole notizie che lo rivelano.
        if (estimator_.dead_reckoning || estimator_.gnss_glitching ||
            !estimator_.horizontal_position_healthy) {
            for (Sample<double>* sample : {&snapshot_.position.latitude_deg,
                                           &snapshot_.position.longitude_deg}) {
                if (sample->origin.signal != SignalId::unknown &&
                    sample->validity == Validity::valid) {
                    sample->validity = Validity::degraded;
                }
            }
        }
    }

    const Validity altitude = absolute_pressure_healthy_ &&
            (!estimator_.reported || estimator_.vertical_position_healthy)
        ? Validity::valid : Validity::unavailable;
    if (snapshot_.air_data.pressure_altitude_ft.origin.signal != SignalId::unknown) {
        snapshot_.air_data.pressure_altitude_ft.validity = altitude;
    }

    const Validity vertical_speed = absolute_pressure_healthy_ &&
            (!estimator_.reported || estimator_.vertical_velocity_healthy)
        ? Validity::valid : Validity::unavailable;
    if (snapshot_.air_data.vertical_speed_fpm.origin.signal != SignalId::unknown) {
        snapshot_.air_data.vertical_speed_fpm.validity = vertical_speed;
    }

    const Validity airspeed = differential_pressure_healthy_ ? Validity::valid
                                                             : Validity::unavailable;
    if (air_data_.computed_airspeed_kt.origin.signal != SignalId::unknown) {
        air_data_.computed_airspeed_kt.validity = airspeed;
        snapshot_.air_data.indicated_airspeed_kt.validity = airspeed;
    }
}

}  // namespace efis
