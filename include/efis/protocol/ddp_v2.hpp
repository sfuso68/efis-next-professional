#pragma once

#include "efis/core/flight_snapshot.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace efis::protocol::ddp {

inline constexpr std::uint16_t schema_version = 2;
inline constexpr std::size_t envelope_size = 44;
inline constexpr std::size_t max_payload_size = 64U * 1024U;

enum class Profile : std::uint8_t { standard = 1, professional = 2, degraded = 3 };
enum class PayloadType : std::uint8_t { snapshot = 1, alert = 2, heartbeat = 3, config = 4 };

enum class DecodeError : std::uint8_t {
    none,
    truncated,
    bad_magic,
    unsupported_schema,
    invalid_profile,
    invalid_payload_type,
    invalid_identity,
    payload_too_large,
    length_mismatch,
    crc_mismatch,
    malformed_payload,
    duplicate_sequence,
    reversed_sequence
};

struct Envelope final {
    Profile profile{Profile::degraded};
    PayloadType payload_type{PayloadType::snapshot};
    std::uint32_t source_node{};
    std::uint64_t publisher_instance{};
    std::uint64_t sequence{};
    std::uint64_t monotonic_us{};
    std::uint32_t payload_length{};
    std::uint32_t payload_crc32c{};
};

struct WireSample final {
    SignalId signal{SignalId::unknown};
    double value{};
    SourceId source{SourceId::unknown};
    Validity validity{Validity::unavailable};
    Quality quality{Quality::unknown};
    Unit unit{Unit::unknown};
    std::uint32_t age_ms{};
    bool simulated{};
};

struct SnapshotMessage final {
    Envelope envelope{};
    SystemHealth health{};
    std::vector<WireSample> samples;
};

struct DecodeResult final {
    DecodeError error{DecodeError::none};
    SnapshotMessage message{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == DecodeError::none;
    }
};

// Codifica uno snapshot gia' normalizzato e arbitrato. Il codec non seleziona
// sorgenti e non corregge valori: conserva la decisione presa dal core.
[[nodiscard]] std::vector<std::uint8_t> encode_snapshot(
    const FlightSnapshot& snapshot, Profile profile, std::uint32_t source_node,
    std::uint64_t publisher_instance, MonoTime published_at);

[[nodiscard]] DecodeResult decode_snapshot(std::span<const std::uint8_t> bytes);

[[nodiscard]] std::uint32_t crc32c(std::span<const std::uint8_t> bytes) noexcept;

// Gate per publisher. Le sequenze sono confrontate per nodo, non globalmente:
// due blackbox legittime possono avere lo stesso numero senza duplicarsi.
class SequenceGate final {
public:
    [[nodiscard]] DecodeError accept(const Envelope& envelope) noexcept;
    void reset() noexcept;

private:
    struct State final {
        std::uint32_t node{};
        std::uint64_t instance{};
        std::uint64_t sequence{};
        bool current{};
    };
    std::vector<State> states_;
};

}  // namespace efis::protocol::ddp
