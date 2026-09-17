#pragma once

#include "efis/engine/engine_snapshot.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace efis::protocol::edp {

inline constexpr std::uint16_t schema_version = 1;
inline constexpr std::size_t envelope_size = 42;
inline constexpr std::size_t engine_sample_count = 18;

enum class DecodeError : std::uint8_t {
  none,
  truncated,
  bad_magic,
  unsupported_schema,
  invalid_identity,
  length_mismatch,
  crc_mismatch,
  malformed_payload,
  duplicate_sequence,
  reversed_sequence
};

struct Envelope final {
  std::uint32_t source_node{};
  std::uint64_t publisher_instance{};
  std::uint64_t sequence{};
  std::uint64_t monotonic_us{};
  std::uint32_t payload_length{};
  std::uint32_t payload_crc32c{};
};

struct Message final {
  Envelope envelope{};
  engine::EngineSnapshot snapshot{};
};

struct DecodeResult final {
  DecodeError error{DecodeError::none};
  Message message{};
  [[nodiscard]] explicit operator bool() const noexcept {
    return error == DecodeError::none;
  }
};

[[nodiscard]] std::vector<std::uint8_t>
encode_snapshot(const engine::EngineSnapshot &snapshot,
                std::uint32_t source_node, std::uint64_t publisher_instance,
                std::uint64_t sequence, MonoTime published_at);
[[nodiscard]] DecodeResult decode_snapshot(std::span<const std::uint8_t> bytes,
                                           MonoTime received_at);

enum class LinkState : std::uint8_t { waiting, live, stale, lost };

[[nodiscard]] constexpr std::string_view
link_state_name(LinkState state) noexcept {
  switch (state) {
  case LinkState::waiting:
    return "WAITING";
  case LinkState::live:
    return "LIVE";
  case LinkState::stale:
    return "STALE";
  case LinkState::lost:
    return "LOST";
  }
  return "LOST";
}

struct LinkStatus final {
  LinkState state{LinkState::waiting};
  std::optional<std::chrono::milliseconds> age{};
};

struct PublisherConfig final {
  std::uint32_t source_node{};
  std::uint64_t publisher_instance{};
  std::uint64_t first_sequence{1};
};

class Publisher final {
public:
  explicit Publisher(PublisherConfig config);
  [[nodiscard]] std::vector<std::uint8_t>
  publish(const engine::EngineSnapshot &snapshot, MonoTime now);

private:
  PublisherConfig config_;
  std::uint64_t next_sequence_{};
};

struct ConsumerPolicy final {
  std::chrono::milliseconds stale_after{250};
  std::chrono::milliseconds lost_after{1000};
  std::uint32_t expected_source_node{};
};

struct ConsumerMetrics final {
  std::uint64_t datagrams{}, accepted{}, decode_rejected{}, crc_rejected{};
  std::uint64_t identity_rejected{}, duplicates{}, reversed{}, sequence_gaps{};
  std::uint64_t publisher_restarts{};
};

class Consumer final {
public:
  explicit Consumer(ConsumerPolicy policy = {});
  [[nodiscard]] DecodeError consume(std::span<const std::uint8_t> datagram,
                                    MonoTime received_at);
  [[nodiscard]] LinkState state(MonoTime now) const noexcept;
  [[nodiscard]] std::optional<std::chrono::milliseconds>
  age(MonoTime now) const noexcept;
  [[nodiscard]] LinkStatus status(MonoTime now) const noexcept;
  [[nodiscard]] const engine::EngineSnapshot *latest() const noexcept;
  [[nodiscard]] const ConsumerMetrics &metrics() const noexcept {
    return metrics_;
  }

private:
  struct SequenceState final {
    std::uint32_t node{};
    std::uint64_t instance{};
    std::uint64_t sequence{};
    bool current{};
  };
  ConsumerPolicy policy_;
  ConsumerMetrics metrics_{};
  std::vector<SequenceState> sequences_;
  Envelope last_{};
  engine::EngineSnapshot latest_{};
  MonoTime last_received_{};
  bool has_snapshot_{};
};

} // namespace efis::protocol::edp
