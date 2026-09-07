#include "efis/providers/edp_udp.hpp"

#include <utility>

namespace efis {
EdpUdpPublisher::EdpUdpPublisher(EdpUdpPublisherConfig c)
    : sender_(std::move(c.destination)), session_(c.session) {}
void EdpUdpPublisher::publish(const engine::EngineSnapshot &s, MonoTime now) {
  const auto d = session_.publish(s, now);
  sender_.send(d);
  ++datagrams_;
  bytes_ += d.size();
}
EdpUdpEngineSource::EdpUdpEngineSource(EdpUdpSourceConfig c)
    : config_(std::move(c)), receiver_(config_.local),
      consumer_(config_.consumer) {
  if (!config_.max_datagrams_per_poll)
    config_.max_datagrams_per_poll = 1;
}
engine::EngineSnapshot EdpUdpEngineSource::poll(MonoTime now) {
  for (std::size_t n = 0; n < config_.max_datagrams_per_poll; ++n) {
    const auto r = receiver_.receive(buffer_);
    if (r.status == io::DatagramStatus::would_block)
      break;
    if (r.status != io::DatagramStatus::received)
      continue;
    (void)consumer_.consume({buffer_.data(), r.size}, now);
  }
  const auto *latest = consumer_.latest();
  if (!latest)
    return {};
  auto out = *latest;
  switch (consumer_.state(now)) {
  case protocol::edp::LinkState::waiting:
    return {};
  case protocol::edp::LinkState::live:
    return out;
  case protocol::edp::LinkState::stale:
    mark(out, Validity::stale);
    out.health = engine::EngineHealth::caution;
    return out;
  case protocol::edp::LinkState::lost:
    mark(out, Validity::unavailable);
    out.health = engine::EngineHealth::warning;
    return out;
  }
  return {};
}
void EdpUdpEngineSource::mark(engine::EngineSnapshot &s, Validity v) noexcept {
  auto apply = [&](Sample<double> &x) { x.validity = v; };
  apply(s.rpm);
  apply(s.manifold_pressure_inhg);
  apply(s.oil_pressure_psi);
  apply(s.oil_temperature_c);
  apply(s.fuel_pressure_psi);
  apply(s.fuel_flow_gph);
  apply(s.fuel_left_gal);
  apply(s.fuel_right_gal);
  apply(s.bus_voltage_v);
  apply(s.alternator_current_a);
  for (auto &x : s.cht_c)
    apply(x);
  for (auto &x : s.egt_c)
    apply(x);
}
} // namespace efis
