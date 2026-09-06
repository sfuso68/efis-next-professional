#include "efis/protocol/ddp_v2.hpp"
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

    std::cout << "DDP V2 codec tests passed\n";
    return EXIT_SUCCESS;
}
