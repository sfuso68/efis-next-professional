#include "efis/recording/flight_record.hpp"

#include <array>
#include <charconv>
#include <chrono>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

namespace efis::recording {
namespace {

std::int64_t micros(MonoTime value) {
    return std::chrono::duration_cast<std::chrono::microseconds>(
               value.time_since_epoch()).count();
}

MonoTime from_micros(std::int64_t value) {
    return MonoTime{std::chrono::microseconds{value}};
}

// Rappresentazione piu' corta che rilegge esattamente lo stesso double.
//
// La formattazione predefinita di ostream ne scrive sei cifre significative, e
// quel che eccede viene perduto senza che nulla lo dichiari. In una scatola nera
// e' inaccettabile per due motivi: il valore registrato non e' piu' quello
// misurato, e un confronto fra registrazione e volo mostra differenze che non
// esistono. to_chars sceglie la forma piu' breve che si rilegge identica.
std::string number(double value) {
    std::array<char, 64> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    if (result.ec != std::errc{}) return "0";
    return std::string(buffer.data(), result.ptr);
}

template <class T>
int code(T value) {
    return static_cast<int>(value);
}

// --- Lettura -----------------------------------------------------------------

bool parse_int(std::string_view text, std::int64_t& out) {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool parse_double(std::string_view text, double& out) {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

std::vector<std::string_view> split(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = line.find(',', start);
        if (comma == std::string_view::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, comma - start));
        start = comma + 1;
    }
    return fields;
}

// Converte solo se il valore ricade fra gli enumeratori dichiarati. Un codice
// fuori intervallo proviene da una registrazione corrotta o da una versione che
// non conosciamo: accettarlo produrrebbe uno stato che il programma non prevede.
template <class Enum>
bool to_enum(std::int64_t value, std::int64_t limit, Enum& out) {
    if (value < 0 || value > limit) return false;
    out = static_cast<Enum>(value);
    return true;
}

constexpr std::int64_t last_source = static_cast<std::int64_t>(SourceId::nav_synthetic);
constexpr std::int64_t last_quality = static_cast<std::int64_t>(Quality::high);
constexpr std::int64_t last_validity = static_cast<std::int64_t>(Validity::degraded);
constexpr std::int64_t last_unit = static_cast<std::int64_t>(Unit::hectopascal);
constexpr std::int64_t last_node_state = static_cast<std::int64_t>(NodeState::lost);
constexpr std::int64_t last_reason = static_cast<std::int64_t>(SourceChangeReason::reversion);

bool read_sample_row(const std::vector<std::string_view>& f, RecordedSample& out) {
    if (f.size() != 15) return false;
    std::int64_t mono{}, utc{}, signal{}, node{}, channel{}, unit{};
    std::int64_t source{}, validity{}, quality{}, simulated{}, calibration{}, sequence{};
    double raw{}, value{};
    if (!parse_int(f[1], mono) || !parse_int(f[2], utc) || !parse_int(f[3], signal) ||
        !parse_int(f[4], node) || !parse_int(f[5], channel) || !parse_int(f[6], unit) ||
        !parse_int(f[7], source) || !parse_int(f[8], validity) ||
        !parse_int(f[9], quality) || !parse_int(f[10], simulated) ||
        !parse_int(f[11], calibration) || !parse_int(f[12], sequence) ||
        !parse_double(f[13], raw) || !parse_double(f[14], value)) {
        return false;
    }
    if (utc < 0 || node < 0 || channel < 0 || calibration < 0 || sequence < 0 ||
        signal < 0 || node > 0xFFFF || channel > 0xFFFF) {
        return false;
    }

    Sample<double> sample{};
    if (!to_enum(source, last_source, sample.source) ||
        !to_enum(validity, last_validity, sample.validity) ||
        !to_enum(quality, last_quality, sample.quality) ||
        !to_enum(unit, last_unit, sample.origin.unit)) {
        return false;
    }
    sample.origin.signal = static_cast<SignalId>(signal);
    sample.origin.node = static_cast<NodeId>(node);
    sample.origin.channel = static_cast<ChannelId>(channel);
    sample.origin.calibration_revision = static_cast<std::uint32_t>(calibration);
    sample.origin.simulated = simulated != 0;
    sample.sequence = static_cast<std::uint64_t>(sequence);
    sample.raw = raw;
    sample.value = value;
    sample.utc = static_cast<UtcMicros>(utc);
    sample.sampled_at = from_micros(mono);
    sample.received_at = sample.sampled_at;

    out.timestamp = sample.sampled_at;
    out.sample = sample;
    return true;
}

bool read_node_row(const std::vector<std::string_view>& f, RecordedNodeEvent& out) {
    if (f.size() != 5) return false;
    std::int64_t mono{}, node{}, previous{}, current{};
    if (!parse_int(f[1], mono) || !parse_int(f[2], node) ||
        !parse_int(f[3], previous) || !parse_int(f[4], current)) {
        return false;
    }
    if (node < 0 || node > 0xFFFF) return false;
    if (!to_enum(previous, last_node_state, out.previous) ||
        !to_enum(current, last_node_state, out.current)) {
        return false;
    }
    out.timestamp = from_micros(mono);
    out.node = static_cast<NodeId>(node);
    return true;
}

bool read_source_row(const std::vector<std::string_view>& f, SourceChangeEvent& out) {
    if (f.size() != 5) return false;
    std::int64_t mono{}, previous{}, selected{}, reason{};
    if (!parse_int(f[1], mono) || !parse_int(f[2], previous) ||
        !parse_int(f[3], selected) || !parse_int(f[4], reason)) {
        return false;
    }
    if (!to_enum(previous, last_source, out.previous) ||
        !to_enum(selected, last_source, out.selected) ||
        !to_enum(reason, last_reason, out.reason)) {
        return false;
    }
    out.timestamp = from_micros(mono);
    return true;
}

}  // namespace

void FlightRecordWriter::write_header(std::ostream& out) {
    out << "EFIS_RECORD_V2\n";
    out << "schema," << flight_record_schema << '\n';
    out << "sample,mono_us,utc_us,signal,node,channel,unit,source,validity,"
           "quality,simulated,calibration,sequence,raw,value\n";
}

void FlightRecordWriter::write_sample(std::ostream& out, const Sample<double>& s,
                                      MonoTime timestamp) {
    out << "sample," << micros(timestamp) << ',' << s.utc << ','
        << code(s.origin.signal) << ',' << s.origin.node << ',' << s.origin.channel
        << ',' << code(s.origin.unit) << ',' << code(s.source) << ','
        << code(s.validity) << ',' << code(s.quality) << ','
        << (s.origin.simulated ? 1 : 0) << ',' << s.origin.calibration_revision
        << ',' << s.sequence << ',' << number(s.raw) << ',' << number(s.value) << '\n';
}

void FlightRecordWriter::write_snapshot(std::ostream& out, const FlightSnapshot& s) {
    const auto at = s.timestamp;
    write_sample(out, s.attitude.roll_deg, at);
    write_sample(out, s.attitude.pitch_deg, at);
    write_sample(out, s.attitude.heading_deg, at);
    write_sample(out, s.air_data.indicated_airspeed_kt, at);
    write_sample(out, s.air_data.pressure_altitude_ft, at);
    write_sample(out, s.air_data.vertical_speed_fpm, at);
    write_sample(out, s.position.latitude_deg, at);
    write_sample(out, s.position.longitude_deg, at);
    write_sample(out, s.position.ground_speed_kt, at);
    write_sample(out, s.guidance.commanded_roll_deg, at);
    write_sample(out, s.guidance.commanded_pitch_deg, at);
    write_sample(out, s.guidance.selected_altitude_ft, at);
    write_sample(out, s.guidance.selected_course_deg, at);
    write_sample(out, s.guidance.selected_airspeed_kt, at);
    write_sample(out, s.navigation.cdi_deviation_dots, at);
    write_sample(out, s.navigation.glide_slope_dots, at);
}

void FlightRecordWriter::write_node_event(std::ostream& out, const NodeStateChange& e) {
    out << "node," << micros(e.timestamp) << ',' << e.node << ','
        << code(e.previous) << ',' << code(e.current) << '\n';
}

void FlightRecordWriter::write_source_change(std::ostream& out,
                                             const SourceChangeEvent& e) {
    out << "source," << micros(e.timestamp) << ',' << code(e.previous) << ','
        << code(e.selected) << ',' << code(e.reason) << '\n';
}

FlightRecord read_flight_record(std::istream& input) {
    FlightRecord record{};
    std::string line;

    if (!std::getline(input, line)) return record;
    if (line != "EFIS_RECORD_V2") {
        record.malformed_rows = 1;
        return record;
    }

    while (std::getline(input, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto fields = split(line);
        const std::string_view kind = fields.front();

        if (kind == "schema") {
            std::int64_t value{};
            if (fields.size() != 2 || !parse_int(fields[1], value) || value < 0) {
                ++record.malformed_rows;
                continue;
            }
            record.schema = static_cast<std::uint16_t>(value);
            if (value > flight_record_schema) {
                // Uno schema piu' recente puo' avere significati diversi per gli
                // stessi campi. Interpretarlo comunque produrrebbe numeri
                // plausibili e sbagliati, che e' il modo peggiore di fallire.
                record.schema_too_new = true;
                return record;
            }
            continue;
        }
        if (kind == "sample") {
            // L'intestazione delle colonne comincia anch'essa con "sample": si
            // distingue perche' i suoi campi non sono numeri.
            RecordedSample sample{};
            if (read_sample_row(fields, sample)) {
                record.samples.push_back(sample);
            } else if (fields.size() == 15 && fields[1] == "mono_us") {
                continue;
            } else {
                ++record.malformed_rows;
            }
            continue;
        }
        if (kind == "node") {
            RecordedNodeEvent event{};
            if (read_node_row(fields, event)) record.node_events.push_back(event);
            else ++record.malformed_rows;
            continue;
        }
        if (kind == "source") {
            SourceChangeEvent event{};
            if (read_source_row(fields, event)) record.source_changes.push_back(event);
            else ++record.malformed_rows;
            continue;
        }
        ++record.unknown_rows;
    }
    return record;
}

}  // namespace efis::recording
