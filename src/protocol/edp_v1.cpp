#include "efis/protocol/edp_v1.hpp"

#include "efis/protocol/ddp_v2.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

namespace efis::protocol::edp {
namespace {
constexpr std::array<std::uint8_t, 4> magic{'E', 'D', 'P', '1'};
constexpr std::size_t sample_size = 36;
constexpr std::size_t header_size = 4;

void u16(std::vector<std::uint8_t> &o, std::uint16_t v) {
  o.push_back(static_cast<std::uint8_t>(v >> 8));
  o.push_back(static_cast<std::uint8_t>(v));
}
void u32(std::vector<std::uint8_t> &o, std::uint32_t v) {
  for (int s = 24; s >= 0; s -= 8)
    o.push_back(static_cast<std::uint8_t>(v >> s));
}
void u64(std::vector<std::uint8_t> &o, std::uint64_t v) {
  for (int s = 56; s >= 0; s -= 8)
    o.push_back(static_cast<std::uint8_t>(v >> s));
}
void f64(std::vector<std::uint8_t> &o, double v) {
  u64(o, std::bit_cast<std::uint64_t>(v));
}
bool u16(std::span<const std::uint8_t> i, std::size_t &a, std::uint16_t &v) {
  if (a + 2 > i.size())
    return false;
  v = (std::uint16_t(i[a]) << 8) | i[a + 1];
  a += 2;
  return true;
}
bool u32(std::span<const std::uint8_t> i, std::size_t &a, std::uint32_t &v) {
  if (a + 4 > i.size())
    return false;
  v = 0;
  for (int n = 0; n < 4; ++n)
    v = (v << 8) | i[a + n];
  a += 4;
  return true;
}
bool u64(std::span<const std::uint8_t> i, std::size_t &a, std::uint64_t &v) {
  if (a + 8 > i.size())
    return false;
  v = 0;
  for (int n = 0; n < 8; ++n)
    v = (v << 8) | i[a + n];
  a += 8;
  return true;
}
bool f64(std::span<const std::uint8_t> i, std::size_t &a, double &v) {
  std::uint64_t b{};
  if (!u64(i, a, b))
    return false;
  v = std::bit_cast<double>(b);
  return true;
}

std::uint32_t age(const Sample<double> &s, MonoTime now) {
  if (s.received_at == MonoTime{} || now <= s.received_at)
    return 0;
  const auto n =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - s.received_at)
          .count();
  return static_cast<std::uint32_t>(
      std::min<std::int64_t>(n, std::numeric_limits<std::uint32_t>::max()));
}
using Pair = std::pair<SignalId, const Sample<double> *>;
std::array<Pair, engine_sample_count> samples(const engine::EngineSnapshot &s) {
  return {{{SignalId::engine_rpm, &s.rpm},
           {SignalId::manifold_pressure, &s.manifold_pressure_inhg},
           {SignalId::oil_pressure, &s.oil_pressure_psi},
           {SignalId::oil_temperature, &s.oil_temperature_c},
           {SignalId::fuel_pressure, &s.fuel_pressure_psi},
           {SignalId::fuel_flow, &s.fuel_flow_gph},
           {SignalId::fuel_quantity_left, &s.fuel_left_gal},
           {SignalId::fuel_quantity_right, &s.fuel_right_gal},
           {SignalId::bus_voltage, &s.bus_voltage_v},
           {SignalId::alternator_current, &s.alternator_current_a},
           {SignalId::cylinder_head_temperature_1, &s.cht_c[0]},
           {SignalId::cylinder_head_temperature_2, &s.cht_c[1]},
           {SignalId::cylinder_head_temperature_3, &s.cht_c[2]},
           {SignalId::cylinder_head_temperature_4, &s.cht_c[3]},
           {SignalId::exhaust_gas_temperature_1, &s.egt_c[0]},
           {SignalId::exhaust_gas_temperature_2, &s.egt_c[1]},
           {SignalId::exhaust_gas_temperature_3, &s.egt_c[2]},
           {SignalId::exhaust_gas_temperature_4, &s.egt_c[3]}}};
}
Sample<double> *target(engine::EngineSnapshot &s, SignalId id) {
  switch (id) {
  case SignalId::engine_rpm:
    return &s.rpm;
  case SignalId::manifold_pressure:
    return &s.manifold_pressure_inhg;
  case SignalId::oil_pressure:
    return &s.oil_pressure_psi;
  case SignalId::oil_temperature:
    return &s.oil_temperature_c;
  case SignalId::fuel_pressure:
    return &s.fuel_pressure_psi;
  case SignalId::fuel_flow:
    return &s.fuel_flow_gph;
  case SignalId::fuel_quantity_left:
    return &s.fuel_left_gal;
  case SignalId::fuel_quantity_right:
    return &s.fuel_right_gal;
  case SignalId::bus_voltage:
    return &s.bus_voltage_v;
  case SignalId::alternator_current:
    return &s.alternator_current_a;
  case SignalId::cylinder_head_temperature_1:
    return &s.cht_c[0];
  case SignalId::cylinder_head_temperature_2:
    return &s.cht_c[1];
  case SignalId::cylinder_head_temperature_3:
    return &s.cht_c[2];
  case SignalId::cylinder_head_temperature_4:
    return &s.cht_c[3];
  case SignalId::exhaust_gas_temperature_1:
    return &s.egt_c[0];
  case SignalId::exhaust_gas_temperature_2:
    return &s.egt_c[1];
  case SignalId::exhaust_gas_temperature_3:
    return &s.egt_c[2];
  case SignalId::exhaust_gas_temperature_4:
    return &s.egt_c[3];
  default:
    return nullptr;
  }
}
std::size_t signal_index(SignalId id) {
  const auto first = static_cast<std::uint16_t>(SignalId::engine_rpm);
  const auto value = static_cast<std::uint16_t>(id);
  if (value >= first &&
      value <= static_cast<std::uint16_t>(SignalId::alternator_current))
    return value - first;
  const auto cht =
      static_cast<std::uint16_t>(SignalId::cylinder_head_temperature_1);
  if (value >= cht && value <= static_cast<std::uint16_t>(
                                   SignalId::cylinder_head_temperature_4))
    return 10 + (value - cht);
  const auto egt =
      static_cast<std::uint16_t>(SignalId::exhaust_gas_temperature_1);
  if (value >= egt &&
      value <= static_cast<std::uint16_t>(SignalId::exhaust_gas_temperature_4))
    return 14 + (value - egt);
  return engine_sample_count;
}
Unit expected_unit(SignalId id) {
  switch (id) {
  case SignalId::engine_rpm:
    return Unit::revolutions_per_minute;
  case SignalId::manifold_pressure:
    return Unit::inch_mercury;
  case SignalId::oil_pressure:
  case SignalId::fuel_pressure:
    return Unit::pound_per_square_inch;
  case SignalId::fuel_flow:
    return Unit::gallon_per_hour;
  case SignalId::fuel_quantity_left:
  case SignalId::fuel_quantity_right:
    return Unit::gallon;
  case SignalId::bus_voltage:
    return Unit::volt;
  case SignalId::alternator_current:
    return Unit::ampere;
  case SignalId::oil_temperature:
  case SignalId::cylinder_head_temperature_1:
  case SignalId::cylinder_head_temperature_2:
  case SignalId::cylinder_head_temperature_3:
  case SignalId::cylinder_head_temperature_4:
  case SignalId::exhaust_gas_temperature_1:
  case SignalId::exhaust_gas_temperature_2:
  case SignalId::exhaust_gas_temperature_3:
  case SignalId::exhaust_gas_temperature_4:
    return Unit::celsius;
  default:
    return Unit::unknown;
  }
}
MonoTime sampled(MonoTime received, std::uint32_t ms) {
  const auto d = std::chrono::milliseconds(ms);
  return d > received.time_since_epoch() ? MonoTime{} : received - d;
}
} // namespace

std::vector<std::uint8_t>
encode_snapshot(const engine::EngineSnapshot &s, std::uint32_t node,
                std::uint64_t instance, std::uint64_t sequence, MonoTime now) {
  std::vector<std::uint8_t> p;
  p.reserve(header_size + engine_sample_count * sample_size);
  p.push_back(static_cast<std::uint8_t>(s.health));
  p.push_back(static_cast<std::uint8_t>(s.injected_fault));
  u16(p, engine_sample_count);
  for (const auto &[id, sp] : samples(s)) {
    const auto &x = *sp;
    u16(p, static_cast<std::uint16_t>(id));
    f64(p, x.value);
    u16(p, static_cast<std::uint16_t>(x.source));
    p.push_back(static_cast<std::uint8_t>(x.validity));
    p.push_back(static_cast<std::uint8_t>(x.quality));
    p.push_back(static_cast<std::uint8_t>(x.origin.unit));
    u32(p, age(x, now));
    p.push_back(x.is_simulated());
    u16(p, x.origin.node);
    u16(p, x.origin.channel);
    u32(p, x.origin.calibration_revision);
    f64(p, x.raw);
  }
  std::vector<std::uint8_t> o;
  o.insert(o.end(), magic.begin(), magic.end());
  u16(o, schema_version);
  u32(o, node);
  u64(o, instance);
  u64(o, sequence);
  const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                      now.time_since_epoch())
                      .count();
  u64(o, std::max<std::int64_t>(0, us));
  u32(o, static_cast<std::uint32_t>(p.size()));
  u32(o, ddp::crc32c(p));
  o.insert(o.end(), p.begin(), p.end());
  return o;
}

DecodeResult decode_snapshot(std::span<const std::uint8_t> b,
                             MonoTime received) {
  DecodeResult r;
  if (b.size() < envelope_size) {
    r.error = DecodeError::truncated;
    return r;
  }
  if (!std::equal(magic.begin(), magic.end(), b.begin())) {
    r.error = DecodeError::bad_magic;
    return r;
  }
  std::size_t a = 4;
  std::uint16_t ver{};
  if (!u16(b, a, ver) || ver != schema_version) {
    r.error = DecodeError::unsupported_schema;
    return r;
  }
  auto &e = r.message.envelope;
  if (!u32(b, a, e.source_node) || !u64(b, a, e.publisher_instance) ||
      !u64(b, a, e.sequence) || !u64(b, a, e.monotonic_us) ||
      !u32(b, a, e.payload_length) || !u32(b, a, e.payload_crc32c)) {
    r.error = DecodeError::truncated;
    return r;
  }
  if (!e.source_node || !e.publisher_instance || !e.sequence) {
    r.error = DecodeError::invalid_identity;
    return r;
  }
  if (b.size() != envelope_size + e.payload_length) {
    r.error = DecodeError::length_mismatch;
    return r;
  }
  const auto p = b.subspan(envelope_size);
  if (ddp::crc32c(p) != e.payload_crc32c) {
    r.error = DecodeError::crc_mismatch;
    return r;
  }
  a = 0;
  if (p.size() < header_size) {
    r.error = DecodeError::malformed_payload;
    return r;
  }
  if (p[a] > static_cast<std::uint8_t>(engine::EngineHealth::warning) ||
      p[a + 1] >
          static_cast<std::uint8_t>(engine::EngineFault::low_fuel_pressure)) {
    r.error = DecodeError::malformed_payload;
    return r;
  }
  auto &s = r.message.snapshot;
  s.health = static_cast<engine::EngineHealth>(p[a++]);
  s.injected_fault = static_cast<engine::EngineFault>(p[a++]);
  std::uint16_t count{};
  if (!u16(p, a, count) || count != engine_sample_count ||
      p.size() != header_size + count * sample_size) {
    r.error = DecodeError::malformed_payload;
    return r;
  }
  std::array<bool, engine_sample_count> seen{};
  for (std::size_t n = 0; n < count; ++n) {
    std::uint16_t idv{}, src{}, node{}, channel{};
    double value{}, raw{};
    std::uint32_t age_ms{}, revision{};
    if (!u16(p, a, idv) || !f64(p, a, value) || !u16(p, a, src) ||
        a + 3 > p.size()) {
      r.error = DecodeError::malformed_payload;
      return r;
    }
    auto id = static_cast<SignalId>(idv);
    auto *out = target(s, id);
    if (!out) {
      r.error = DecodeError::malformed_payload;
      return r;
    }
    const auto index = signal_index(id);
    if (index >= seen.size() || seen[index] ||
        src > static_cast<std::uint16_t>(SourceId::blackbox_broker) ||
        p[a] > static_cast<std::uint8_t>(Validity::degraded) ||
        p[a + 1] > static_cast<std::uint8_t>(Quality::high) ||
        p[a + 2] > static_cast<std::uint8_t>(Unit::pascal)) {
      r.error = DecodeError::malformed_payload;
      return r;
    }
    seen[index] = true;
    const auto validity = static_cast<Validity>(p[a++]);
    const auto quality = static_cast<Quality>(p[a++]);
    const auto unit = static_cast<Unit>(p[a++]);
    if (!u32(p, a, age_ms) || a >= p.size()) {
      r.error = DecodeError::malformed_payload;
      return r;
    }
    const bool sim = p[a++] != 0;
    if (unit != expected_unit(id) || !u16(p, a, node) || !u16(p, a, channel) ||
        !u32(p, a, revision) || !f64(p, a, raw)) {
      r.error = DecodeError::malformed_payload;
      return r;
    }
    out->value = value;
    out->source = static_cast<SourceId>(src);
    out->sampled_at = sampled(received, age_ms);
    out->received_at = received;
    out->quality = quality;
    out->validity = validity;
    out->sequence = e.sequence;
    out->origin = {id, node, channel, unit, revision, sim};
    out->raw = raw;
  }
  s.sequence = e.sequence;
  s.timestamp = received;
  return r;
}

Publisher::Publisher(PublisherConfig c)
    : config_(c), next_sequence_(c.first_sequence) {
  if (!c.source_node || c.source_node > std::numeric_limits<NodeId>::max() ||
      !c.publisher_instance || !c.first_sequence)
    throw std::invalid_argument(
        "EDP publisher identity must be non-zero and fit NodeId");
}
std::vector<std::uint8_t> Publisher::publish(const engine::EngineSnapshot &s,
                                             MonoTime now) {
  if (next_sequence_ == std::numeric_limits<std::uint64_t>::max())
    throw std::overflow_error("EDP sequence exhausted");
  return encode_snapshot(s, config_.source_node, config_.publisher_instance,
                         next_sequence_++, now);
}
Consumer::Consumer(ConsumerPolicy p) : policy_(p) {
  if (p.stale_after <= std::chrono::milliseconds::zero() ||
      p.lost_after <= p.stale_after)
    throw std::invalid_argument(
        "EDP timeout policy must satisfy 0 < stale < lost");
}
DecodeError Consumer::consume(std::span<const std::uint8_t> d, MonoTime now) {
  ++metrics_.datagrams;
  auto x = decode_snapshot(d, now);
  if (!x) {
    ++metrics_.decode_rejected;
    if (x.error == DecodeError::crc_mismatch)
      ++metrics_.crc_rejected;
    return x.error;
  }
  auto &e = x.message.envelope;
  if (policy_.expected_source_node &&
      e.source_node != policy_.expected_source_node) {
    ++metrics_.identity_rejected;
    return DecodeError::invalid_identity;
  }
  const auto exact = std::find_if(
      sequences_.begin(), sequences_.end(), [&](const SequenceState &state) {
        return state.node == e.source_node &&
               state.instance == e.publisher_instance;
      });
  if (exact != sequences_.end()) {
    if (!exact->current || e.sequence < exact->sequence) {
      ++metrics_.reversed;
      return DecodeError::reversed_sequence;
    }
    if (e.sequence == exact->sequence) {
      ++metrics_.duplicates;
      return DecodeError::duplicate_sequence;
    }
    if (e.sequence > exact->sequence + 1)
      metrics_.sequence_gaps += e.sequence - exact->sequence - 1;
    exact->sequence = e.sequence;
  } else {
    const auto current = std::find_if(
        sequences_.begin(), sequences_.end(), [&](const SequenceState &state) {
          return state.node == e.source_node && state.current;
        });
    if (current != sequences_.end()) {
      current->current = false;
      ++metrics_.publisher_restarts;
    }
    sequences_.push_back(
        {e.source_node, e.publisher_instance, e.sequence, true});
  }
  latest_ = std::move(x.message.snapshot);
  last_ = e;
  last_received_ = now;
  has_snapshot_ = true;
  ++metrics_.accepted;
  return DecodeError::none;
}
LinkState Consumer::state(MonoTime now) const noexcept {
  if (!has_snapshot_)
    return LinkState::waiting;
  if (now <= last_received_ || now - last_received_ <= policy_.stale_after)
    return LinkState::live;
  if (now - last_received_ <= policy_.lost_after)
    return LinkState::stale;
  return LinkState::lost;
}
std::optional<std::chrono::milliseconds>
Consumer::age(MonoTime now) const noexcept {
  if (!has_snapshot_)
    return std::nullopt;
  if (now <= last_received_)
    return std::chrono::milliseconds::zero();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                               last_received_);
}
const engine::EngineSnapshot *Consumer::latest() const noexcept {
  return has_snapshot_ ? &latest_ : nullptr;
}
} // namespace efis::protocol::edp
