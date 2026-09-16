#include "efis/core/engine_runtime.hpp"

#include <algorithm>
#include <cmath>

namespace efis::engine {

RuntimeTracker::RuntimeTracker(RuntimeConfig config, RuntimeTotals initial)
    : config_(config), totals_(initial) {
    if (!std::isfinite(config_.engine_run_threshold_rpm) ||
        config_.engine_run_threshold_rpm < 0.0) {
        config_.engine_run_threshold_rpm = 500.0;
    }
    if (!std::isfinite(config_.tach_reference_rpm) ||
        config_.tach_reference_rpm <= 0.0) {
        config_.tach_reference_rpm = 5000.0;
    }
}

void RuntimeTracker::update(double rpm, double elapsed_seconds, bool airborne) {
    if (!std::isfinite(elapsed_seconds) || elapsed_seconds <= 0.0) return;
    const double hours = elapsed_seconds / 3600.0;
    totals_.master_hours += hours;

    const double usable_rpm = std::isfinite(rpm) ? std::max(0.0, rpm) : 0.0;
    engine_running_ = usable_rpm >= config_.engine_run_threshold_rpm;
    if (engine_running_) {
        totals_.engine_hours += hours;
        totals_.tach_hours += hours * usable_rpm / config_.tach_reference_rpm;
    }
    if (airborne) totals_.flight_hours += hours;
}

}  // namespace efis::engine
