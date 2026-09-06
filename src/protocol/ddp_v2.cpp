#include "efis/protocol/ddp_v2.hpp"

#include "efis/core/signal_access.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <limits>
#include <string_view>

namespace efis::protocol::ddp {
namespace {

constexpr std::array<std::uint8_t, 4> magic{'E', 'F', 'N', '2'};
constexpr std::size_t health_size = 25;
constexpr std::size_t sample_size = 20;

void put_u16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
    out.push_back(static_cast<std::uint8_t>(value));
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>(value >> static_cast<unsigned>(shift)));
    }
}

void put_u64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<std::uint8_t>(value >> static_cast<unsigned>(shift)));
    }
}

void put_f64(std::vector<std::uint8_t>& out, double value) {
    put_u64(out, std::bit_cast<std::uint64_t>(value));
}

bool take_u16(std::span<const std::uint8_t> in, std::size_t& at, std::uint16_t& value) {
    if (at + 2U > in.size()) return false;
    value = static_cast<std::uint16_t>((static_cast<std::uint16_t>(in[at]) << 8U) |
                                       static_cast<std::uint16_t>(in[at + 1U]));
    at += 2U;
    return true;
}

bool take_u32(std::span<const std::uint8_t> in, std::size_t& at, std::uint32_t& value) {
    if (at + 4U > in.size()) return false;
    value = 0;
    for (int index = 0; index < 4; ++index) {
        value = (value << 8U) | in[at + static_cast<std::size_t>(index)];
    }
    at += 4U;
    return true;
}

bool take_u64(std::span<const std::uint8_t> in, std::size_t& at, std::uint64_t& value) {
    if (at + 8U > in.size()) return false;
    value = 0;
    for (int index = 0; index < 8; ++index) {
        value = (value << 8U) | in[at + static_cast<std::size_t>(index)];
    }
    at += 8U;
    return true;
}

bool take_f64(std::span<const std::uint8_t> in, std::size_t& at, double& value) {
    std::uint64_t bits{};
    if (!take_u64(in, at, bits)) return false;
    value = std::bit_cast<double>(bits);
    return true;
}

std::uint32_t age_ms(const Sample<double>& sample, MonoTime now) {
    if (sample.received_at == MonoTime{} || now <= sample.received_at) return 0;
    const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - sample.received_at).count();
    return static_cast<std::uint32_t>(std::min<std::int64_t>(
        age, std::numeric_limits<std::uint32_t>::max()));
}

void append_sample(std::vector<WireSample>& out, SignalId signal,
                   const Sample<double>& sample, MonoTime now) {
    out.push_back(WireSample{signal, sample.value, sample.source,
                             sample.validity, sample.quality, sample.origin.unit,
                             age_ms(sample, now), sample.is_simulated()});
}

template <class T>
void append_boolean(std::vector<WireSample>& out, SignalId signal,
                    const Sample<T>& sample, MonoTime now) {
    Sample<double> converted{};
    converted.value = sample.value ? 1.0 : 0.0;
    converted.source = sample.source;
    converted.sampled_at = sample.sampled_at;
    converted.received_at = sample.received_at;
    converted.quality = sample.quality;
    converted.validity = sample.validity;
    converted.sequence = sample.sequence;
    converted.origin = sample.origin;
    converted.origin.unit = Unit::boolean;
    append_sample(out, signal, converted, now);
}

std::vector<WireSample> collect_samples(const FlightSnapshot& snapshot, MonoTime now) {
    std::vector<WireSample> samples;
    samples.reserve(flight_signals().size() + 2U);
    for (const auto signal : flight_signals()) {
        if (const auto* sample = find_signal(snapshot, signal)) {
            append_sample(samples, signal, *sample, now);
        }
    }
    append_boolean(samples, SignalId::flight_director_active,
                   snapshot.guidance.flight_director_active, now);
    append_boolean(samples, SignalId::to_flag, snapshot.navigation.to_flag, now);
    return samples;
}

bool known_profile(std::uint8_t value) {
    return value >= static_cast<std::uint8_t>(Profile::standard) &&
           value <= static_cast<std::uint8_t>(Profile::degraded);
}

bool known_payload(std::uint8_t value) {
    return value >= static_cast<std::uint8_t>(PayloadType::snapshot) &&
           value <= static_cast<std::uint8_t>(PayloadType::config);
}

bool known_signal(std::uint16_t value) {
    const auto signal = static_cast<SignalId>(value);
    return signal != SignalId::unknown &&
           std::string_view{signal_name(signal)} != "unknown";
}

}  // namespace

std::uint32_t crc32c(std::span<const std::uint8_t> bytes) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const auto byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0x82F63B78U & mask);
        }
    }
    return ~crc;
}

std::vector<std::uint8_t> encode_snapshot(const FlightSnapshot& snapshot, Profile profile,
                                          std::uint32_t source_node,
                                          std::uint64_t publisher_instance,
                                          MonoTime published_at) {
    const auto samples = collect_samples(snapshot, published_at);
    std::vector<std::uint8_t> payload;
    payload.reserve(health_size + samples.size() * sample_size);
    payload.push_back(static_cast<std::uint8_t>(snapshot.health.level));
    put_u32(payload, snapshot.health.unavailable_fields);
    put_u32(payload, snapshot.health.stale_fields);
    put_u32(payload, snapshot.health.suspect_fields);
    put_u32(payload, snapshot.health.disagreeing_fields);
    put_u32(payload, snapshot.health.degraded_fields);
    put_u16(payload, static_cast<std::uint16_t>(snapshot.health.frozen_signal));
    put_u16(payload, static_cast<std::uint16_t>(samples.size()));

    for (const auto& sample : samples) {
        put_u16(payload, static_cast<std::uint16_t>(sample.signal));
        put_f64(payload, sample.value);
        put_u16(payload, static_cast<std::uint16_t>(sample.source));
        payload.push_back(static_cast<std::uint8_t>(sample.validity));
        payload.push_back(static_cast<std::uint8_t>(sample.quality));
        payload.push_back(static_cast<std::uint8_t>(sample.unit));
        put_u32(payload, sample.age_ms);
        payload.push_back(sample.simulated ? 1U : 0U);
    }

    std::vector<std::uint8_t> output;
    output.reserve(envelope_size + payload.size());
    output.insert(output.end(), magic.begin(), magic.end());
    put_u16(output, schema_version);
    output.push_back(static_cast<std::uint8_t>(profile));
    output.push_back(static_cast<std::uint8_t>(PayloadType::snapshot));
    put_u32(output, source_node);
    put_u64(output, publisher_instance);
    put_u64(output, snapshot.sequence);
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        published_at.time_since_epoch()).count();
    put_u64(output, static_cast<std::uint64_t>(std::max<std::int64_t>(0, us)));
    put_u32(output, static_cast<std::uint32_t>(payload.size()));
    put_u32(output, crc32c(payload));
    output.insert(output.end(), payload.begin(), payload.end());
    return output;
}

DecodeResult decode_snapshot(std::span<const std::uint8_t> bytes) {
    DecodeResult result{};
    if (bytes.size() < envelope_size) { result.error = DecodeError::truncated; return result; }
    if (!std::equal(magic.begin(), magic.end(), bytes.begin())) {
        result.error = DecodeError::bad_magic; return result;
    }

    std::size_t at = magic.size();
    std::uint16_t schema{};
    if (!take_u16(bytes, at, schema)) { result.error = DecodeError::truncated; return result; }
    if (schema != schema_version) { result.error = DecodeError::unsupported_schema; return result; }
    const auto profile = bytes[at++];
    const auto payload_type = bytes[at++];
    if (!known_profile(profile)) { result.error = DecodeError::invalid_profile; return result; }
    if (!known_payload(payload_type)) { result.error = DecodeError::invalid_payload_type; return result; }
    if (payload_type != static_cast<std::uint8_t>(PayloadType::snapshot)) {
        result.error = DecodeError::invalid_payload_type; return result;
    }

    auto& envelope = result.message.envelope;
    envelope.profile = static_cast<Profile>(profile);
    envelope.payload_type = static_cast<PayloadType>(payload_type);
    if (!take_u32(bytes, at, envelope.source_node) ||
        !take_u64(bytes, at, envelope.publisher_instance) ||
        !take_u64(bytes, at, envelope.sequence) ||
        !take_u64(bytes, at, envelope.monotonic_us) ||
        !take_u32(bytes, at, envelope.payload_length) ||
        !take_u32(bytes, at, envelope.payload_crc32c)) {
        result.error = DecodeError::truncated; return result;
    }
    if (envelope.source_node == 0 || envelope.publisher_instance == 0) {
        result.error = DecodeError::invalid_identity; return result;
    }
    if (envelope.payload_length > max_payload_size) {
        result.error = DecodeError::payload_too_large; return result;
    }
    if (bytes.size() != envelope_size + envelope.payload_length) {
        result.error = DecodeError::length_mismatch; return result;
    }
    const auto payload = bytes.subspan(envelope_size, envelope.payload_length);
    if (crc32c(payload) != envelope.payload_crc32c) {
        result.error = DecodeError::crc_mismatch; return result;
    }

    at = 0;
    if (payload.size() < health_size) { result.error = DecodeError::malformed_payload; return result; }
    auto& health = result.message.health;
    if (payload[at] > static_cast<std::uint8_t>(HealthLevel::warning)) {
        result.error = DecodeError::malformed_payload; return result;
    }
    health.level = static_cast<HealthLevel>(payload[at++]);
    std::uint16_t frozen{};
    std::uint16_t count{};
    if (!take_u32(payload, at, health.unavailable_fields) ||
        !take_u32(payload, at, health.stale_fields) ||
        !take_u32(payload, at, health.suspect_fields) ||
        !take_u32(payload, at, health.disagreeing_fields) ||
        !take_u32(payload, at, health.degraded_fields) ||
        !take_u16(payload, at, frozen) || !take_u16(payload, at, count)) {
        result.error = DecodeError::malformed_payload; return result;
    }
    health.frozen_signal = static_cast<SignalId>(frozen);
    if (payload.size() != health_size + static_cast<std::size_t>(count) * sample_size) {
        result.error = DecodeError::malformed_payload; return result;
    }
    result.message.samples.reserve(count);
    for (std::uint16_t index = 0; index < count; ++index) {
        WireSample sample{};
        std::uint16_t signal{};
        std::uint16_t source{};
        if (!take_u16(payload, at, signal) || !take_f64(payload, at, sample.value) ||
            !take_u16(payload, at, source) || at + 3U > payload.size()) {
            result.error = DecodeError::malformed_payload; return result;
        }
        if (!known_signal(signal) || source > static_cast<std::uint16_t>(SourceId::pilot_setting) ||
            payload[at] > static_cast<std::uint8_t>(Validity::degraded) ||
            payload[at + 1U] > static_cast<std::uint8_t>(Quality::high) ||
            payload[at + 2U] > static_cast<std::uint8_t>(Unit::pascal)) {
            result.error = DecodeError::malformed_payload; return result;
        }
        sample.signal = static_cast<SignalId>(signal);
        sample.source = static_cast<SourceId>(source);
        sample.validity = static_cast<Validity>(payload[at++]);
        sample.quality = static_cast<Quality>(payload[at++]);
        sample.unit = static_cast<Unit>(payload[at++]);
        if (!take_u32(payload, at, sample.age_ms) || at >= payload.size()) {
            result.error = DecodeError::malformed_payload; return result;
        }
        sample.simulated = payload[at++] != 0;
        result.message.samples.push_back(sample);
    }
    return result;
}

DecodeError SequenceGate::accept(const Envelope& envelope) noexcept {
    const auto exact = std::find_if(states_.begin(), states_.end(), [&](const State& state) {
        return state.node == envelope.source_node &&
               state.instance == envelope.publisher_instance;
    });
    if (exact != states_.end()) {
        if (!exact->current) return DecodeError::reversed_sequence;
        if (envelope.sequence == exact->sequence) return DecodeError::duplicate_sequence;
        if (envelope.sequence < exact->sequence) return DecodeError::reversed_sequence;
        exact->sequence = envelope.sequence;
        return DecodeError::none;
    }

    const auto current = std::find_if(states_.begin(), states_.end(), [&](const State& state) {
        return state.node == envelope.source_node && state.current;
    });
    if (current == states_.end()) {
        states_.push_back(State{envelope.source_node, envelope.publisher_instance,
                                envelope.sequence, true});
        return DecodeError::none;
    }
    current->current = false;
    states_.push_back(State{envelope.source_node, envelope.publisher_instance,
                            envelope.sequence, true});
    return DecodeError::none;
}

void SequenceGate::reset() noexcept { states_.clear(); }

}  // namespace efis::protocol::ddp
