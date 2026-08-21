#include "efis/providers/engine_can_adapter.hpp"

#include <algorithm>

namespace efis {
namespace {

std::uint16_t load_u16(const io::CanFrame& frame, std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(frame.data[offset]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(frame.data[offset + 1]) << 8));
}

std::int16_t load_i16(const io::CanFrame& frame, std::size_t offset) noexcept {
    return static_cast<std::int16_t>(load_u16(frame, offset));
}

// Lunghezza attesa di ogni messaggio. Un frame piu' corto non e' un frame con
// gli zeri tolti: in Engine CAN V1 la lunghezza e' fissa.
std::size_t expected_length(std::uint8_t message) noexcept {
    switch (message) {
        case engine_can::engine_primary: return 7;
        case engine_can::engine_fluids: return 7;
        case engine_can::engine_fuel: return 5;
        case engine_can::engine_electrical: return 5;
        case engine_can::cht_1_2:
        case engine_can::cht_3_4:
        case engine_can::egt_1_2:
        case engine_can::egt_3_4: return 5;
        case engine_can::node_heartbeat: return 6;
        default: return 0;
    }
}

}  // namespace

EngineCanAdapter::EngineCanAdapter(NodeHealthPolicy policy) : health_(policy) {}

void EngineCanAdapter::consume(io::CanSource& source, MonoTime now) {
    while (auto frame = source.next()) apply(*frame, now);
    snapshot_.timestamp = now;
}

void EngineCanAdapter::update(MonoTime now) {
    auto changes = health_.update(now);
    node_events_.insert(node_events_.end(), changes.begin(), changes.end());
}

std::uint64_t EngineCanAdapter::track(const io::CanFrame& frame) {
    // Ogni messaggio ha un contatore proprio: il nodo lo incrementa a ogni
    // trasmissione dello stesso messaggio, e i periodi sono diversi. Tenerne uno
    // solo per nodo produrrebbe regressioni a ogni frame.
    const auto key = frame.id;
    const std::uint8_t sequence = frame.data[0];

    auto it = std::find_if(sequences_.begin(), sequences_.end(),
                           [key](const SequenceTracker& t) { return t.key == key; });
    if (it == sequences_.end()) {
        sequences_.push_back(SequenceTracker{key, sequence, 0});
        return sequence;
    }

    if (sequence < it->last) {
        it->extended += 256;
        ++statistics_.sequence_wraps;
    }
    it->last = sequence;
    return it->extended + sequence;
}

void EngineCanAdapter::apply(const io::CanFrame& frame, MonoTime now) {
    const std::uint8_t node = frame.node();
    if (node == 0) {
        // Lo zero significa "non dichiarato" in tutto il progetto. Un nodo che
        // lo usa e' mal configurato, e va contato invece che accolto: accoglierlo
        // significherebbe attribuire misure a un'identita' che non esiste.
        ++statistics_.frames_undeclared_node;
        return;
    }

    const std::uint8_t message = frame.message();
    const std::size_t expected = expected_length(message);
    if (expected == 0) {
        ++statistics_.frames_ignored;
        return;
    }
    if (frame.length != expected) {
        ++statistics_.frames_wrong_length;
        return;
    }

    const auto sequence = track(frame);
    health_.heartbeat(node, now, sequence);

    // Ogni campione dichiara da dove viene e in quale unita' e'. Il valore
    // trasmesso resta conservato come grezzo: rileggendo una registrazione la
    // conversione si puo' rifare con una taratura diversa senza ripetere la
    // prova.
    auto measured = [&](std::int32_t raw, double scale, SignalId signal, Unit unit,
                        bool available) {
        Sample<double> sample{};
        sample.raw = static_cast<double>(raw);
        sample.value = static_cast<double>(raw) * scale;
        sample.source = SourceId::esp32_legacy;
        sample.sampled_at = now;
        sample.received_at = now;
        sample.quality = Quality::nominal;
        sample.validity = available ? Validity::valid : Validity::unavailable;
        sample.sequence = sequence;
        sample.origin.signal = signal;
        sample.origin.unit = unit;
        sample.origin.node = node;
        sample.origin.channel = message;
        sample.origin.simulated = false;
        if (!available) ++statistics_.unavailable_readings;
        return sample;
    };

    auto from_u16 = [&](std::size_t offset, double scale, SignalId signal, Unit unit) {
        const std::uint16_t raw = load_u16(frame, offset);
        return measured(raw, scale, signal, unit,
                        raw != engine_can::unsigned_unavailable);
    };
    auto from_i16 = [&](std::size_t offset, double scale, SignalId signal, Unit unit) {
        const std::int16_t raw = load_i16(frame, offset);
        return measured(raw, scale, signal, unit,
                        raw != engine_can::signed_unavailable);
    };

    switch (message) {
        case engine_can::engine_primary:
            snapshot_.rpm = from_u16(1, 1.0, SignalId::engine_rpm,
                                     Unit::revolutions_per_minute);
            snapshot_.manifold_pressure_inhg =
                from_u16(3, 0.01, SignalId::manifold_pressure, Unit::inch_mercury);
            snapshot_.oil_pressure_psi =
                from_u16(5, 0.1, SignalId::oil_pressure, Unit::pound_per_square_inch);
            break;

        case engine_can::engine_fluids:
            snapshot_.oil_temperature_c =
                from_i16(1, 0.1, SignalId::oil_temperature, Unit::celsius);
            snapshot_.fuel_pressure_psi =
                from_u16(3, 0.01, SignalId::fuel_pressure, Unit::pound_per_square_inch);
            snapshot_.fuel_flow_gph =
                from_u16(5, 0.01, SignalId::fuel_flow, Unit::gallon_per_hour);
            break;

        case engine_can::engine_fuel:
            snapshot_.fuel_left_gal =
                from_u16(1, 0.01, SignalId::fuel_quantity_left, Unit::gallon);
            snapshot_.fuel_right_gal =
                from_u16(3, 0.01, SignalId::fuel_quantity_right, Unit::gallon);
            break;

        case engine_can::engine_electrical:
            snapshot_.bus_voltage_v =
                from_u16(1, 0.01, SignalId::bus_voltage, Unit::volt);
            snapshot_.alternator_current_a =
                from_i16(3, 0.01, SignalId::alternator_current, Unit::ampere);
            break;

        case engine_can::cht_1_2:
            snapshot_.cht_c[0] = from_i16(1, 0.1,
                SignalId::cylinder_head_temperature_1, Unit::celsius);
            snapshot_.cht_c[1] = from_i16(3, 0.1,
                SignalId::cylinder_head_temperature_2, Unit::celsius);
            break;

        case engine_can::cht_3_4:
            snapshot_.cht_c[2] = from_i16(1, 0.1,
                SignalId::cylinder_head_temperature_3, Unit::celsius);
            snapshot_.cht_c[3] = from_i16(3, 0.1,
                SignalId::cylinder_head_temperature_4, Unit::celsius);
            break;

        case engine_can::egt_1_2:
            snapshot_.egt_c[0] = from_i16(1, 0.1,
                SignalId::exhaust_gas_temperature_1, Unit::celsius);
            snapshot_.egt_c[1] = from_i16(3, 0.1,
                SignalId::exhaust_gas_temperature_2, Unit::celsius);
            break;

        case engine_can::egt_3_4:
            snapshot_.egt_c[2] = from_i16(1, 0.1,
                SignalId::exhaust_gas_temperature_3, Unit::celsius);
            snapshot_.egt_c[3] = from_i16(3, 0.1,
                SignalId::exhaust_gas_temperature_4, Unit::celsius);
            break;

        case engine_can::node_heartbeat: {
            const std::uint8_t flags = frame.data[5];
            node_status_.seen = true;
            node_status_.uptime_s =
                static_cast<std::uint32_t>(frame.data[1]) |
                (static_cast<std::uint32_t>(frame.data[2]) << 8) |
                (static_cast<std::uint32_t>(frame.data[3]) << 16) |
                (static_cast<std::uint32_t>(frame.data[4]) << 24);
            node_status_.calibration_valid =
                (flags & engine_can::flag_calibration_valid) != 0;
            node_status_.probe_failure =
                (flags & engine_can::flag_probe_failure) != 0;
            node_status_.supply_out_of_tolerance =
                (flags & engine_can::flag_supply_out_of_tolerance) != 0;
            node_status_.recently_restarted =
                (flags & engine_can::flag_recently_restarted) != 0;
            break;
        }

        default:
            ++statistics_.frames_ignored;
            return;
    }

    ++snapshot_.sequence;
    ++statistics_.frames_applied;
}

}  // namespace efis
