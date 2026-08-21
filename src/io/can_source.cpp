#include "efis/io/can_source.hpp"

#include <charconv>
#include <fstream>

namespace efis::io {
namespace {

std::optional<std::uint8_t> hex_nibble(char character) noexcept {
    if (character >= '0' && character <= '9') {
        return static_cast<std::uint8_t>(character - '0');
    }
    if (character >= 'a' && character <= 'f') {
        return static_cast<std::uint8_t>(character - 'a' + 10);
    }
    if (character >= 'A' && character <= 'F') {
        return static_cast<std::uint8_t>(character - 'A' + 10);
    }
    return std::nullopt;
}

std::optional<std::uint16_t> hex_value(std::string_view text) noexcept {
    if (text.empty() || text.size() > 4) return std::nullopt;
    std::uint16_t value = 0;
    for (const char character : text) {
        const auto nibble = hex_nibble(character);
        if (!nibble.has_value()) return std::nullopt;
        value = static_cast<std::uint16_t>((value << 4) | *nibble);
    }
    return value;
}

}  // namespace

std::optional<CanFrame> parse_candump_line(std::string_view line) noexcept {
    // Il timestamp fra parentesi e il nome dell'interfaccia precedono il frame.
    // Nessuno dei due serve qui: il tempo di ricezione lo mette chi legge, e
    // l'interfaccia l'ha gia' scelta chi ha aperto la sorgente.
    const auto hash = line.find('#');
    if (hash == std::string_view::npos) return std::nullopt;

    // L'identificatore e' l'ultimo campo separato da spazio prima del cancelletto.
    const auto before = line.substr(0, hash);
    const auto space = before.find_last_of(" \t");
    const auto id_text =
        space == std::string_view::npos ? before : before.substr(space + 1);

    const auto id = hex_value(id_text);
    if (!id.has_value() || *id > 0x7FF) return std::nullopt;

    auto payload = line.substr(hash + 1);
    // Una riga puo' terminare con spazi o con il ritorno a capo di Windows.
    while (!payload.empty() && (payload.back() == '\r' || payload.back() == '\n' ||
                                payload.back() == ' ' || payload.back() == '\t')) {
        payload.remove_suffix(1);
    }
    // I frame remoti e quelli a lunghezza flessibile non appartengono a
    // Engine CAN V1: si scartano invece di interpretarli a meta'.
    if (!payload.empty() && (payload.front() == 'R' || payload.front() == '#')) {
        return std::nullopt;
    }
    if (payload.size() % 2 != 0 || payload.size() > 16) return std::nullopt;

    CanFrame frame{};
    frame.id = *id;
    frame.length = static_cast<std::uint8_t>(payload.size() / 2);
    for (std::size_t index = 0; index < frame.length; ++index) {
        const auto high = hex_nibble(payload[index * 2]);
        const auto low = hex_nibble(payload[index * 2 + 1]);
        if (!high.has_value() || !low.has_value()) return std::nullopt;
        frame.data[index] = static_cast<std::uint8_t>((*high << 4) | *low);
    }
    return frame;
}

RecordedCanSource::RecordedCanSource(const std::filesystem::path& path) {
    std::ifstream file{path};
    std::string line;
    while (std::getline(file, line)) lines_.push_back(line);
}

RecordedCanSource::RecordedCanSource(std::vector<std::string> lines)
    : lines_(std::move(lines)) {}

std::optional<CanFrame> RecordedCanSource::next() {
    while (position_ < lines_.size()) {
        const auto& line = lines_[position_++];
        if (line.empty()) continue;
        if (auto frame = parse_candump_line(line)) return frame;
        ++malformed_;
    }
    return std::nullopt;
}

bool RecordedCanSource::exhausted() const noexcept {
    return position_ >= lines_.size();
}

}  // namespace efis::io
