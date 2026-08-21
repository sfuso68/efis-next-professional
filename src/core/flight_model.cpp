#include "efis/core/flight_model.hpp"
#include "efis/core/alert_engine.hpp"

#include <array>

namespace efis {
namespace {

template <class T>
void age(Sample<T>& sample, MonoTime now, std::chrono::milliseconds limit,
         SystemHealth& health) {
    if (sample.validity == Validity::unavailable) {
        ++health.unavailable_fields;
        return;
    }
    if (now - sample.received_at > limit) {
        sample.validity = Validity::stale;
    }
    if (sample.validity == Validity::stale) {
        ++health.stale_fields;
    }
}

}  // namespace

FlightModel::FlightModel(std::chrono::milliseconds stale_after)
    : stale_after_(stale_after) {}

FlightSnapshot FlightModel::normalize(FlightSnapshot input, MonoTime now) const {
    age(input.attitude.roll_deg, now, stale_after_, input.health);
    age(input.attitude.pitch_deg, now, stale_after_, input.health);
    age(input.attitude.heading_deg, now, stale_after_, input.health);
    age(input.air_data.indicated_airspeed_kt, now, stale_after_, input.health);
    age(input.air_data.pressure_altitude_ft, now, stale_after_, input.health);
    age(input.air_data.vertical_speed_fpm, now, stale_after_, input.health);
    age(input.position.latitude_deg, now, stale_after_, input.health);
    age(input.position.longitude_deg, now, stale_after_, input.health);
    age(input.position.ground_speed_kt, now, stale_after_, input.health);
    age(input.guidance.flight_director_active, now, stale_after_, input.health);
    age(input.guidance.commanded_roll_deg, now, stale_after_, input.health);
    age(input.guidance.commanded_pitch_deg, now, stale_after_, input.health);
    age(input.guidance.selected_altitude_ft, now, stale_after_, input.health);
    age(input.guidance.selected_course_deg, now, stale_after_, input.health);
    age(input.guidance.selected_airspeed_kt, now, stale_after_, input.health);
    age(input.navigation.cdi_deviation_dots, now, stale_after_, input.health);
    age(input.navigation.glide_slope_dots, now, stale_after_, input.health);
    age(input.navigation.to_flag, now, stale_after_, input.health);

    if (input.health.unavailable_fields > 0) {
        input.health.level = HealthLevel::warning;
    } else if (input.health.stale_fields > 0) {
        input.health.level = HealthLevel::caution;
    } else {
        input.health.level = HealthLevel::normal;
    }
    input.timestamp = now;
    input.alerts = AlertEngine{}.evaluate(input);
    return input;
}

}  // namespace efis
