#include "efis/providers/simulation_provider.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace efis {
namespace {

template <class T>
Sample<T> simulated(T value, SignalId signal, Unit unit, MonoTime now,
                    std::uint64_t sequence) {
    Sample<T> sample{value, SourceId::simulation, now, now, Quality::high,
                     Validity::simulated, sequence};
    sample.origin.signal = signal;
    sample.origin.unit = unit;
    // Marcatore esplicito, oltre a Validity::simulated: sopravvive
    // all'invecchiamento del campione, che altrimenti lo cancellerebbe.
    sample.origin.simulated = true;
    // Nella simulazione non esiste una conversione da valore grezzo: i due
    // valori coincidono, e dichiararlo e' piu' onesto che lasciare raw a zero.
    sample.raw = static_cast<double>(value);
    return sample;
}

struct SpeedKeyframe {
    double second;
    double knots;
};

double airspeed_profile(double seconds) {
    constexpr std::array<SpeedKeyframe, 11> profile{{
        {0.0, 45.0}, {8.0, 55.0}, {16.0, 62.0}, {24.0, 68.0},
        {36.0, 105.0}, {48.0, 125.0}, {58.0, 145.0},
        {68.0, 115.0}, {80.0, 85.0}, {90.0, 55.0}, {96.0, 45.0}
    }};
    const double cycle = std::fmod(std::max(0.0, seconds), profile.back().second);
    for (std::size_t index = 1; index < profile.size(); ++index) {
        if (cycle <= profile[index].second) {
            const auto& from = profile[index - 1];
            const auto& to = profile[index];
            const double ratio = (cycle - from.second) / (to.second - from.second);
            const double smooth = ratio * ratio * (3.0 - 2.0 * ratio);
            return from.knots + (to.knots - from.knots) * smooth;
        }
    }
    return profile.back().knots;
}

double environment_number(const char* name, double fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') return fallback;
    char* end = nullptr;
    const double parsed = std::strtod(value, &end);
    return end != value && std::isfinite(parsed) ? parsed : fallback;
}

}  // namespace

SimulationProvider::SimulationProvider(std::uint64_t seed) noexcept : seed_(seed) {}

FlightSnapshot SimulationProvider::sample(MonoTime now,
                                           std::chrono::milliseconds elapsed) {
    ++sequence_;
    const auto seconds = static_cast<double>(elapsed.count()) / 1000.0;
    const auto phase = seconds + static_cast<double>(seed_ % 360U);

    FlightSnapshot out;
    out.sequence = sequence_;
    out.timestamp = now;
    out.attitude.roll_deg = simulated(12.0 * std::sin(phase * 0.25), SignalId::roll, Unit::degree, now, sequence_);
    out.attitude.pitch_deg = simulated(3.0 * std::sin(phase * 0.17), SignalId::pitch, Unit::degree, now, sequence_);
    // Default development flight: Treviso (LIPH) to Belluno (LIDB). The route
    // crosses both prepared Veneto terrain tiles and exercises climb, cruise,
    // descent and arrival phases. Environment overrides remain available for
    // deterministic display tests.
    constexpr double departureLat = 45.648399;
    constexpr double departureLon = 12.194400;
    constexpr double destinationLat = 46.166549;
    constexpr double destinationLon = 12.250389;
    constexpr double scenarioCourse = 4.2;
    constexpr double routeDurationSeconds = 600.0;
    const double routeProgress = std::clamp(
        std::fmod(std::max(0.0, seconds), routeDurationSeconds) / routeDurationSeconds,
        0.0, 1.0);
    const double simulatedHeading = environment_number("EFIS_SIM_HEADING_DEG", scenarioCourse);
    out.attitude.heading_deg = simulated(simulatedHeading + 1.5 * std::sin(phase * 0.035), SignalId::heading, Unit::degree, now, sequence_);
    const double indicatedAirspeed = airspeed_profile(seconds);
    out.air_data.indicated_airspeed_kt = simulated(indicatedAirspeed, SignalId::indicated_airspeed, Unit::knot, now, sequence_);
    constexpr double pi = 3.141592653589793;
    const double smoothProgress = routeProgress * routeProgress * (3.0 - 2.0 * routeProgress);
    const double lateralOffset = 0.0020 * std::sin(routeProgress * pi);
    const double routeLatitude = departureLat + (destinationLat-departureLat)*smoothProgress;
    const double routeLongitude = departureLon + (destinationLon-departureLon)*smoothProgress + lateralOffset;
    double routeAltitude = 5500.0;
    double routeVerticalSpeed = 0.0;
    if (routeProgress < 0.20) {
        const double climb = routeProgress / 0.20;
        routeAltitude = 59.0 + (5500.0-59.0)*climb;
        routeVerticalSpeed = 900.0;
    } else if (routeProgress > 0.72) {
        const double descent = (routeProgress-0.72)/0.28;
        routeAltitude = 5500.0 + (1240.0-5500.0)*descent;
        routeVerticalSpeed = -750.0;
    }
    routeAltitude += 35.0 * std::sin(phase * 0.045);
    const double simulatedAltitude = environment_number("EFIS_SIM_ALTITUDE_FT", routeAltitude);
    out.air_data.pressure_altitude_ft = simulated(simulatedAltitude, SignalId::pressure_altitude, Unit::foot, now, sequence_);
    out.air_data.vertical_speed_fpm = simulated(routeVerticalSpeed, SignalId::vertical_speed, Unit::foot_per_minute, now, sequence_);
    out.position.latitude_deg = simulated(routeLatitude, SignalId::latitude, Unit::degree, now, sequence_);
    out.position.longitude_deg = simulated(routeLongitude, SignalId::longitude, Unit::degree, now, sequence_);
    out.position.ground_speed_kt = simulated(std::max(0.0, indicatedAirspeed + 4.0), SignalId::ground_speed, Unit::knot, now, sequence_);
    out.guidance.flight_director_active = simulated(true, SignalId::flight_director_active, Unit::boolean, now, sequence_);
    out.guidance.commanded_roll_deg = simulated(4.0, SignalId::commanded_roll, Unit::degree, now, sequence_);
    out.guidance.commanded_pitch_deg = simulated(1.5, SignalId::commanded_pitch, Unit::degree, now, sequence_);
    out.guidance.selected_altitude_ft = simulated(5500.0, SignalId::selected_altitude, Unit::foot, now, sequence_);
    out.guidance.selected_course_deg = simulated(scenarioCourse, SignalId::selected_course, Unit::degree, now, sequence_);
    out.guidance.selected_airspeed_kt = simulated(
        indicatedAirspeed < 60.0 ? 55.0 : indicatedAirspeed < 90.0 ? 68.0
        : indicatedAirspeed < 120.0 ? 105.0 : 125.0, SignalId::selected_airspeed, Unit::knot, now, sequence_);
    out.navigation.cdi_deviation_dots = simulated(0.8 * std::sin(phase * 0.08), SignalId::cdi_deviation, Unit::dot, now, sequence_);
    out.navigation.glide_slope_dots = simulated(0.3 * std::cos(phase * 0.06), SignalId::glide_slope_deviation, Unit::dot, now, sequence_);
    out.navigation.to_flag = simulated(true, SignalId::to_flag, Unit::boolean, now, sequence_);
    out.health.level = HealthLevel::normal;
    return out;
}

}  // namespace efis
