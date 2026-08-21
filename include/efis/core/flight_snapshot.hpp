#pragma once

#include "efis/core/sample.hpp"

#include <string>
#include <vector>

namespace efis {

struct AttitudeState final {
    Sample<double> roll_deg;
    Sample<double> pitch_deg;
    Sample<double> heading_deg;
};

struct AirDataState final {
    Sample<double> indicated_airspeed_kt;
    Sample<double> pressure_altitude_ft;
    Sample<double> vertical_speed_fpm;
};

struct PositionState final {
    Sample<double> latitude_deg;
    Sample<double> longitude_deg;
    Sample<double> ground_speed_kt;
};

struct GuidanceState final {
    Sample<bool> flight_director_active;
    Sample<double> commanded_roll_deg;
    Sample<double> commanded_pitch_deg;
    Sample<double> selected_altitude_ft;
    Sample<double> selected_course_deg;
    Sample<double> selected_airspeed_kt;
};

struct NavigationState final {
    Sample<double> cdi_deviation_dots;
    Sample<double> glide_slope_dots;
    Sample<bool> to_flag;
};

enum class AlertSeverity : std::uint8_t { advisory, caution, warning };

struct Alert final {
    std::string code;
    std::string message;
    AlertSeverity severity{AlertSeverity::advisory};
    bool acknowledged{};
};

struct AlertState final {
    std::vector<Alert> active;
};

enum class HealthLevel : std::uint8_t { normal, advisory, caution, warning };

struct SystemHealth final {
    HealthLevel level{HealthLevel::warning};
    std::uint32_t unavailable_fields{};
    std::uint32_t stale_fields{};
};

struct FlightSnapshot final {
    AttitudeState attitude;
    AirDataState air_data;
    PositionState position;
    GuidanceState guidance;
    NavigationState navigation;
    AlertState alerts;
    SystemHealth health;
    MonoTime timestamp{};
    std::uint64_t sequence{};
};

}  // namespace efis
