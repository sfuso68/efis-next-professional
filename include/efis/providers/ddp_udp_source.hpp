#pragma once

#include "efis/core/flight_source.hpp"
#include "efis/io/udp_datagram.hpp"
#include "efis/protocol/ddp_session.hpp"

#include <array>

namespace efis {

struct DdpUdpSourceConfig final {
    io::UdpEndpoint local{"0.0.0.0", 49'002};
    protocol::ddp::ConsumerPolicy consumer{};
    std::size_t max_datagrams_per_poll{64};
};

// Adapter display read-only. Riceve esclusivamente snapshot DDP gia'
// normalizzati: non conosce MAVLink, CAN o Pixhawk e non possiede un metodo di
// trasmissione, come richiesto dalla ADR-003.
class DdpUdpFlightSource final : public FlightSource {
public:
    explicit DdpUdpFlightSource(DdpUdpSourceConfig config);

    [[nodiscard]] SourceId source() const noexcept override {
        return SourceId::blackbox_broker;
    }
    [[nodiscard]] FlightSnapshot poll(MonoTime now,
                                      std::chrono::milliseconds elapsed) override;

    [[nodiscard]] protocol::ddp::LinkState link_state(MonoTime now) const noexcept {
        return consumer_.state(now);
    }
    [[nodiscard]] const protocol::ddp::ConsumerMetrics& metrics() const noexcept {
        return consumer_.metrics();
    }
    [[nodiscard]] std::uint64_t socket_errors() const noexcept { return socket_errors_; }
    [[nodiscard]] std::uint64_t truncated_datagrams() const noexcept {
        return truncated_datagrams_;
    }
    [[nodiscard]] const io::UdpEndpoint& local_endpoint() const noexcept {
        return receiver_.local();
    }

private:
    static void mark_unusable(FlightSnapshot& snapshot, Validity validity) noexcept;

    DdpUdpSourceConfig config_;
    io::UdpReceiver receiver_;
    protocol::ddp::SnapshotConsumer consumer_;
    std::array<std::uint8_t, io::max_udp_payload> buffer_{};
    std::uint64_t socket_errors_{};
    std::uint64_t truncated_datagrams_{};
};

}  // namespace efis
