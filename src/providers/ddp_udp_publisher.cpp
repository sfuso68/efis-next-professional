#include "efis/providers/ddp_udp_publisher.hpp"

#include <utility>

namespace efis {

DdpUdpPublisher::DdpUdpPublisher(DdpUdpPublisherConfig config)
    : sender_(std::move(config.destination)), session_(config.session) {}

void DdpUdpPublisher::publish(const FlightSnapshot& snapshot, MonoTime now) {
    const auto datagram = session_.publish(snapshot, now);
    sender_.send(datagram);
    ++metrics_.datagrams;
    metrics_.bytes += datagram.size();
}

}  // namespace efis
