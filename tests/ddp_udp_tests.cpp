#include "efis/providers/ddp_udp_publisher.hpp"
#include "efis/providers/ddp_udp_source.hpp"
#include "efis/providers/simulation_provider.hpp"
#include "efis/ingest/blackbox_publisher_session.hpp"
#include "efis/recording/flight_record.hpp"
#include "efis/recording/segmented_recorder.hpp"

#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

std::vector<std::uint8_t> frame_v2(std::uint32_t id, std::uint8_t crc_extra,
                                   const std::vector<std::uint8_t>& payload,
                                   std::uint8_t sequence) {
    std::vector<std::uint8_t> body{
        static_cast<std::uint8_t>(payload.size()), 0, 0, sequence, 1, 1,
        static_cast<std::uint8_t>(id & 0xFFU),
        static_cast<std::uint8_t>((id >> 8U) & 0xFFU),
        static_cast<std::uint8_t>((id >> 16U) & 0xFFU)};
    body.insert(body.end(), payload.begin(), payload.end());
    std::uint16_t crc = 0xFFFFU;
    auto accumulate = [&crc](std::uint8_t byte) {
        auto tmp = static_cast<std::uint8_t>(
            byte ^ static_cast<std::uint8_t>(crc & 0xFFU));
        tmp = static_cast<std::uint8_t>(tmp ^ static_cast<std::uint8_t>(tmp << 4U));
        crc = static_cast<std::uint16_t>(
            (crc >> 8U) ^ (static_cast<std::uint16_t>(tmp) << 8U) ^
            (static_cast<std::uint16_t>(tmp) << 3U) ^
            (static_cast<std::uint16_t>(tmp) >> 4U));
    };
    for (const auto byte : body) accumulate(byte);
    accumulate(crc_extra);
    std::vector<std::uint8_t> output{0xFDU};
    output.insert(output.end(), body.begin(), body.end());
    output.push_back(static_cast<std::uint8_t>(crc & 0xFFU));
    output.push_back(static_cast<std::uint8_t>(crc >> 8U));
    return output;
}

void put_u32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void put_f32(std::vector<std::uint8_t>& output, float value) {
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    put_u32(output, bits);
}

std::vector<std::uint8_t> healthy_air_data(std::uint8_t sequence) {
    constexpr std::uint32_t differential_pressure = 0x20U;
    std::vector<std::uint8_t> payload;
    put_u32(payload, differential_pressure);
    put_u32(payload, differential_pressure);
    put_u32(payload, differential_pressure);
    payload.resize(31, 0);
    return frame_v2(1, 124, payload, sequence);
}

std::vector<std::uint8_t> vfr_hud(double airspeed_kt, std::uint8_t sequence) {
    constexpr double metres_per_second_per_knot = 1.0 / 1.943844;
    std::vector<std::uint8_t> payload;
    put_f32(payload, static_cast<float>(airspeed_kt * metres_per_second_per_knot));
    put_f32(payload, 45.0F * static_cast<float>(metres_per_second_per_knot));
    put_f32(payload, 1000.0F);
    put_f32(payload, 0.0F);
    payload.resize(20, 0);
    return frame_v2(74, 20, payload, sequence);
}

}  // namespace

int main() {
    using namespace std::chrono_literals;
    const auto now = efis::MonoTime{} + 10s;

    efis::DdpUdpSourceConfig source_config{};
    source_config.local = {"127.0.0.1", 0};
    source_config.consumer = {50ms, 150ms, 47};
    efis::DdpUdpFlightSource source{source_config};
    require(source.local_endpoint().port != 0,
            "an ephemeral receiver must expose its assigned port");
    require(source.receive_buffer_bytes() >= 64U * 1024U,
            "the DDP receiver must expose a non-trivial kernel backlog");

    efis::DdpUdpPublisher publisher{{
        {"127.0.0.1", source.local_endpoint().port},
        {efis::protocol::ddp::Profile::professional, 47, 0xDD02U, 1}}};
    efis::SimulationProvider simulation{42};
    const auto sent = simulation.sample(now, 10s);
    publisher.publish(sent, now);

    efis::FlightSnapshot received{};
    for (int attempt = 0; attempt < 100; ++attempt) {
        received = source.poll(now, 10s);
        if (received.attitude.roll_deg.usable()) break;
        std::this_thread::sleep_for(1ms);
    }
    require(received.attitude.roll_deg.usable(),
            "the UDP source must receive the loopback snapshot");
    require(received.attitude.roll_deg.source == efis::SourceId::simulation,
            "the broker transport must preserve the physical sample source");
    require(received.attitude.roll_deg.origin.node == 47,
            "the receiving sample must identify the publishing blackbox");
    require(source.source() == efis::SourceId::blackbox_broker,
            "the display router must identify the broker connection separately");
    require(publisher.metrics().datagrams == 1 && publisher.metrics().bytes > 0,
            "the publisher must expose transmitted datagram metrics");
    require(source.metrics().accepted == 1 && source.socket_errors() == 0,
            "the consumer must expose a clean accepted packet");

    // The renderer may be briefly unavailable while the scene graph or the
    // desktop session settles. The kernel backlog must retain a bounded burst
    // so the next poll can drain it without manufacturing a sequence gap.
    constexpr std::uint64_t backlog_datagrams = 128;
    for (std::uint64_t index = 0; index < backlog_datagrams; ++index) {
        publisher.publish(sent, now + 10ms);
    }
    for (int attempt = 0; attempt < 8; ++attempt) {
        (void)source.poll(now + 20ms, 10s);
        if (source.metrics().accepted == backlog_datagrams + 1) break;
        std::this_thread::sleep_for(1ms);
    }
    require(source.metrics().accepted == backlog_datagrams + 1,
            "the DDP receiver must drain a bounded startup backlog");
    require(source.metrics().sequence_gaps == 0,
            "the retained startup backlog must preserve sequence continuity");

    const auto stale = source.poll(now + 71ms, 10s);
    require(stale.attitude.roll_deg.validity == efis::Validity::stale,
            "a late network snapshot must not remain usable and green");
    const auto lost = source.poll(now + 171ms, 10s);
    require(lost.attitude.roll_deg.validity == efis::Validity::unavailable,
            "a lost network source must explicitly invalidate its values");

    efis::DdpUdpPublisher foreign{{
        {"127.0.0.1", source.local_endpoint().port},
        {efis::protocol::ddp::Profile::professional, 48, 0xBADU, 1}}};
    foreign.publish(sent, now + 200ms);
    for (int attempt = 0; attempt < 20; ++attempt) {
        (void)source.poll(now + 200ms, 10s);
        if (source.metrics().identity_rejected != 0) break;
        std::this_thread::sleep_for(1ms);
    }
    require(source.metrics().identity_rejected == 1,
            "a datagram from a different blackbox must be rejected");

    // Il publisher operativo deve inviare lo stato prodotto dalla pipeline
    // MAVLink, non un SimulationProvider nascosto dietro al broker.
    efis::DdpUdpSourceConfig live_source_config{};
    live_source_config.local = {"127.0.0.1", 0};
    live_source_config.consumer = {50ms, 150ms, 47};
    efis::DdpUdpFlightSource live_source{live_source_config};
    auto secondary_source =
        std::make_unique<efis::DdpUdpFlightSource>(live_source_config);
    const auto record_directory = std::filesystem::temp_directory_path() /
        ("efis-blackbox-ddp-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::remove_all(record_directory);
    efis::recording::SegmentedRecorder recorder{record_directory};
    const efis::protocol::ddp::PublisherConfig broker_identity{
        efis::protocol::ddp::Profile::professional, 47, 0xB10CU, 1};
    std::vector<efis::DdpUdpPublisherConfig> display_publishers{
        {{"127.0.0.1", live_source.local_endpoint().port}, broker_identity},
        {{"127.0.0.1", secondary_source->local_endpoint().port}, broker_identity}};
    efis::ingest::BlackboxPublisherSession broker{
        std::move(display_publishers), {}, &recorder};
    const auto health = healthy_air_data(1);
    const auto air = vfr_hud(83.0, 2);
    broker.consume_mavlink(health, now + 300ms);
    broker.consume_mavlink(air, now + 300ms);
    broker.publish(now + 300ms);

    efis::FlightSnapshot live_received{};
    for (int attempt = 0; attempt < 100; ++attempt) {
        live_received = live_source.poll(now + 300ms, 10s);
        if (live_received.air_data.indicated_airspeed_kt.usable()) break;
        std::this_thread::sleep_for(1ms);
    }
    require(live_received.air_data.indicated_airspeed_kt.usable(),
            "the blackbox pipeline snapshot must reach the DDP consumer");
    require(std::abs(live_received.air_data.indicated_airspeed_kt.value - 83.0) < 0.5,
            "the broker must publish the normalized MAVLink airspeed");
    require(live_received.air_data.indicated_airspeed_kt.source ==
                efis::SourceId::pixhawk_primary,
            "the broker must preserve Pixhawk physical provenance");
    require(!live_received.air_data.indicated_airspeed_kt.origin.simulated,
            "the operational broker must not relabel live input as simulation");

    efis::FlightSnapshot secondary_received{};
    for (int attempt = 0; attempt < 100; ++attempt) {
        secondary_received = secondary_source->poll(now + 300ms, 10s);
        if (secondary_received.air_data.indicated_airspeed_kt.usable()) break;
        std::this_thread::sleep_for(1ms);
    }
    require(secondary_received.air_data.indicated_airspeed_kt.usable(),
            "each configured display must receive the normalized snapshot");

    // La scomparsa di un display UDP non deve fermare il recorder ne' il PFD
    // rimasto: non esistono ACK o dipendenze inverse dal renderer.
    secondary_source.reset();
    broker.publish(now + 310ms);
    for (int attempt = 0; attempt < 100; ++attempt) {
        live_received = live_source.poll(now + 310ms, 10s);
        if (live_source.metrics().accepted == 2) break;
        std::this_thread::sleep_for(1ms);
    }
    const auto aggregate = broker.publisher_metrics();
    const auto display_metrics = broker.display_statistics();
    require(live_source.metrics().accepted == 2,
            "a remaining display must continue after another display disappears");
    require(aggregate.datagrams == 4 && display_metrics.size() == 2 &&
                display_metrics[0].transmitted.datagrams == 2 &&
                display_metrics[1].transmitted.datagrams == 2,
            "fan-out metrics must remain separate and aggregate correctly");
    require(broker.statistics().mavlink_bytes == health.size() + air.size() &&
                broker.statistics().snapshots_published == 2 &&
                broker.statistics().snapshots_recorded == 2 &&
                broker.statistics().ddp_publish_failures == 0,
            "the broker must expose ingress and publication metrics");

    bool duplicate_rejected = false;
    try {
        const efis::DdpUdpPublisherConfig duplicate{
            {"127.0.0.1", live_source.local_endpoint().port}, broker_identity};
        efis::ingest::BlackboxPublisherSession invalid{
            std::vector<efis::DdpUdpPublisherConfig>{duplicate, duplicate}};
    } catch (const std::invalid_argument&) {
        duplicate_rejected = true;
    }
    require(duplicate_rejected,
            "duplicate display endpoints must be rejected at configuration time");

    recorder.close();
    std::filesystem::path segment;
    for (const auto& entry : std::filesystem::directory_iterator(record_directory)) {
        if (entry.path().extension() == ".csv") segment = entry.path();
    }
    require(!segment.empty(), "the operational broker must close a flight segment");
    std::ifstream record_stream{segment};
    const auto record = efis::recording::read_flight_record(record_stream);
    bool recorded_airspeed = false;
    for (const auto& sample : record.samples) {
        if (sample.sample.origin.signal == efis::SignalId::indicated_airspeed &&
            std::abs(sample.sample.value - 83.0) < 0.5 &&
            sample.sample.source == efis::SourceId::pixhawk_primary) {
            recorded_airspeed = true;
        }
    }
    require(recorded_airspeed,
            "the Blackbox must record the same normalized state distributed by DDP");
    record_stream.close();
    std::filesystem::remove_all(record_directory);

    std::cout << "DDP V2 UDP loopback tests passed\n";
    return EXIT_SUCCESS;
}
