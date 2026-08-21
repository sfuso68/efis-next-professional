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
                snapshot_.air_data.pressure_altitude_ft =
                    measured(*altitude * feet_per_meter, *altitude,
                             SignalId::pressure_altitude, Unit::foot, Validity::valid);
            }
            if (climb) {
                snapshot_.air_data.vertical_speed_fpm =
                    measured(*climb * feet_per_minute_per_meter_per_second, *climb,
                             SignalId::vertical_speed, Unit::foot_per_minute,
                             Validity::valid);
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
            }
            if (healthy) {
                differential_pressure_healthy_ =
                    (*healthy & sensor_bit_differential_pressure) != 0;
                air_data_.declared_healthy = differential_pressure_healthy_;
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
                                   &snapshot_.position.ground_speed_kt}) {
        if (sample->origin.signal != SignalId::unknown) sample->validity = position;
    }

    const Validity airspeed = differential_pressure_healthy_ ? Validity::valid
                                                             : Validity::unavailable;
    if (air_data_.computed_airspeed_kt.origin.signal != SignalId::unknown) {
        air_data_.computed_airspeed_kt.validity = airspeed;
        snapshot_.air_data.indicated_airspeed_kt.validity = airspeed;
    }
}

}  // namespace efis
