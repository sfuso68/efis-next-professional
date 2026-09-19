#pragma once

#include "efis/core/sample.hpp"

#include <chrono>
#include <cstdint>

namespace efis {

struct EngineIntegrationInput final {
    double accountable_rpm{};
    double fuel_consumed_gallons{};
    bool runtime_usable{};
    bool fuel_usable{};
    bool simulated{};
};

[[nodiscard]] EngineIntegrationInput engine_integration_input(
    const Sample<double>& rpm, const Sample<double>& fuel_flow_gph,
    double elapsed_seconds) noexcept;

// Updates a six-second IAS projection. Invalid samples break the derivative
// chain: the next valid sample establishes a new baseline instead of comparing
// against a stale residual value.
[[nodiscard]] bool update_airspeed_trend(
    const Sample<double>& indicated_airspeed_kt,
    double& previous_airspeed_kt, double& filtered_projection_kt,
    bool& initialized) noexcept;

// The recorder publishes a wall-clock timestamp, but consumers must not use
// wall time to measure liveness: NTP/GNSS corrections can move it backwards.
// A changed publication token is observed against the local monotonic clock.
class PublicationFreshness final {
public:
    void observe(std::uint64_t publication_token, MonoTime now) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool fresh(MonoTime now,
                             std::chrono::milliseconds budget) const noexcept;

private:
    std::uint64_t token_{};
    MonoTime observed_at_{};
    bool observed_{};
};

}  // namespace efis
