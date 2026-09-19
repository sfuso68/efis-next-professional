#include "efis/core/flight_model.hpp"
#include "efis/core/freshness_budget.hpp"
#include "efis/core/node_health.hpp"
#include "efis/protocol/ddp_session.hpp"
#include "efis/protocol/edp_v1.hpp"

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
    using efis::FreshnessBudget;
    require(efis::FlightModelPolicy{}.stale_after == FreshnessBudget::sample_stale,
            "flight samples must use the central sample budget");
    require(efis::protocol::ddp::ConsumerPolicy{}.stale_after ==
                FreshnessBudget::transport_stale &&
            efis::protocol::ddp::ConsumerPolicy{}.lost_after ==
                FreshnessBudget::transport_lost,
            "DDP must derive both thresholds from the central budget");
    require(efis::protocol::edp::ConsumerPolicy{}.stale_after ==
                FreshnessBudget::transport_stale &&
            efis::protocol::edp::ConsumerPolicy{}.lost_after ==
                FreshnessBudget::transport_lost,
            "EDP must derive both thresholds from the central budget");
    require(efis::NodeHealthPolicy{}.stale_after == FreshnessBudget::node_stale &&
            efis::NodeHealthPolicy{}.lost_after == FreshnessBudget::node_lost,
            "node health must derive both thresholds from the central budget");

    const auto published = efis::MonoTime{} + 10s;
    efis::FlightSnapshot snapshot{};
    snapshot.attitude.roll_deg.value = 2.0;
    snapshot.attitude.roll_deg.received_at = published - 450ms;
    snapshot.attitude.roll_deg.validity = efis::Validity::valid;
    snapshot.attitude.roll_deg.origin.signal = efis::SignalId::roll;
    efis::protocol::ddp::SnapshotPublisher publisher{{
        .source_node = 47, .publisher_instance = 1}};
    const auto datagram = publisher.publish(snapshot, published);
    efis::protocol::ddp::SnapshotConsumer consumer;
    const auto received = published + 200ms;
    require(static_cast<bool>(consumer.consume(datagram, received)),
            "reference DDP snapshot must decode");
    const auto* reconstructed = consumer.latest();
    require(reconstructed != nullptr &&
            reconstructed->attitude.roll_deg.received_at ==
                received - 450ms - FreshnessBudget::network_age_margin,
            "DDP age must include the declared network uncertainty margin");
    std::cout << "Freshness budget tests passed\n";
    return EXIT_SUCCESS;
}
