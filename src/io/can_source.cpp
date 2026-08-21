#include "efis/io/can_source.hpp"

#include <algorithm>
#include <istream>
#include <string>

namespace efis::io {
namespace {

// Nel formato candump l'identificativo e' esadecimale: tre cifre per la forma a
// 11 bit, otto per quella a 29. La lunghezza distingue le due, ed e' l'unico
// modo di distinguerle, perche' il testo non porta un flag.
constexpr std::size_t extended_id_digits = 8;

[[nodiscard]] bool hex_value(char digit, std::uint8_t& out) noexcept {
    if (digit >= '0' && digit <= '9') { out = static_cast<std::uint8_t>(digit - '0'); return true; }
    if (digit >= 'a' && digit <= 'f') { out = static_cast<std::uint8_t>(digit - 'a' + 10); return true; }
    if (digit >= 'A' && digit <= 'F') { out = static_cast<std::uint8_t>(digit - 'A' + 10); return true; }
    return false;
}

[[nodiscard]] bool parse_hex(std::string_view text, std::uint32_t& out) noexcept {
    if (text.empty() || text.size() > 8) return false;
    std::uint32_t value = 0;
    for (const char digit : text) {
        std::uint8_t nibble = 0;
        if (!hex_value(digit, nibble)) return false;
        value = (value << 4) | nibble;
    }
    out = value;
    return true;
}

}  // namespace

RecordedCanSource::RecordedCanSource(std::vector<can::Frame> frames)
    : frames_(std::move(frames)) {}

std::size_t RecordedCanSource::read(std::span<can::Frame> frames) {
    const std::size_t available = frames_.size() - position_;
    const std::size_t count = std::min(available, frames.size());
    std::copy_n(frames_.begin() + static_cast<std::ptrdiff_t>(position_), count,
                frames.begin());
    position_ += count;
    return count;
}

bool RecordedCanSource::exhausted() const noexcept {
    return position_ >= frames_.size();
}

CandumpLog parse_candump(std::istream& input) {
    CandumpLog log{};
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // (timestamp) interfaccia identificativo#dati
        const std::size_t close = line.find(')');
        const std::size_t hash = line.find('#');
        if (line.front() != '(' || close == std::string::npos ||
            hash == std::string::npos || hash < close) {
            ++log.malformed_lines;
            continue;
        }

        const std::size_t interface_start = line.find_first_not_of(' ', close + 1);
        if (interface_start == std::string::npos) {
            ++log.malformed_lines;
            continue;
        }
        const std::size_t interface_end = line.find(' ', interface_start);
        if (interface_end == std::string::npos || interface_end > hash) {
            ++log.malformed_lines;
            continue;
        }

        const std::size_t id_start = line.find_first_not_of(' ', interface_end);
        if (id_start == std::string::npos || id_start >= hash) {
            ++log.malformed_lines;
            continue;
        }
        const std::string_view id_text{line.data() + id_start, hash - id_start};

        // Richiesta remota: il payload non esiste, la lettera R sta al posto dei
        // dati. Non porta misure.
        const std::string_view payload_text{line.data() + hash + 1,
                                            line.size() - hash - 1};
        if (!payload_text.empty() && (payload_text.front() == 'R' ||
                                      payload_text.front() == 'r')) {
            ++log.remote_frames;
            continue;
        }

        std::uint32_t id = 0;
        if (!parse_hex(id_text, id)) {
            ++log.malformed_lines;
            continue;
        }

        // Frame di errore. candump li stampa con l'identificativo a otto cifre e
        // il bit alto acceso: sono uno stato del bus, non una grandezza fisica.
        if (id_text.size() >= extended_id_digits && (id & 0x20000000U) != 0) {
            ++log.error_frames;
            continue;
        }

        if (payload_text.size() % 2 != 0 || payload_text.size() > 16) {
            ++log.malformed_lines;
            continue;
        }

        can::Frame frame{};
        frame.id = id;
        frame.length = static_cast<std::uint8_t>(payload_text.size() / 2);

        bool valid = true;
        for (std::uint8_t index = 0; index < frame.length; ++index) {
            std::uint8_t high = 0;
            std::uint8_t low = 0;
            if (!hex_value(payload_text[index * 2U], high) ||
                !hex_value(payload_text[index * 2U + 1U], low)) {
                valid = false;
                break;
            }
            frame.data[index] = static_cast<std::uint8_t>((high << 4) | low);
        }
        if (!valid) {
            ++log.malformed_lines;
            continue;
        }

        log.frames.push_back(frame);
    }

    return log;
}

}  // namespace efis::io
