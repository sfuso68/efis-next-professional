#pragma once

#include "efis/io/udp_datagram.hpp"
#include "efis/protocol/ddp_session.hpp"

namespace efis {

struct DdpUdpPublisherConfig final {
    io::UdpEndpoint destination;
    protocol::ddp::PublisherConfig session;
};

struct DdpUdpPublisherMetrics final {
    std::uint64_t datagrams{};
    std::uint64_t bytes{};
};

class DdpUdpPublisher final {
public:
    explicit DdpUdpPublisher(DdpUdpPublisherConfig config);

    void publish(const FlightSnapshot& snapshot, MonoTime now);
    [[nodiscard]] const DdpUdpPublisherMetrics& metrics() const noexcept {
        return metrics_;
    }

private:
    io::UdpSender sender_;
    protocol::ddp::SnapshotPublisher session_;
    DdpUdpPublisherMetrics metrics_{};
};

}  // namespace efis
