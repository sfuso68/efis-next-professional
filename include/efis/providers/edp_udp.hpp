#pragma once

#include "efis/io/udp_datagram.hpp"
#include "efis/protocol/edp_v1.hpp"

#include <array>
#include <optional>

namespace efis {

struct EdpUdpPublisherConfig final {
  io::UdpEndpoint destination;
  protocol::edp::PublisherConfig session;
};

class EdpUdpPublisher final {
public:
  explicit EdpUdpPublisher(EdpUdpPublisherConfig config);
  void publish(const engine::EngineSnapshot &snapshot, MonoTime now);
  [[nodiscard]] std::uint64_t datagrams() const noexcept { return datagrams_; }
  [[nodiscard]] std::uint64_t bytes() const noexcept { return bytes_; }

private:
  io::UdpSender sender_;
  protocol::edp::Publisher session_;
  std::uint64_t datagrams_{}, bytes_{};
};

struct EdpUdpSourceConfig final {
  io::UdpEndpoint local{"0.0.0.0", 49'003};
  protocol::edp::ConsumerPolicy consumer{};
  std::size_t max_datagrams_per_poll{64};
};

class EdpUdpEngineSource final {
public:
  explicit EdpUdpEngineSource(EdpUdpSourceConfig config);
  [[nodiscard]] engine::EngineSnapshot poll(MonoTime now);
  [[nodiscard]] protocol::edp::LinkState
  link_state(MonoTime now) const noexcept {
    return consumer_.state(now);
  }
  [[nodiscard]] protocol::edp::LinkStatus
  link_status(MonoTime now) const noexcept {
    return consumer_.status(now);
  }
  [[nodiscard]] std::optional<std::chrono::milliseconds>
  data_age(MonoTime now) const noexcept {
    return consumer_.age(now);
  }
  [[nodiscard]] const protocol::edp::ConsumerMetrics &metrics() const noexcept {
    return consumer_.metrics();
  }
  [[nodiscard]] protocol::edp::ConsumerDiagnostics
  diagnostics(MonoTime now) const noexcept {
    return consumer_.diagnostics(now);
  }
  [[nodiscard]] const io::UdpEndpoint &local_endpoint() const noexcept {
    return receiver_.local();
  }

private:
  static void mark(engine::EngineSnapshot &, Validity) noexcept;
  EdpUdpSourceConfig config_;
  io::UdpReceiver receiver_;
  protocol::edp::Consumer consumer_;
  std::array<std::uint8_t, io::max_udp_payload> buffer_{};
};

} // namespace efis
