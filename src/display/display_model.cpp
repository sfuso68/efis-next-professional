#include "efis/display/display_model.hpp"

namespace efis::display {
namespace {

DisplayField field(const Sample<double>& sample) {
    return {sample.value, sample.validity, sample.source};
}

}  // namespace

PfdView make_pfd_view(const FlightSnapshot& snapshot, DisplayProfile profile) {
    return {field(snapshot.attitude.roll_deg),
            field(snapshot.attitude.pitch_deg),
            field(snapshot.attitude.heading_deg),
            field(snapshot.air_data.indicated_airspeed_kt),
            field(snapshot.air_data.pressure_altitude_ft),
            field(snapshot.air_data.vertical_speed_fpm),
            field(snapshot.position.ground_speed_kt),
            field(snapshot.guidance.commanded_roll_deg),
            field(snapshot.guidance.commanded_pitch_deg),
            field(snapshot.guidance.selected_altitude_ft),
            field(snapshot.guidance.selected_course_deg),
            snapshot.guidance.flight_director_active.value &&
                snapshot.guidance.flight_director_active.usable(),
            snapshot.alerts.active,
            snapshot.health.level,
            profile,
            snapshot.sequence};
}

const char* validity_label(Validity validity) noexcept {
    switch (validity) {
        case Validity::valid: return "";
        case Validity::simulated: return "SIM";
        case Validity::degraded: return "DEG";
        case Validity::stale: return "STALE";
        case Validity::unavailable: return "FAIL";
        case Validity::out_of_range: return "RANGE";
        case Validity::disagree: return "DISAGREE";
    }
    return "FAIL";
}

}  // namespace efis::display
