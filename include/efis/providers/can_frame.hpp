#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace efis::can {

// Un frame CAN, indipendente dal trasporto che lo ha portato.
//
// SocketCAN, un adattatore USB, un banco che rilegge una registrazione: chi
// produce i frame non deve comparire qui. E' la stessa separazione che la
// ADR-003 impone fra adapter e trasporto, e senza di essa provare un adapter
// richiederebbe l'hardware.
struct Frame final {
    // Identificatore. Questo protocollo usa solo la forma a 11 bit.
    std::uint32_t id{};

    // Byte effettivamente presenti. Il protocollo del nodo sensori usa sempre
    // otto, ma un frame piu' corto e' legittimo sul bus e non va interpretato
    // completandolo con zeri: sarebbe inventare misure.
    std::uint8_t length{};

    std::array<std::uint8_t, 8> data{};
};

// Estrazione little-endian. L'ordine e' quello del firmware del nodo, verificato
// leggendo come impacchetta i valori, non assunto.
[[nodiscard]] constexpr std::optional<std::int16_t> read_i16(
    const Frame& frame, std::uint8_t offset) noexcept {
    const auto start = static_cast<std::size_t>(offset);
    const auto available = std::min<std::size_t>(frame.length, frame.data.size());
    if (start > available || 2U > available - start) return std::nullopt;
    const auto low = static_cast<std::uint16_t>(frame.data[offset]);
    const auto high = static_cast<std::uint16_t>(frame.data[offset + 1]);
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(low | (high << 8)));
}

[[nodiscard]] constexpr std::optional<std::uint16_t> read_u16(
    const Frame& frame, std::uint8_t offset) noexcept {
    const auto start = static_cast<std::size_t>(offset);
    const auto available = std::min<std::size_t>(frame.length, frame.data.size());
    if (start > available || 2U > available - start) return std::nullopt;
    const auto low = static_cast<std::uint16_t>(frame.data[offset]);
    const auto high = static_cast<std::uint16_t>(frame.data[offset + 1]);
    return static_cast<std::uint16_t>(low | (high << 8));
}

[[nodiscard]] constexpr std::optional<std::int32_t> read_i32(
    const Frame& frame, std::uint8_t offset) noexcept {
    const auto start = static_cast<std::size_t>(offset);
    const auto available = std::min<std::size_t>(frame.length, frame.data.size());
    if (start > available || 4U > available - start) return std::nullopt;
    std::uint32_t value = 0;
    for (std::uint8_t index = 0; index < 4; ++index) {
        value |= static_cast<std::uint32_t>(frame.data[offset + index]) << (8U * index);
    }
    return static_cast<std::int32_t>(value);
}

}  // namespace efis::can
