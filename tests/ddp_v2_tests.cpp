#include "efis/protocol/ddp_v2.hpp"
#include "efis/protocol/ddp_session.hpp"
#include "efis/providers/simulation_provider.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    using namespace std::chrono_literals;
    const auto now = efis::MonoTime{} + 10s;
    efis::SimulationProvider provider{42};
    auto snapshot = provider.sample(now, 10s);
    snapshot.sequence = 73;
    snapshot.health.level = efis::HealthLevel::caution;
    snapshot.health.stale_fields = 2;

    const auto wire = efis::protocol::ddp::encode_snapshot(
        snapshot, efis::protocol::ddp::Profile::professional, 47, 0xAABBCCDDU, now);
    const auto decoded = efis::protocol::ddp::decode_snapshot(wire);
    require(static_cast<bool>(decoded), "valid snapshot must decode");
    require(decoded.message.envelope.source_node == 47, "source node must survive");
    require(decoded.message.envelope.publisher_instance == 0xAABBCCDDU,
            "publisher instance must survive");
    require(decoded.message.envelope.sequence == 73, "sequence must survive");
    require(decoded.message.health.stale_fields == 2, "health must survive");
    require(decoded.message.samples.size() == 23, "all flight samples must be present");

    bool found_ias = false;
    bool found_fd = false;
    for (const auto& sample : decoded.message.samples) {
        if (sample.signal == efis::SignalId::indicated_airspeed) {
            found_ias = sample.value == snapshot.air_data.indicated_airspeed_kt.value &&
                        sample.simulated && sample.unit == efis::Unit::knot;
        }
        if (sample.signal == efis::SignalId::flight_director_active) {
            found_fd = sample.value == 1.0 && sample.unit == efis::Unit::boolean;
        }
    }
    require(found_ias, "IAS value, unit and simulated flag must survive");
    require(found_fd, "boolean samples must use an explicit wire representation");

    auto corrupted = wire;
    corrupted.back() ^= 0x80U;
    require(efis::protocol::ddp::decode_snapshot(corrupted).error ==
                efis::protocol::ddp::DecodeError::crc_mismatch,
            "payload corruption must be rejected");

    auto future = wire;
    future[4] = 0;
    future[5] = 3;
    require(efis::protocol::ddp::decode_snapshot(future).error ==
                efis::protocol::ddp::DecodeError::unsupported_schema,
            "newer schema must be rejected instead of partially interpreted");
    require(efis::protocol::ddp::decode_snapshot(
                std::span<const std::uint8_t>{wire}.first(20)).error ==
                efis::protocol::ddp::DecodeError::truncated,
            "truncated envelope must be rejected");

    auto malformed = wire;
    constexpr std::size_t first_sample_validity =
        efis::protocol::ddp::envelope_size + 25U + 12U;
    malformed[first_sample_validity] = 0xFFU;
    const auto replacement_crc = efis::protocol::ddp::crc32c(
        std::span<const std::uint8_t>{malformed}.subspan(
            efis::protocol::ddp::envelope_size));
    for (std::size_t index = 0; index < 4; ++index) {
        malformed[40U + index] = static_cast<std::uint8_t>(
            replacement_crc >> static_cast<unsigned>(24U - index * 8U));
    }
    require(efis::protocol::ddp::decode_snapshot(malformed).error ==
                efis::protocol::ddp::DecodeError::malformed_payload,
            "unknown sample enums must be rejected even with a valid CRC");

    auto duplicate_signal = wire;
    constexpr std::size_t first_sample =
        efis::protocol::ddp::envelope_size + 25U;
    constexpr std::size_t second_sample = first_sample + 20U;
    duplicate_signal[second_sample] = duplicate_signal[first_sample];
    duplicate_signal[second_sample + 1U] = duplicate_signal[first_sample + 1U];
    const auto duplicate_crc = efis::protocol::ddp::crc32c(
        std::span<const std::uint8_t>{duplicate_signal}.subspan(
            efis::protocol::ddp::envelope_size));
    for (std::size_t index = 0; index < 4; ++index) {
        duplicate_signal[40U + index] = static_cast<std::uint8_t>(
            duplicate_crc >> static_cast<unsigned>(24U - index * 8U));
    }
    require(efis::protocol::ddp::decode_snapshot(duplicate_signal).error ==
                efis::protocol::ddp::DecodeError::malformed_payload,
            "a duplicated signal must invalidate the complete snapshot");

    require(efis::protocol::ddp::crc32c({}) == 0,
            "CRC32C empty vector must match the standard check value");
    constexpr std::uint8_t check_bytes[]{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    require(efis::protocol::ddp::crc32c(check_bytes) == 0xE3069283U,
            "CRC32C must match the Castagnoli standard check vector");

    efis::protocol::ddp::SequenceGate gate;
    require(gate.accept(decoded.message.envelope) == efis::protocol::ddp::DecodeError::none,
            "first sequence must be accepted");
    require(gate.accept(decoded.message.envelope) ==
                efis::protocol::ddp::DecodeError::duplicate_sequence,
            "duplicate sequence must be rejected");
    auto next = decoded.message.envelope;
    next.sequence++;
    require(gate.accept(next) == efis::protocol::ddp::DecodeError::none,
            "increasing sequence must be accepted");
    next.sequence -= 2;
    require(gate.accept(next) == efis::protocol::ddp::DecodeError::reversed_sequence,
            "reversed sequence must be rejected");
    next.publisher_instance++;
    next.sequence = 1;
    require(gate.accept(next) == efis::protocol::ddp::DecodeError::none,
            "a new publisher instance must allow sequence restart after reboot");
    require(gate.accept(decoded.message.envelope) ==
                efis::protocol::ddp::DecodeError::reversed_sequence,
            "packets from a retired publisher instance must not become current again");

    using efis::protocol::ddp::ConsumerPolicy;
    using efis::protocol::ddp::LinkState;
    using efis::protocol::ddp::PublisherConfig;
    using efis::protocol::ddp::SnapshotConsumer;
    using efis::protocol::ddp::SnapshotPublisher;

    SnapshotPublisher publisher{PublisherConfig{
        efis::protocol::ddp::Profile::professional, 47, 0x101U, 10}};
    SnapshotConsumer consumer{ConsumerPolicy{100ms, 300ms, 47}};
    require(consumer.state(now) == LinkState::waiting,
            "a consumer without data must explicitly report waiting");

    const auto packet_10 = publisher.publish(snapshot, now);
    require(publisher.next_sequence() == 11,
            "the publisher must own a monotonic sequence independent of the input snapshot");
    const auto accepted_10 = consumer.consume(packet_10, now);
    require(static_cast<bool>(accepted_10), "the first publisher datagram must be accepted");
    require(consumer.latest() != nullptr && consumer.latest()->sequence == 10,
            "the consumer must reconstruct the published sequence");
    require(consumer.latest()->air_data.indicated_airspeed_kt.origin.node == 47,
            "the receiving display must retain the publishing blackbox identity");
    require(consumer.latest()->air_data.indicated_airspeed_kt.is_simulated(),
            "the session layer must never erase the simulation marker");
    require(consumer.state(now + 100ms) == LinkState::live,
            "the stale boundary must still be live");
    require(consumer.state(now + 101ms) == LinkState::stale,
            "a late publisher must be distinguished from a lost publisher");
    require(consumer.state(now + 301ms) == LinkState::lost,
            "the consumer must report a lost publisher after the hard timeout");

    require(consumer.consume(packet_10, now + 10ms).error ==
                efis::protocol::ddp::DecodeError::duplicate_sequence,
            "the session must reject duplicate datagrams");
    require(consumer.metrics().duplicates == 1 && consumer.metrics().accepted == 1,
            "duplicate rejection must be visible in operational metrics");

    auto corrupt_session_packet = packet_10;
    corrupt_session_packet.back() ^= 1U;
    require(consumer.consume(corrupt_session_packet, now + 20ms).error ==
                efis::protocol::ddp::DecodeError::crc_mismatch,
            "CRC failure must not reach the display snapshot");
    require(consumer.metrics().crc_rejected == 1 && consumer.latest()->sequence == 10,
            "CRC rejection must be counted without replacing the last good state");

    const auto skipped_11 = publisher.publish(snapshot, now + 30ms);
    (void)skipped_11;
    const auto packet_12 = publisher.publish(snapshot, now + 40ms);
    const auto accepted_12 = consumer.consume(packet_12, now + 40ms);
    require(static_cast<bool>(accepted_12) && accepted_12.missing_before == 1,
            "a lost datagram must be accepted and reported as a sequence gap");
    require(consumer.metrics().sequence_gaps == 1,
            "sequence loss must accumulate in transport metrics");

    SnapshotPublisher foreign{PublisherConfig{
        efis::protocol::ddp::Profile::professional, 48, 0x202U, 1}};
    require(consumer.consume(foreign.publish(snapshot, now + 50ms), now + 50ms).error ==
                efis::protocol::ddp::DecodeError::invalid_identity,
            "a display pinned to its blackbox must reject another node");
    require(consumer.metrics().identity_rejected == 1,
            "publisher identity rejection must be observable");

    SnapshotPublisher rebooted{PublisherConfig{
        efis::protocol::ddp::Profile::professional, 47, 0x303U, 1}};
    require(static_cast<bool>(consumer.consume(
                rebooted.publish(snapshot, now + 60ms), now + 60ms)),
            "a new instance from the expected node must recover after reboot");
    require(consumer.metrics().publisher_restarts == 1,
            "publisher reboot must be counted separately from packet loss");
    require(consumer.consume(packet_12, now + 70ms).error ==
                efis::protocol::ddp::DecodeError::reversed_sequence,
            "a delayed packet from the retired instance must stay rejected");

    consumer.reset();
    require(consumer.state(now) == LinkState::waiting &&
                consumer.metrics().datagrams == 0 && consumer.latest() == nullptr,
            "reset must clear both state and diagnostics");

    std::cout << "DDP V2 codec and session tests passed\n";
    return EXIT_SUCCESS;
}
