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

    // Vero se il valore non descrive il mondo reale. E' un campo separato da
    // `validity` perche' un campione simulato che invecchia diventa `stale` e
    // perderebbe altrimenti ogni traccia di essere simulato, presentandosi come
    // un dato reale scaduto. Il paragrafo 19 dell'handoff lo vieta.
    bool simulated{};
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

// Etichetta da mostrare accanto al valore. A differenza di validity_label tiene
// conto anche del marcatore di simulazione, che deve restare visibile in ogni
// stato di validita'.
[[nodiscard]] const char* field_label(const DisplayField& field) noexcept;

}  // namespace efis::display
