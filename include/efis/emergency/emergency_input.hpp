#pragma once

#include "efis/core/flight_snapshot.hpp"

#include <chrono>
#include <string>

namespace efis::emergency {

enum class EmergencyInputHealth {
    ready,
    source_unhealthy,
    stuck_pressed,
    rearming
};

struct EmergencyInputConfig final {
    std::chrono::milliseconds debounce{50};
    std::chrono::milliseconds hold{1200};
    std::chrono::milliseconds stuck{10000};
    std::chrono::milliseconds release_rearm{300};
};

struct EmergencyInputSample final {
    bool pressed{};
    bool source_healthy{};
};

struct EmergencyInputOutput final {
    EmergencyInputHealth health{EmergencyInputHealth::source_unhealthy};
    bool armed{};
    bool stable_pressed{};
    bool activation_edge{};
    std::chrono::milliseconds pressed_for{};
    std::string diagnostic{"SOURCE UNHEALTHY"};
};

// Deterministic conditioning for a future physical Emergency input. This class
// does not know GPIO, USB or CAN and cannot issue any flight-control command.
class EmergencyInputMonitor final {
public:
    explicit EmergencyInputMonitor(EmergencyInputConfig config = {});

    [[nodiscard]] const EmergencyInputOutput& update(
        EmergencyInputSample sample, MonoTime now);
    [[nodiscard]] const EmergencyInputOutput& output() const noexcept { return output_; }

private:
    EmergencyInputConfig config_{};
    EmergencyInputOutput output_{};
    bool initialized_{};
    bool raw_pressed_{};
    bool stable_pressed_{};
    bool armed_{};
    bool activation_sent_{};
    MonoTime raw_changed_at_{};
    MonoTime stable_changed_at_{};
};

[[nodiscard]] const char* input_health_name(EmergencyInputHealth health) noexcept;

}  // namespace efis::emergency
