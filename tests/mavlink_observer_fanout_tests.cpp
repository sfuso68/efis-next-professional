#include "efis/io/readonly_udp_fanout.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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

std::vector<std::uint8_t> receive(
    efis::io::UdpReceiver& receiver, efis::io::UdpEndpoint& source) {
    std::array<std::uint8_t, 256> buffer{};
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto result = receiver.receive(buffer);
        if (result.status == efis::io::DatagramStatus::received) {
            source = result.source;
            return {buffer.begin(), buffer.begin() +
                    static_cast<std::ptrdiff_t>(result.size)};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return {};
}

}  // namespace

int main() {
    efis::io::UdpReceiver laptop{{"127.0.0.1", 0}};
    efis::io::UdpReceiver server{{"127.0.0.1", 0}};
    efis::io::ReadOnlyUdpFanout fanout{{
        {"127.0.0.1", laptop.local().port},
        {"127.0.0.1", server.local().port},
    }};

    const std::vector<std::uint8_t> mavlink_datagram{
        0xFDU, 0x01U, 0x00U, 0x00U, 0x2AU, 0x01U, 0x01U,
        0x00U, 0x00U, 0x00U, 0x03U, 0x4BU, 0xA1U};
    fanout.forward(mavlink_datagram);

    efis::io::UdpEndpoint laptop_source;
    efis::io::UdpEndpoint server_source;
    require(receive(laptop, laptop_source) == mavlink_datagram,
            "the laptop observer must receive an exact MAVLink copy");
    require(receive(server, server_source) == mavlink_datagram,
            "the server observer must receive an exact MAVLink copy");
    require(laptop_source.address == "127.0.0.1" && laptop_source.port != 0 &&
                server_source.address == "127.0.0.1" && server_source.port != 0,
            "UDP receive metadata must identify the datagram source");

    const auto aggregate = fanout.statistics();
    const auto targets = fanout.target_statistics();
    require(aggregate.datagrams == 2 &&
                aggregate.bytes == mavlink_datagram.size() * 2 &&
                aggregate.failures == 0,
            "fan-out aggregate metrics must count each observer independently");
    require(targets.size() == 2 && targets[0].datagrams == 1 &&
                targets[1].datagrams == 1,
            "per-observer metrics must remain separate");

    bool duplicate_rejected = false;
    try {
        efis::io::ReadOnlyUdpFanout invalid{{
            {"127.0.0.1", laptop.local().port},
            {"127.0.0.1", laptop.local().port},
        }};
    } catch (const std::invalid_argument&) {
        duplicate_rejected = true;
    }
    require(duplicate_rejected,
            "duplicate observer endpoints must be rejected");

    bool equivalent_duplicate_rejected = false;
    try {
        efis::io::ReadOnlyUdpFanout invalid{{
            {"127.0.0.1", laptop.local().port},
            {"127.000.000.001", laptop.local().port},
        }};
    } catch (const std::invalid_argument&) {
        equivalent_duplicate_rejected = true;
    }
    require(equivalent_duplicate_rejected,
            "numerically equivalent observer endpoints must be rejected");

    bool broadcast_rejected = false;
    try {
        efis::io::ReadOnlyUdpFanout invalid{{{"255.255.255.255", 14550}}};
    } catch (const std::invalid_argument&) {
        broadcast_rejected = true;
    }
    require(broadcast_rejected,
            "broadcast observers must be rejected");

    bool multicast_rejected = false;
    try {
        efis::io::ReadOnlyUdpFanout invalid{{{"239.1.2.3", 14550}}};
    } catch (const std::invalid_argument&) {
        multicast_rejected = true;
    }
    require(multicast_rejected,
            "multicast observers must be rejected");

    bool flight_data_rejected = false;
    try {
        efis::io::ReadOnlyUdpFanout invalid{{{"10.77.10.3", 14550}}};
    } catch (const std::invalid_argument&) {
        flight_data_rejected = true;
    }
    require(flight_data_rejected,
            "the Pixhawk address must be rejected as an observer destination");

    bool flight_data_node_rejected = false;
    try {
        efis::io::ReadOnlyUdpFanout invalid{{{"10.77.10.11", 14550}}};
    } catch (const std::invalid_argument&) {
        flight_data_node_rejected = true;
    }
    require(flight_data_node_rejected,
            "every FLIGHT-DATA destination must be rejected, not only the Pixhawk");

    bool protected_source_rejected = false;
    try {
        efis::io::ReadOnlyUdpFanout invalid{
            {{"192.168.1.47", 14550}}, "192.168.1.47"};
    } catch (const std::invalid_argument&) {
        protected_source_rejected = true;
    }
    require(protected_source_rejected,
            "an observer must never equal the configured MAVLink source");

    bool adjacent_service_network_allowed = true;
    try {
        efis::io::ReadOnlyUdpFanout allowed{{{"10.77.11.3", 14550}}};
    } catch (const std::invalid_argument&) {
        adjacent_service_network_allowed = false;
    }
    require(adjacent_service_network_allowed,
            "the policy must reject exactly FLIGHT-DATA /24, not adjacent networks");

    std::cout << "MAVLink read-only observer fan-out tests passed\n";
    return EXIT_SUCCESS;
}
