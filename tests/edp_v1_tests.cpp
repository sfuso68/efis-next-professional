#include "efis/protocol/edp_v1.hpp"
#include "efis/providers/edp_udp.hpp"
#include "efis/providers/engine_simulation_provider.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace {
void check(bool ok, const char *text) {
  if (!ok) {
    std::cerr << "FAILED: " << text << '\n';
    std::exit(EXIT_FAILURE);
  }
}
} // namespace

int main() {
  using namespace std::chrono_literals;
  const auto now = efis::MonoTime{} + 10s;
  efis::EngineSimulationProvider sim;
  auto snapshot = sim.sample(now, 10s);
  efis::protocol::edp::Publisher publisher{{47, 1234, 1}};
  const auto wire = publisher.publish(snapshot, now);
  auto decoded = efis::protocol::edp::decode_snapshot(wire, now + 2ms);
  check(bool(decoded), "EDP snapshot decodes");
  check(decoded.message.snapshot.rpm.value == snapshot.rpm.value,
        "RPM survives");
  check(decoded.message.snapshot.rpm.is_simulated(),
        "simulation marker survives");
  check(decoded.message.snapshot.rpm.origin.signal ==
            efis::SignalId::engine_rpm,
        "signal provenance survives");
  auto bad = wire;
  bad.back() ^= 1;
  check(efis::protocol::edp::decode_snapshot(bad, now).error ==
            efis::protocol::edp::DecodeError::crc_mismatch,
        "CRC rejects corruption");
  efis::protocol::edp::Consumer consumer{{250ms, 1000ms, 47}};
  check(!consumer.age(now), "age absent before first accepted snapshot");
  check(consumer.consume(wire, now) == efis::protocol::edp::DecodeError::none,
        "consumer accepts expected publisher");
  check(consumer.age(now - 1ms) == 0ms, "age clamps clock reversal to zero");
  check(consumer.age(now + 200ms) == 200ms, "age follows accepted snapshot");
  check(consumer.state(now + 200ms) == efis::protocol::edp::LinkState::live,
        "link live");
  check(consumer.state(now + 500ms) == efis::protocol::edp::LinkState::stale,
        "link stale");
  check(consumer.state(now + 1500ms) == efis::protocol::edp::LinkState::lost,
        "link lost");
  check(consumer.consume(wire, now + 10ms) ==
            efis::protocol::edp::DecodeError::duplicate_sequence,
        "duplicate rejected");
  efis::protocol::edp::Consumer wrong{{250ms, 1000ms, 99}};
  check(wrong.consume(wire, now) ==
            efis::protocol::edp::DecodeError::invalid_identity,
        "unexpected node rejected");
  const auto next = publisher.publish(snapshot, now + 20ms);
  check(consumer.consume(next, now + 20ms) ==
            efis::protocol::edp::DecodeError::none,
        "next sequence accepted");
  const auto gap =
      efis::protocol::edp::encode_snapshot(snapshot, 47, 1234, 4, now + 30ms);
  check(consumer.consume(gap, now + 30ms) ==
                efis::protocol::edp::DecodeError::none &&
            consumer.metrics().sequence_gaps == 1,
        "sequence gap counted");
  efis::protocol::edp::Publisher restarted{{47, 9999, 1}};
  const auto after_restart = restarted.publish(snapshot, now + 40ms);
  check(consumer.consume(after_restart, now + 40ms) ==
                efis::protocol::edp::DecodeError::none &&
            consumer.metrics().publisher_restarts == 1,
        "publisher restart accepted and counted");
  check(consumer.consume(gap, now + 50ms) ==
            efis::protocol::edp::DecodeError::reversed_sequence,
        "datagram from retired publisher instance rejected");
  efis::EdpUdpSourceConfig source_config;
  source_config.local = {"127.0.0.1", 0};
  source_config.consumer = {50ms, 150ms, 47};
  efis::EdpUdpEngineSource source{source_config};
  check(source.local_endpoint().port != 0,
        "UDP receiver exposes ephemeral port");
  check(!source.data_age(now), "UDP source age absent while waiting");
  efis::EdpUdpPublisher udp{
      {{"127.0.0.1", source.local_endpoint().port}, {47, 5678, 1}}};
  udp.publish(snapshot, now);
  efis::engine::EngineSnapshot received;
  for (int attempt = 0; attempt < 100; ++attempt) {
    received = source.poll(now);
    if (received.rpm.usable())
      break;
    std::this_thread::sleep_for(1ms);
  }
  check(received.rpm.usable(), "UDP loopback receives engine snapshot");
  check(source.data_age(now).value_or(1ms) == 0ms,
        "UDP source exposes fresh sample age");
  check(received.rpm.is_simulated(), "UDP preserves simulation marker");
  received = source.poll(now + 100ms);
  check(source.data_age(now + 100ms).value_or(0ms) == 100ms,
        "UDP source exposes stale sample age");
  check(received.rpm.validity == efis::Validity::stale,
        "UDP source marks stale samples");
  received = source.poll(now + 200ms);
  check(received.rpm.validity == efis::Validity::unavailable,
        "UDP source marks lost samples unavailable");
  std::cout << "EDP V1 tests passed\n";
}
