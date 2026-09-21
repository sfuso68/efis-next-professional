#pragma once

#include <chrono>

namespace efis {

// One timing contract for the distributed data path. Different layers use
// explicit multiples because they answer different questions: datagram flow,
// sample usability, node heartbeat and slow bench inspection.
struct FreshnessBudget final {
    static constexpr auto tick = std::chrono::milliseconds{250};
    static constexpr auto network_age_margin = tick;
    static constexpr auto transport_stale = tick;
    static constexpr auto sample_stale = tick * 2;
    static constexpr auto transport_lost = tick * 4;
    static constexpr auto node_stale = tick * 5;
    static constexpr auto bench_stale = tick * 8;
    static constexpr auto node_lost = tick * 12;
};

static_assert(FreshnessBudget::transport_stale < FreshnessBudget::sample_stale);
static_assert(FreshnessBudget::sample_stale < FreshnessBudget::transport_lost);
static_assert(FreshnessBudget::transport_lost < FreshnessBudget::node_stale);
static_assert(FreshnessBudget::node_stale < FreshnessBudget::node_lost);

}  // namespace efis
