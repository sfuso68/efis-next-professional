#include "efis/providers/ddp_udp_publisher.hpp"
#include "efis/providers/ddp_udp_source.hpp"
#include "efis/providers/simulation_provider.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
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

    const auto stale = source.poll(now + 51ms, 10s);
    require(stale.attitude.roll_deg.validity == efis::Validity::stale,
            "a late network snapshot must not remain usable and green");
    const auto lost = source.poll(now + 151ms, 10s);
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

    std::cout << "DDP V2 UDP loopback tests passed\n";
    return EXIT_SUCCESS;
}
