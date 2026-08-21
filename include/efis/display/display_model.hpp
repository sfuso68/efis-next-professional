#pragma once

#include "efis/core/flight_snapshot.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace efis::display {

enum class DisplayProfile : std::uint8_t { standard, professional, degraded };

struct DisplayField final {
    double value{};
    Validity validity{Validity::unavailable};
    SourceId source{SourceId::unknown};
};

struct PfdView final {
    DisplayField roll_deg;
    DisplayField pitch_deg;
    DisplayField heading_deg;
    DisplayField airspeed_kt;
    DisplayField altitude_ft;
    DisplayField vertical_speed_fpm;
    DisplayField ground_speed_kt;
    DisplayField commanded_roll_deg;
    DisplayField commanded_pitch_deg;
    DisplayField selected_altitude_ft;
    DisplayField selected_course_deg;
    bool flight_director_active{};
    std::vector<Alert> alerts;
    HealthLevel health{HealthLevel::warning};
    DisplayProfile profile{DisplayProfile::standard};
    std::uint64_t sequence{};
};

[[nodiscard]] PfdView make_pfd_view(const FlightSnapshot& snapshot,
                                    DisplayProfile profile);
[[nodiscard]] const char* validity_label(Validity validity) noexcept;

}  // namespace efis::display
