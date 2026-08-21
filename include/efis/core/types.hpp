#pragma once

#include <chrono>
#include <cstdint>

namespace efis {

using MonoClock = std::chrono::steady_clock;
using MonoTime = MonoClock::time_point;

enum class SourceId : std::uint16_t {
    unknown,
    simulation,
    pixhawk_primary,
    esp32_legacy,
    dlvr_legacy,
    nav_rf,
    nav_synthetic
};

enum class Quality : std::uint8_t { unknown, poor, nominal, high };

enum class Validity : std::uint8_t {
    valid,
    stale,
    unavailable,
    out_of_range,
    disagree,
    simulated,
    degraded
};

constexpr bool is_usable(Validity value) noexcept {
    return value == Validity::valid || value == Validity::simulated ||
           value == Validity::degraded;
}

}  // namespace efis
