#pragma once

#include "efis/protocol/ddp_v2.hpp"

#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

namespace efis::protocol::ddp {

// Il session layer non apre socket: conserva il confine fissato da ADR-003.
// Un adapter UDP, un replay o un test gli consegnano un datagramma intero e
// ricevono lo stesso risultato, senza portare il trasporto dentro al core.
struct PublisherConfig final {
    Profile profile{Profile::professional};
    std::uint32_t source_node{};
    std::uint64_t publisher_instance{};
    std::uint64_t first_sequence{1};
};

class SnapshotPublisher final {
public:
    explicit SnapshotPublisher(PublisherConfig config);

    [[nodiscard]] std::vector<std::uint8_t> publish(
        const FlightSnapshot& snapshot, MonoTime published_at);
    [[nodiscard]] std::uint64_t next_sequence() const noexcept { return next_sequence_; }

private:
    PublisherConfig config_;
    std::uint64_t next_sequence_{};
};

enum class LinkState : std::uint8_t { waiting, live, stale, lost };

struct ConsumerPolicy final {
    std::chrono::milliseconds stale_after{250};
    std::chrono::milliseconds lost_after{1000};
    // Zero accetta qualsiasi nodo. In installazione il display deve impostare
    // l'identita' della propria blackbox, evitando publisher estranei.
    std::uint32_t expected_source_node{};
};

struct ConsumerMetrics final {
    std::uint64_t datagrams{};
    std::uint64_t accepted{};
    std::uint64_t decode_rejected{};
    std::uint64_t crc_rejected{};
    std::uint64_t identity_rejected{};
    std::uint64_t duplicates{};
    std::uint64_t reversed{};
    std::uint64_t sequence_gaps{};
    std::uint64_t publisher_restarts{};
};

struct ConsumeResult final {
    DecodeError error{DecodeError::none};
    std::uint64_t missing_before{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == DecodeError::none;
    }
};

class SnapshotConsumer final {
public:
    explicit SnapshotConsumer(ConsumerPolicy policy = {});

    [[nodiscard]] ConsumeResult consume(std::span<const std::uint8_t> datagram,
                                        MonoTime received_at);
    [[nodiscard]] LinkState state(MonoTime now) const noexcept;
    [[nodiscard]] const FlightSnapshot* latest() const noexcept;
    [[nodiscard]] const ConsumerMetrics& metrics() const noexcept { return metrics_; }
    [[nodiscard]] MonoTime last_received_at() const noexcept { return last_received_at_; }
    void reset() noexcept;

private:
    ConsumerPolicy policy_;
    SequenceGate sequence_gate_;
    ConsumerMetrics metrics_{};
    FlightSnapshot latest_{};
    MonoTime last_received_at_{};
    Envelope last_envelope_{};
    bool has_snapshot_{};
};

}  // namespace efis::protocol::ddp
