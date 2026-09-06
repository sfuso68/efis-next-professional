#include "efis/protocol/ddp_session.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace efis::protocol::ddp {
namespace {

MonoTime sample_time(MonoTime received_at, std::uint32_t age_ms) {
    const auto age = std::chrono::milliseconds{age_ms};
    if (age > received_at.time_since_epoch()) return MonoTime{};
    return received_at - age;
}

void populate(Sample<double>& target, const WireSample& wire,
              std::uint32_t source_node, MonoTime received_at,
              std::uint64_t sequence) {
    target.value = wire.value;
    target.source = wire.source;
    target.sampled_at = sample_time(received_at, wire.age_ms);
    target.received_at = target.sampled_at;
    target.quality = wire.quality;
    target.validity = wire.validity;
    target.sequence = sequence;
    target.origin.signal = wire.signal;
    target.origin.node = static_cast<NodeId>(source_node);
    target.origin.channel = channel_undeclared;
    target.origin.unit = wire.unit;
    target.origin.simulated = wire.simulated;
}

void populate(Sample<bool>& target, const WireSample& wire,
              std::uint32_t source_node, MonoTime received_at,
              std::uint64_t sequence) {
    target.value = wire.value != 0.0;
    target.source = wire.source;
    target.sampled_at = sample_time(received_at, wire.age_ms);
    target.received_at = target.sampled_at;
    target.quality = wire.quality;
    target.validity = wire.validity;
    target.sequence = sequence;
    target.origin.signal = wire.signal;
    target.origin.node = static_cast<NodeId>(source_node);
    target.origin.channel = channel_undeclared;
    target.origin.unit = wire.unit;
    target.origin.simulated = wire.simulated;
}

bool apply(FlightSnapshot& out, const WireSample& wire,
           const Envelope& envelope, MonoTime received_at) {
    auto set = [&](Sample<double>& sample) {
        populate(sample, wire, envelope.source_node, received_at, envelope.sequence);
        return true;
    };
    switch (wire.signal) {
        case SignalId::roll: return set(out.attitude.roll_deg);
        case SignalId::pitch: return set(out.attitude.pitch_deg);
        case SignalId::heading: return set(out.attitude.heading_deg);
        case SignalId::turn_rate: return set(out.attitude.turn_rate_deg_s);
        case SignalId::lateral_acceleration: return set(out.attitude.lateral_acceleration_g);
        case SignalId::indicated_airspeed: return set(out.air_data.indicated_airspeed_kt);
        case SignalId::pressure_altitude: return set(out.air_data.pressure_altitude_ft);
        case SignalId::vertical_speed: return set(out.air_data.vertical_speed_fpm);
        case SignalId::baro_setting: return set(out.air_data.baro_setting_hpa);
        case SignalId::indicated_altitude: return set(out.air_data.indicated_altitude_ft);
        case SignalId::latitude: return set(out.position.latitude_deg);
        case SignalId::longitude: return set(out.position.longitude_deg);
        case SignalId::ground_speed: return set(out.position.ground_speed_kt);
        case SignalId::gnss_vertical_speed: return set(out.position.gnss_vertical_speed_fpm);
        case SignalId::commanded_roll: return set(out.guidance.commanded_roll_deg);
        case SignalId::commanded_pitch: return set(out.guidance.commanded_pitch_deg);
        case SignalId::selected_altitude: return set(out.guidance.selected_altitude_ft);
        case SignalId::selected_course: return set(out.guidance.selected_course_deg);
        case SignalId::selected_airspeed: return set(out.guidance.selected_airspeed_kt);
        case SignalId::cdi_deviation: return set(out.navigation.cdi_deviation_dots);
        case SignalId::glide_slope_deviation: return set(out.navigation.glide_slope_dots);
        case SignalId::flight_director_active:
            populate(out.guidance.flight_director_active, wire, envelope.source_node,
                     received_at, envelope.sequence);
            return true;
        case SignalId::to_flag:
            populate(out.navigation.to_flag, wire, envelope.source_node,
                     received_at, envelope.sequence);
            return true;
        default: return false;
    }
}

bool reconstruct(const SnapshotMessage& message, MonoTime received_at,
                 FlightSnapshot& out) {
    FlightSnapshot candidate{};
    candidate.timestamp = received_at;
    candidate.sequence = message.envelope.sequence;
    candidate.health = message.health;
    for (const auto& wire : message.samples) {
        if (!apply(candidate, wire, message.envelope, received_at)) return false;
    }
    out = std::move(candidate);
    return true;
}

}  // namespace

SnapshotPublisher::SnapshotPublisher(PublisherConfig config)
    : config_(config), next_sequence_(config.first_sequence) {
    if (config_.source_node == 0 || config_.publisher_instance == 0 ||
        config_.source_node > std::numeric_limits<NodeId>::max() ||
        config_.first_sequence == 0 ||
        config_.profile < Profile::standard || config_.profile > Profile::degraded) {
        throw std::invalid_argument{"DDP publisher identity and sequence must be non-zero"};
    }
}

std::vector<std::uint8_t> SnapshotPublisher::publish(
    const FlightSnapshot& snapshot, MonoTime published_at) {
    if (next_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error{"DDP publisher sequence exhausted; start a new instance"};
    }
    auto numbered = snapshot;
    numbered.sequence = next_sequence_++;
    return encode_snapshot(numbered, config_.profile, config_.source_node,
                           config_.publisher_instance, published_at);
}

SnapshotConsumer::SnapshotConsumer(ConsumerPolicy policy) : policy_(policy) {
    if (policy_.stale_after <= std::chrono::milliseconds::zero() ||
        policy_.lost_after <= policy_.stale_after) {
        throw std::invalid_argument{"DDP timeout policy must satisfy 0 < stale < lost"};
    }
}

ConsumeResult SnapshotConsumer::consume(std::span<const std::uint8_t> datagram,
                                        MonoTime received_at) {
    ++metrics_.datagrams;
    const auto decoded = decode_snapshot(datagram);
    if (!decoded) {
        ++metrics_.decode_rejected;
        if (decoded.error == DecodeError::crc_mismatch) ++metrics_.crc_rejected;
        return {decoded.error, 0};
    }
    if (policy_.expected_source_node != 0 &&
        decoded.message.envelope.source_node != policy_.expected_source_node) {
        ++metrics_.identity_rejected;
        return {DecodeError::invalid_identity, 0};
    }
    if (decoded.message.envelope.source_node >
        std::numeric_limits<NodeId>::max()) {
        ++metrics_.identity_rejected;
        return {DecodeError::invalid_identity, 0};
    }

    FlightSnapshot candidate{};
    if (!reconstruct(decoded.message, received_at, candidate)) {
        ++metrics_.decode_rejected;
        return {DecodeError::malformed_payload, 0};
    }

    const auto gate_error = sequence_gate_.accept(decoded.message.envelope);
    if (gate_error != DecodeError::none) {
        if (gate_error == DecodeError::duplicate_sequence) ++metrics_.duplicates;
        else ++metrics_.reversed;
        return {gate_error, 0};
    }

    std::uint64_t missing{};
    if (has_snapshot_ &&
        last_envelope_.source_node == decoded.message.envelope.source_node) {
        if (last_envelope_.publisher_instance ==
            decoded.message.envelope.publisher_instance) {
            const auto advance = decoded.message.envelope.sequence - last_envelope_.sequence;
            if (advance > 1U) {
                missing = advance - 1U;
                metrics_.sequence_gaps += missing;
            }
        } else {
            ++metrics_.publisher_restarts;
        }
    }

    latest_ = std::move(candidate);
    last_envelope_ = decoded.message.envelope;
    last_received_at_ = received_at;
    has_snapshot_ = true;
    ++metrics_.accepted;
    return {DecodeError::none, missing};
}

LinkState SnapshotConsumer::state(MonoTime now) const noexcept {
    if (!has_snapshot_) return LinkState::waiting;
    if (now <= last_received_at_) return LinkState::live;
    const auto age = now - last_received_at_;
    if (age <= policy_.stale_after) return LinkState::live;
    if (age <= policy_.lost_after) return LinkState::stale;
    return LinkState::lost;
}

const FlightSnapshot* SnapshotConsumer::latest() const noexcept {
    return has_snapshot_ ? &latest_ : nullptr;
}

void SnapshotConsumer::reset() noexcept {
    sequence_gate_.reset();
    metrics_ = {};
    latest_ = {};
    last_received_at_ = {};
    last_envelope_ = {};
    has_snapshot_ = false;
}

}  // namespace efis::protocol::ddp
