#include "efis/core/alert_engine.hpp"

#include <algorithm>

namespace efis {
namespace {

int priority(AlertSeverity severity) {
    return static_cast<int>(severity);
}

bool is_simulated(const Sample<double>& sample) {
    return sample.validity == Validity::simulated;
}

}  // namespace

AlertState AlertEngine::evaluate(const FlightSnapshot& s) const {
    AlertState result;

    if (!s.air_data.indicated_airspeed_kt.usable()) {
        result.active.push_back({"AIRSPEED_FAIL", "AIRSPEED FAIL",
                                 AlertSeverity::warning, false});
    }
    if (!s.air_data.pressure_altitude_ft.usable()) {
        result.active.push_back({"ALTITUDE_FAIL", "ALTITUDE FAIL",
                                 AlertSeverity::warning, false});
    }
    if (!s.navigation.cdi_deviation_dots.usable() ||
        !s.attitude.heading_deg.usable()) {
        result.active.push_back({"NAV_DATA_FAIL", "NAV DATA FAIL",
                                 AlertSeverity::caution, false});
    }
    if (s.health.stale_fields > 0) {
        result.active.push_back({"DATA_STALE", "DATA STALE",
                                 AlertSeverity::caution, false});
    }
    if (is_simulated(s.attitude.roll_deg) ||
        is_simulated(s.air_data.indicated_airspeed_kt) ||
        is_simulated(s.air_data.pressure_altitude_ft)) {
        result.active.push_back({"SIMULATION", "SIMULATION ACTIVE",
                                 AlertSeverity::advisory, false});
    }

    std::stable_sort(result.active.begin(), result.active.end(),
                     [](const Alert& left, const Alert& right) {
                         return priority(left.severity) > priority(right.severity);
                     });
    return result;
}

}  // namespace efis
