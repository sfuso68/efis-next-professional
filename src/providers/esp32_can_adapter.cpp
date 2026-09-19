#include "efis/providers/esp32_can_adapter.hpp"

namespace efis {
namespace {

// Il nodo dichiara il barometro in hPa con un offset di 1000, per far stare il
// valore in un intero con segno. Dimenticarlo produce una pressione di circa
// 13 hPa, che si riconosce, oppure di 1013 quando il grezzo e' vicino a zero,
// che non si riconosce affatto.
constexpr double pressure_offset_hpa = 1000.0;

constexpr double feet_per_meter = 3.280839895;

// Calibrazione BNO055: 0 non calibrato, 3 pienamente calibrato.
constexpr std::uint8_t calibration_full = 3;

[[nodiscard]] Validity from_calibration(bool sensor_ok, std::uint8_t calibration) noexcept {
    if (!sensor_ok) return Validity::unavailable;
    // Una fusione non calibrata produce numeri plausibili e privi di
    // significato. E' il caso che il paragrafo 22 dell'handoff vieta di
    // presentare come misura, e va escluso invece che attenuato.
    if (calibration == 0) return Validity::unavailable;
    return calibration >= calibration_full ? Validity::valid : Validity::degraded;
}

Sample<double> measured(double value, double raw, SignalId signal, Unit unit,
                        NodeId node, MonoTime now, std::uint64_t sequence) {
    Sample<double> sample{};
    sample.value = value;
    sample.raw = raw;
    sample.source = SourceId::esp32_legacy;
    sample.sampled_at = now;
    sample.received_at = now;
    sample.quality = Quality::nominal;
    // La validita' definitiva la decide revalidate(), che conosce lo stato
    // dichiarato dal nodo. Qui il campione nasce valido soltanto in quanto
    // ricevuto, e non e' la stessa cosa.
    sample.validity = Validity::valid;
    sample.sequence = sequence;
    sample.origin.signal = signal;
    sample.origin.unit = unit;
    sample.origin.node = node;
    return sample;
}

}  // namespace

Esp32CanAdapter::Esp32CanAdapter(NodeId node, NodeHealthPolicy policy)
    : node_(node), health_(policy) {
    health_.declare(node_);
}

void Esp32CanAdapter::collect(std::vector<NodeStateChange> changes) {
    node_events_.insert(node_events_.end(), changes.begin(), changes.end());
}

void Esp32CanAdapter::consume(std::span<const can::Frame> frames, MonoTime now) {
    for (const can::Frame& frame : frames) consume(frame, now);
}

void Esp32CanAdapter::consume(const can::Frame& frame, MonoTime now) {
    // Un payload piu' corto del previsto non viene completato con zeri. Il
    // protocollo dichiara otto byte per ogni messaggio: se ne arrivano meno, o
    // il bus ha corrotto il frame o il nodo parla una versione diversa, e in
    // entrambi i casi i byte mancanti non valgono zero.
    const auto require_full = [&]() {
        if (frame.length >= 8) return true;
        ++statistics_.frames_truncated;
        return false;
    };

    switch (frame.id) {
        case esp32_can::id_attitude:
            if (!require_full()) return;
            apply_attitude(frame, now);
            break;
        case esp32_can::id_air_data:
            if (!require_full()) return;
            apply_air_data(frame, now);
            break;
        case esp32_can::id_health:
            if (!require_full()) return;
            apply_health(frame, now);
            break;
        case esp32_can::id_diagnostics:
            if (!require_full()) return;
            apply_diagnostics(frame);
            break;
        case esp32_can::id_gps_position:
            if (!require_full()) return;
            apply_gps_position(frame, now);
            break;
        case esp32_can::id_gps_velocity:
            if (!require_full()) return;
            apply_gps_velocity(frame, now);
            break;
        case esp32_can::id_gps_status:
            if (!require_full()) return;
            apply_gps_status(frame);
            break;
        default:
            // Compresi i comandi 0x018 diretti al nodo, che non ci riguardano.
            ++statistics_.frames_ignored;
            return;
    }

    ++statistics_.frames_applied;
    snapshot_.timestamp = now;
    snapshot_.sequence = sequence_;
    revalidate();
}

void Esp32CanAdapter::apply_attitude(const can::Frame& frame, MonoTime now) {
    // Attenzione all'ordine: il nodo trasmette beccheggio prima di rollio, al
    // contrario di quasi ogni altra rappresentazione del progetto. Scambiarli
    // produrrebbe due numeri entrambi plausibili, che e' il modo peggiore di
    // sbagliare.
    const double pitch = can::read_i16(frame, 0).value() / 100.0;
    const double roll = can::read_i16(frame, 2).value() / 100.0;
    const double heading = can::read_i16(frame, 4).value() / 100.0;

    const std::uint8_t flags = frame.data[6];
    state_.imu_ok = (flags & 0x01) != 0;
    state_.barometer_ok = (flags & 0x02) != 0;
    state_.pitot_ok = (flags & 0x04) != 0;

    const std::uint8_t calibration = frame.data[7];
    state_.calibration_system = static_cast<std::uint8_t>(calibration & 0x03);
    state_.calibration_magnetometer = static_cast<std::uint8_t>((calibration >> 2) & 0x03);
    state_.calibration_gyroscope = static_cast<std::uint8_t>((calibration >> 4) & 0x03);
    state_.calibration_accelerometer = static_cast<std::uint8_t>((calibration >> 6) & 0x03);

    snapshot_.attitude.pitch_deg = measured(pitch, can::read_i16(frame, 0).value(),
                                            SignalId::pitch, Unit::degree, node_,
                                            now, sequence_);
    snapshot_.attitude.roll_deg = measured(roll, can::read_i16(frame, 2).value(),
                                           SignalId::roll, Unit::degree, node_,
                                           now, sequence_);
    snapshot_.attitude.heading_deg = measured(heading, can::read_i16(frame, 4).value(),
                                              SignalId::heading, Unit::degree, node_,
                                              now, sequence_);
}

void Esp32CanAdapter::apply_air_data(const can::Frame& frame, MonoTime now) {
    const double airspeed = can::read_i16(frame, 0).value() / 10.0;
    const double altitude = can::read_i16(frame, 2).value();
    const double pressure = can::read_i16(frame, 4).value() / 10.0 + pressure_offset_hpa;
    const double differential = can::read_i16(frame, 6).value() / 10.0;

    snapshot_.air_data.indicated_airspeed_kt =
        measured(airspeed, can::read_i16(frame, 0).value(), SignalId::indicated_airspeed,
                 Unit::knot, node_, now, sequence_);
    snapshot_.air_data.pressure_altitude_ft =
        measured(altitude, can::read_i16(frame, 2).value(), SignalId::pressure_altitude,
                 Unit::foot, node_, now, sequence_);

    state_.static_pressure_hpa =
        measured(pressure, can::read_i16(frame, 4).value(), SignalId::unknown,
                 Unit::hectopascal, node_, now, sequence_);
    state_.differential_pressure_pa =
        measured(differential, can::read_i16(frame, 6).value(), SignalId::differential_pressure,
                 Unit::pascal, node_, now, sequence_);
}

void Esp32CanAdapter::apply_health(const can::Frame& frame, MonoTime now) {
    const std::uint16_t heartbeat = can::read_u16(frame, 0).value();

    // Il contatore e' a 16 bit e avvolge. La sorveglianza dei nodi lavora su una
    // sequenza monotona, quindi l'avvolgimento va srotolato qui: passarlo cosi'
    // com'e' farebbe apparire una regressione, e il monitor scarterebbe il
    // battito senza rinviare il timeout, dichiarando perduto un nodo sano.
    if (heartbeat_seen_ && heartbeat < last_heartbeat_) {
        ++statistics_.heartbeat_wraps;
        sequence_ += 0x10000ULL;
    }
    heartbeat_seen_ = true;
    last_heartbeat_ = heartbeat;
    const std::uint64_t unwrapped =
        (sequence_ & ~0xFFFFULL) | static_cast<std::uint64_t>(heartbeat);
    sequence_ = unwrapped;

    health_.heartbeat(node_, now, sequence_);
    collect(health_.update(now));

    state_.load_factor_g = measured(can::read_i16(frame, 2).value() / 100.0,
                                    can::read_i16(frame, 2).value(), SignalId::unknown,
                                    Unit::none, node_, now, sequence_);
    state_.temperature_c = measured(can::read_i16(frame, 4).value() / 10.0,
                                    can::read_i16(frame, 4).value(),
                                    SignalId::air_data_temperature, Unit::celsius,
                                    node_, now, sequence_);
}

void Esp32CanAdapter::apply_diagnostics(const can::Frame& frame) {
    state_.firmware_major = frame.data[0];
    state_.firmware_minor = frame.data[1];
    state_.reset_reason = frame.data[2];
    state_.boot_count = frame.data[3];
    state_.crash_count = frame.data[4];
    state_.free_heap_kb = can::read_u16(frame, 5).value();
    state_.diagnostics_received = true;
}

void Esp32CanAdapter::apply_gps_position(const can::Frame& frame, MonoTime now) {
    snapshot_.position.latitude_deg =
        measured(can::read_i32(frame, 0).value() / 1e7, can::read_i32(frame, 0).value(),
                 SignalId::latitude, Unit::degree, node_, now, sequence_);
    snapshot_.position.longitude_deg =
        measured(can::read_i32(frame, 4).value() / 1e7, can::read_i32(frame, 4).value(),
                 SignalId::longitude, Unit::degree, node_, now, sequence_);
}

void Esp32CanAdapter::apply_gps_velocity(const can::Frame& frame, MonoTime now) {
    snapshot_.position.ground_speed_kt =
        measured(can::read_i16(frame, 0).value() / 10.0, can::read_i16(frame, 0).value(),
                 SignalId::ground_speed, Unit::knot, node_, now, sequence_);
    // La quota satellitare arriva in metri. Viene convertita in piedi, che e'
    // l'unita' del resto del sistema, e tenuta fuori dallo snapshot di volo: la
    // quota di volo e' quella barometrica, e confondere le due renderebbe
    // inutile il confronto fra loro.
    state_.gnss_altitude_ft =
        measured(can::read_i16(frame, 4).value() * feet_per_meter, can::read_i16(frame, 4).value(),
                 SignalId::unknown, Unit::foot, node_, now, sequence_);
    state_.gnss_track_deg =
        measured(can::read_i16(frame, 2).value() / 10.0, can::read_i16(frame, 2).value(),
                 SignalId::unknown, Unit::degree, node_, now, sequence_);
}

void Esp32CanAdapter::apply_gps_status(const can::Frame& frame) {
    state_.gnss_fix = frame.data[0] != 0;
    state_.gnss_satellites = frame.data[1];
    state_.gnss_hdop = can::read_i16(frame, 2).value() / 10.0;
}

void Esp32CanAdapter::revalidate() {
    // Assetto. Rollio e beccheggio dipendono dalla calibrazione di sistema, la
    // prua anche da quella magnetica: sul BNO055 sono cose diverse, e trattarle
    // insieme significherebbe o scartare un assetto buono o presentare una prua
    // che non lo e'.
    const Validity attitude = from_calibration(state_.imu_ok, state_.calibration_system);
    for (Sample<double>* sample : {&snapshot_.attitude.roll_deg,
                                   &snapshot_.attitude.pitch_deg}) {
        if (sample->origin.signal != SignalId::unknown) sample->validity = attitude;
    }
    if (snapshot_.attitude.heading_deg.origin.signal != SignalId::unknown) {
        const Validity magnetic =
            from_calibration(state_.imu_ok, state_.calibration_magnetometer);
        snapshot_.attitude.heading_deg.validity =
            attitude == Validity::unavailable ? Validity::unavailable : magnetic;
    }

    const Validity barometric =
        state_.barometer_ok ? Validity::valid : Validity::unavailable;
    if (snapshot_.air_data.pressure_altitude_ft.origin.signal != SignalId::unknown) {
        snapshot_.air_data.pressure_altitude_ft.validity = barometric;
    }
    if (state_.static_pressure_hpa.origin.unit != Unit::unknown) {
        state_.static_pressure_hpa.validity = barometric;
    }

    const Validity pitot = state_.pitot_ok ? Validity::valid : Validity::unavailable;
    if (snapshot_.air_data.indicated_airspeed_kt.origin.signal != SignalId::unknown) {
        snapshot_.air_data.indicated_airspeed_kt.validity = pitot;
    }
    if (state_.differential_pressure_pa.origin.signal != SignalId::unknown) {
        state_.differential_pressure_pa.validity = pitot;
    }

    // Il fix e' booleano: da questo protocollo non si distingue un fix
    // bidimensionale da uno tridimensionale, ed e' una perdita di informazione
    // rispetto al satellitare del Pixhawk. Senza fix nulla di posizionale e'
    // utilizzabile.
    const Validity position = state_.gnss_fix ? Validity::valid : Validity::unavailable;
    for (Sample<double>* sample : {&snapshot_.position.latitude_deg,
                                   &snapshot_.position.longitude_deg,
                                   &snapshot_.position.ground_speed_kt}) {
        if (sample->origin.signal != SignalId::unknown) sample->validity = position;
    }
}

void Esp32CanAdapter::update(MonoTime now) {
    collect(health_.update(now));

    const auto status = health_.status(node_);
    if (!status.has_value() || is_usable(status->state)) return;

    // Nodo scaduto o perduto: i valori che ha lasciato non sono piu' attuali.
    // Lasciarli validi significherebbe mostrare l'ultimo assetto buono di un
    // sensore che non parla piu'.
    // Tutte le grandezze, comprese quelle che non stanno nello snapshot di volo.
    // Fermarsi allo snapshot lascerebbe valide proprio le misure che il banco
    // mostra per prime, come la pressione differenziale del sensore aria.
    for (Sample<double>* sample : {&snapshot_.attitude.roll_deg,
                                   &snapshot_.attitude.pitch_deg,
                                   &snapshot_.attitude.heading_deg,
                                   &snapshot_.air_data.indicated_airspeed_kt,
                                   &snapshot_.air_data.pressure_altitude_ft,
                                   &snapshot_.position.latitude_deg,
                                   &snapshot_.position.longitude_deg,
                                   &snapshot_.position.ground_speed_kt,
                                   &state_.load_factor_g,
                                   &state_.temperature_c,
                                   &state_.differential_pressure_pa,
                                   &state_.static_pressure_hpa,
                                   &state_.gnss_altitude_ft,
                                   &state_.gnss_track_deg}) {
        if (sample->received_at != MonoTime{}) {
            sample->validity = Validity::unavailable;
        }
    }
}

}  // namespace efis
