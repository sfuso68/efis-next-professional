#include "efis/simulation/mission_simulator.hpp"
#include "efis/core/altimetry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace efis::simulation {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double earth_m = 6371000.0, knot_mps = 1852.0 / 3600.0, foot_m = .3048;
double radians(double deg) {
    return deg * pi / 180.0;
}
double degrees(double rad) {
    return rad * 180.0 / pi;
}
double heading(double rad) {
    return std::fmod(degrees(rad) + 360.0, 360.0);
}
double approach(double current, double target, double dt, double tau) {
    return target + (current - target) * std::exp(-dt / tau);
}
double slew(double current, double target, double maximum_change) {
    return current + std::clamp(target - current, -maximum_change, maximum_change);
}
template <class T>
Sample<T> sample_of(T value, SignalId id, Unit unit, MonoTime now, std::uint64_t seq) {
    Sample<T> result{value,         SourceId::simulation, now, now,
                     Quality::high, Validity::simulated,  seq};
    result.origin.signal = id;
    result.origin.unit = unit;
    result.origin.simulated = true;
    result.raw = static_cast<double>(value);
    return result;
}
struct State {
    double rpm{}, oil{}, cht{}, egt{}, used{}, battery_used{}, flap{}, gear{1.0};
};
bool supported(MissionFault f) {
    return f >= MissionFault::low_oil_pressure && f <= MissionFault::engine_stop;
}
} // namespace

SimulationClock::SimulationClock(std::chrono::milliseconds duration) : duration_(duration) {
    if (duration.count() <= 0 || duration > std::chrono::hours{24})
        throw std::invalid_argument("invalid simulation duration");
}
void SimulationClock::advance(std::chrono::nanoseconds delta) {
    if (delta.count() < 0)
        throw std::invalid_argument("negative monotonic delta");
    if (!paused_)
        elapsed_ms_ =
            std::min(static_cast<long double>(duration_.count()),
                     elapsed_ms_ + static_cast<long double>(delta.count()) / 1000000.0L * speed_);
}
void SimulationClock::seek(std::chrono::milliseconds time) {
    if (time.count() < 0 || time > duration_)
        throw std::invalid_argument("seek outside mission");
    elapsed_ms_ = static_cast<long double>(time.count());
}
void SimulationClock::set_speed(double speed) {
    if (!std::isfinite(speed) || speed < .125 || speed > 20.0)
        throw std::invalid_argument("invalid simulation speed");
    speed_ = speed;
}
std::chrono::milliseconds SimulationClock::time() const noexcept {
    return std::chrono::milliseconds{static_cast<std::int64_t>(elapsed_ms_)};
}

MissionSimulator::MissionSimulator(MissionProfile profile, ScenarioId scenario, std::uint64_t seed)
    : profile_(std::move(profile)), scenario_(scenario), seed_(seed) {
    const auto& p = profile_;
    const auto range = [](double v, double lo, double hi) {
        return std::isfinite(v) && v >= lo && v <= hi;
    };
    if (p.id.empty() || p.id.size() > 80 ||
        p.id.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.") !=
            std::string::npos ||
        !range(p.departure_lat, -85, 85) || !range(p.destination_lat, -85, 85) ||
        !range(p.departure_lon, -180, 180) || !range(p.destination_lon, -180, 180) ||
        !range(p.departure_ft, -1000, 10000) || !range(p.destination_ft, -1000, 10000) ||
        !range(p.cruise_ft, 0, 18000) ||
        p.cruise_ft <= std::max(p.departure_ft + 150, p.destination_ft + 1000) ||
        !range(p.climb_fpm, 100, 2000) || !range(p.descent_fpm, 100, 2000) ||
        !range(p.cruise_ground_kt, 90, 180) || !range(p.wind_north_kt, -60, 60) ||
        !range(p.wind_east_kt, -60, 60) || !range(p.ambient_c, -30, 45) ||
        !range(p.fuel_left_gal, 0, 100) || !range(p.fuel_right_gal, 0, 100) ||
        !range(p.maximum_flow_gph, .1, 50) || !range(p.battery_ah, 1, 200) ||
        !range(p.load_a, 0, 100))
        throw std::invalid_argument("invalid synthetic mission profile");
    const double a = radians(p.departure_lat), b = radians(p.destination_lat),
                 dl = radians(p.destination_lon - p.departure_lon);
    route_angle_ = std::acos(std::clamp(
        std::sin(a) * std::sin(b) + std::cos(a) * std::cos(b) * std::cos(dl), -1.0, 1.0));
    if (route_angle_ < 1e-6 || route_angle_ * earth_m > 300.0 * 1852.0)
        throw std::invalid_argument("unsupported mission distance");
    const double climb_seconds = (p.cruise_ft - p.departure_ft - 150) / p.climb_fpm * 60;
    const double descent_seconds = (p.cruise_ft - p.destination_ft - 1000) / p.descent_fpm * 60;
    legs_ = {{FlightPhase::cold_and_dark, 15, p.departure_ft, p.departure_ft, 0, 0, 0, 0, 0, true,
              false, false},
             {FlightPhase::engine_start, 15, p.departure_ft, p.departure_ft, 0, 0, 1800, .2, 0,
              true, true, true},
             {FlightPhase::taxi, 45, p.departure_ft, p.departure_ft, 0, 8, 2000, .25, 0, true, true,
              true},
             {FlightPhase::takeoff, 20, p.departure_ft, p.departure_ft, 8, 60, 5800, 1, 10, true,
              true, true},
             {FlightPhase::takeoff, 20, p.departure_ft, p.departure_ft + 150, 60, 75, 5800, 1, 10,
              true, true, true},
             {FlightPhase::climb, climb_seconds, p.departure_ft + 150, p.cruise_ft, 75, 90, 5500,
              .9, 0, false, true, true},
             {FlightPhase::cruise, 0, p.cruise_ft, p.cruise_ft, 90, p.cruise_ground_kt, 5000, .7, 0,
              false, true, true},
             {FlightPhase::descent, descent_seconds, p.cruise_ft, p.destination_ft + 1000,
              p.cruise_ground_kt, 105, 4200, .4, 0, false, true, true},
             {FlightPhase::approach, 114, p.destination_ft + 1000, p.destination_ft + 50, 105, 62,
              4500, .5, 15, true, true, true},
             {FlightPhase::landing, 15, p.destination_ft + 50, p.destination_ft, 62, 45, 2500, .2,
              30, true, true, true},
             {FlightPhase::landing, 20, p.destination_ft, p.destination_ft, 45, 0, 1800, .15, 30,
              true, true, true},
             {FlightPhase::shutdown, 20, p.destination_ft, p.destination_ft, 0, 0, 0, 0, 0, true,
              true, false}};
    double other_m = 0;
    for (const auto& l : legs_)
        other_m += (l.start_kt + l.end_kt) / 2 * knot_mps * l.seconds;
    const double cruise_seconds =
        (route_angle_ * earth_m - other_m) / ((90 + p.cruise_ground_kt) / 2 * knot_mps);
    if (!range(cruise_seconds, 1, 10800))
        throw std::invalid_argument("route too short/long for configured mission profile");
    legs_[6].seconds = cruise_seconds;
    double seconds = 0;
    for (const auto& l : legs_)
        seconds += l.seconds;
    duration_ = std::chrono::milliseconds{static_cast<std::int64_t>(std::ceil(seconds * 1000))};
    reset(scenario);
}
void MissionSimulator::reset(ScenarioId scenario) {
    std::vector<FaultWindow> windows;
    const double t = static_cast<double>(duration_.count()) / 1000;
    switch (scenario) {
    case ScenarioId::full_flight:
        break;
    case ScenarioId::engine_emergency:
        windows = {{MissionFault::low_oil_pressure, .45 * t, .58 * t},
                   {MissionFault::high_cht, .58 * t, .68 * t}};
        break;
    case ScenarioId::electrical_failure:
        windows = {{MissionFault::alternator_failure, .4 * t, .7 * t}};
        break;
    case ScenarioId::sensor_degradation:
        windows = {{MissionFault::ias_lost, .35 * t, .45 * t},
                   {MissionFault::baro_lost, .55 * t, .65 * t},
                   {MissionFault::gnss_lost, .75 * t, .85 * t}};
        break;
    default:
        throw std::invalid_argument("unknown mission scenario");
    }
    set_faults(std::move(windows));
    scenario_ = scenario;
}
void MissionSimulator::set_faults(std::vector<FaultWindow> faults) {
    if (faults.size() > 16)
        throw std::invalid_argument("too many mission faults");
    const double end = static_cast<double>(duration_.count()) / 1000;
    for (const auto& f : faults)
        if (!supported(f.fault) || !std::isfinite(f.begin_seconds) ||
            !std::isfinite(f.end_seconds) || f.begin_seconds < 0 ||
            f.end_seconds <= f.begin_seconds || f.end_seconds > end)
            throw std::invalid_argument("invalid mission fault window");
    faults_ = std::move(faults);
}

SimulationFrame MissionSimulator::sample(MonoTime now, std::chrono::milliseconds elapsed) {
    const double t =
        static_cast<double>(std::clamp(elapsed, std::chrono::milliseconds{0}, duration_).count()) /
        1000;
    const auto active = [&](MissionFault type, double at) {
        return std::any_of(faults_.begin(), faults_.end(), [&](const auto& f) {
            return f.fault == type && at >= f.begin_seconds && at < f.end_seconds;
        });
    };
    const auto& p = profile_;
    State s;
    s.oil = p.ambient_c;
    s.cht = p.ambient_c;
    s.egt = p.ambient_c;
    const double initial_fuel = p.fuel_left_gal + p.fuel_right_gal;
    const Leg* current = &legs_.back();
    double leg_start = 0, local = legs_.back().seconds, distance = 0;
    double altitude = p.destination_ft, gs = 0, vs = 0;
    for (const auto& leg : legs_) {
        const double dt = std::clamp(t - leg_start, 0.0, leg.seconds);
        std::vector<double> cuts{0, dt};
        for (const auto& f : faults_)
            for (const double boundary : {f.begin_seconds, f.end_seconds})
                if (boundary > leg_start && boundary < leg_start + dt)
                    cuts.push_back(boundary - leg_start);
        std::sort(cuts.begin(), cuts.end());
        for (std::size_t i = 1; i < cuts.size(); ++i) {
            const double interval = cuts[i] - cuts[i - 1],
                         at = leg_start + (cuts[i] + cuts[i - 1]) / 2;
            const bool powered = leg.powered && !active(MissionFault::engine_stop, at);
            const double available_before = initial_fuel - s.used, rpm_start = s.rpm;
            const double target = powered ? leg.rpm : 0,
                         coefficient =
                             powered ? p.maximum_flow_gph / 5800.0 * (.25 + .75 * leg.throttle) : 0;
            const auto rpm_integral = [&](double seconds) {
                return std::max(0.0, target * seconds +
                                         (s.rpm - target) * 2.0 * (-std::expm1(-seconds / 2.0)));
            };
            double burning = interval;
            if (coefficient > 0 &&
                coefficient * rpm_integral(interval) / 3600.0 > initial_fuel - s.used) {
                double lo = 0, hi = interval;
                for (int iteration = 0; iteration < 50; ++iteration) {
                    const double mid = (lo + hi) / 2;
                    if (coefficient * rpm_integral(mid) / 3600.0 > initial_fuel - s.used)
                        hi = mid;
                    else
                        lo = mid;
                }
                burning = (lo + hi) / 2;
            }
            s.used = std::min(initial_fuel, s.used + coefficient * rpm_integral(burning) / 3600.0);
            s.rpm = approach(s.rpm, target, burning, 2.0);
            if (burning < interval)
                s.rpm = approach(s.rpm, 0, interval - burning, 2.0);
            const double heat = powered && available_before > 0 ? leg.throttle : 0;
            s.oil = approach(approach(s.oil, p.ambient_c + 90 * heat, burning, 150), p.ambient_c,
                             interval - burning, 150);
            s.cht = approach(approach(s.cht, p.ambient_c + 110 * heat, burning, 60), p.ambient_c,
                             interval - burning, 60);
            s.egt = approach(approach(s.egt, p.ambient_c + 800 * heat, burning, 8), p.ambient_c,
                             interval - burning, 8);
            double generation_seconds = 0;
            if (powered && available_before > 0 && target > 800 &&
                !active(MissionFault::alternator_failure, at)) {
                const double crossing =
                    rpm_start >= 800 ? 0 : -2 * std::log((target - 800) / (target - rpm_start));
                generation_seconds = std::max(0.0, burning - crossing);
            }
            if (leg.master)
                s.battery_used =
                    std::min(p.battery_ah,
                             s.battery_used + p.load_a * (interval - generation_seconds) / 3600);
            if (!active(MissionFault::flaps_jammed, at))
                s.flap = slew(s.flap, leg.flap, 4 * interval);
            s.gear = slew(s.gear, leg.gear_down ? 1.0 : 0.0, interval / 3.0);
        }
        distance +=
            (leg.start_kt * dt + (leg.end_kt - leg.start_kt) * dt * dt / (2 * leg.seconds)) *
            knot_mps;
        if (t < leg_start + leg.seconds) {
            current = &leg;
            local = dt;
            break;
        }
        leg_start += leg.seconds;
    }
    const double total_seconds = static_cast<double>(duration_.count()) / 1000;
    const bool complete = t >= total_seconds;
    if (!complete) {
        altitude =
            current->start_ft + (current->end_ft - current->start_ft) * local / current->seconds;
        gs = current->start_kt + (current->end_kt - current->start_kt) * local / current->seconds;
        vs = (current->end_ft - current->start_ft) / current->seconds * 60;
    }
    const double fraction =
        complete ? 1.0 : std::clamp(distance / (route_angle_ * earth_m), 0.0, 1.0);
    const double a = radians(p.departure_lat), b = radians(p.destination_lat),
                 al = radians(p.departure_lon), bl = radians(p.destination_lon);
    const double w0 = std::sin((1 - fraction) * route_angle_) / std::sin(route_angle_),
                 w1 = std::sin(fraction * route_angle_) / std::sin(route_angle_);
    const double x = w0 * std::cos(a) * std::cos(al) + w1 * std::cos(b) * std::cos(bl);
    const double y = w0 * std::cos(a) * std::sin(al) + w1 * std::cos(b) * std::sin(bl);
    const double z = w0 * std::sin(a) + w1 * std::sin(b);
    const double lat = std::atan2(z, std::hypot(x, y)), lon = std::atan2(y, x);
    const double initial_course =
        std::atan2(std::sin(bl - al) * std::cos(b),
                   std::cos(a) * std::sin(b) - std::sin(a) * std::cos(b) * std::cos(bl - al));
    const double course = fraction >= 1
                              ? initial_course
                              : std::atan2(std::sin(bl - lon) * std::cos(b),
                                           std::cos(lat) * std::sin(b) -
                                               std::sin(lat) * std::cos(b) * std::cos(bl - lon));
    const double north = gs * std::cos(course) - p.wind_north_kt,
                 east = gs * std::sin(course) - p.wind_east_kt;
    const double horizontal = std::hypot(north, east), vertical = vs * foot_m / 60 / knot_mps;
    const double tas = std::hypot(horizontal, vertical);
    // EAS proxy using an explicitly simplified isothermal density model, not certified IAS/CAS.
    const bool on_ground =
        complete || current->phase == FlightPhase::cold_and_dark ||
        current->phase == FlightPhase::engine_start || current->phase == FlightPhase::taxi ||
        current->phase == FlightPhase::shutdown ||
        ((current->phase == FlightPhase::takeoff || current->phase == FlightPhase::landing) &&
         current->start_ft == current->end_ft);
    const double ias = (on_ground ? std::max(0.0, gs - p.wind_north_kt * std::cos(course) -
                                                      p.wind_east_kt * std::sin(course))
                                  : tas) *
                       std::sqrt(std::exp(-altitude * foot_m / 8500.0));
    const double course_rate = gs * knot_mps * std::sin(course) * std::tan(lat) / earth_m;
    const double acceleration =
        complete ? 0 : (current->end_kt - current->start_kt) / current->seconds;
    const double dn = acceleration * std::cos(course) - gs * std::sin(course) * course_rate;
    const double de = acceleration * std::sin(course) + gs * std::cos(course) * course_rate;
    const double yaw_rate =
        on_ground || horizontal < .01 ? 0 : (north * de - east * dn) / (horizontal * horizontal);
    SimulationFrame frame;
    frame.scenario = scenario_;
    frame.phase = complete ? FlightPhase::complete : current->phase;
    frame.scenario_time = std::clamp(elapsed, std::chrono::milliseconds{0}, duration_);
    frame.progress = t / total_seconds;
    frame.complete = complete;
    const auto seq = ++sequence_;
    auto put = [&](Sample<double>& field, double value, SignalId id, Unit unit) {
        field = sample_of(value, id, unit, now, seq);
    };
    auto& f = frame.flight;
    f.sequence = seq;
    f.timestamp = now;
    put(f.attitude.roll_deg, degrees(std::atan(yaw_rate * horizontal * knot_mps / 9.80665)),
        SignalId::roll, Unit::degree);
    put(f.attitude.pitch_deg, on_ground ? 0 : degrees(std::atan2(vertical, horizontal)),
        SignalId::pitch, Unit::degree);
    put(f.attitude.heading_deg,
        !on_ground && horizontal > .01 ? heading(std::atan2(east, north)) : heading(course),
        SignalId::heading, Unit::degree);
    put(f.attitude.turn_rate_deg_s, degrees(yaw_rate), SignalId::turn_rate,
        Unit::degree_per_second);
    put(f.attitude.lateral_acceleration_g, 0, SignalId::lateral_acceleration, Unit::gravity);
    put(f.air_data.indicated_airspeed_kt, ias, SignalId::indicated_airspeed, Unit::knot);
    put(f.air_data.pressure_altitude_ft, altitude, SignalId::pressure_altitude, Unit::foot);
    put(f.air_data.indicated_altitude_ft, altitude, SignalId::indicated_altitude, Unit::foot);
    put(f.air_data.baro_setting_hpa, standard_pressure_hpa, SignalId::baro_setting,
        Unit::hectopascal);
    put(f.air_data.vertical_speed_fpm, vs, SignalId::vertical_speed, Unit::foot_per_minute);
    put(f.position.latitude_deg, degrees(lat), SignalId::latitude, Unit::degree);
    put(f.position.longitude_deg, degrees(lon), SignalId::longitude, Unit::degree);
    put(f.position.ground_speed_kt, gs, SignalId::ground_speed, Unit::knot);
    put(f.position.gnss_vertical_speed_fpm, vs, SignalId::gnss_vertical_speed,
        Unit::foot_per_minute);
    f.guidance.flight_director_active =
        sample_of(false, SignalId::flight_director_active, Unit::boolean, now, seq);
    put(f.guidance.commanded_roll_deg, 0, SignalId::commanded_roll, Unit::degree);
    put(f.guidance.commanded_pitch_deg, 0, SignalId::commanded_pitch, Unit::degree);
    put(f.guidance.selected_altitude_ft, p.cruise_ft, SignalId::selected_altitude, Unit::foot);
    put(f.guidance.selected_course_deg, heading(course), SignalId::selected_course, Unit::degree);
    put(f.guidance.selected_airspeed_kt, ias, SignalId::selected_airspeed, Unit::knot);
    put(f.navigation.cdi_deviation_dots, 0, SignalId::cdi_deviation, Unit::dot);
    put(f.navigation.glide_slope_dots, 0, SignalId::glide_slope_deviation, Unit::dot);
    f.navigation.to_flag = sample_of(true, SignalId::to_flag, Unit::boolean, now, seq);
    auto& e = frame.engine;
    e.sequence = seq;
    e.timestamp = now;
    const bool enabled = !complete && current->powered && !active(MissionFault::engine_stop, t) &&
                         s.used < initial_fuel;
    const bool alternator = enabled && s.rpm > 800 && !active(MissionFault::alternator_failure, t);
    const double fuel_ratio = initial_fuel > 0 ? (initial_fuel - s.used) / initial_fuel : 0;
    put(e.rpm, s.rpm, SignalId::engine_rpm, Unit::revolutions_per_minute);
    put(e.manifold_pressure_inhg, enabled ? 10 + 18 * current->throttle : 0,
        SignalId::manifold_pressure, Unit::inch_mercury);
    put(e.oil_pressure_psi, std::min(65.0, s.rpm * .013), SignalId::oil_pressure,
        Unit::pound_per_square_inch);
    put(e.oil_temperature_c, s.oil, SignalId::oil_temperature, Unit::celsius);
    put(e.fuel_pressure_psi, enabled ? 4.6 : 0, SignalId::fuel_pressure,
        Unit::pound_per_square_inch);
    put(e.fuel_flow_gph,
        enabled ? p.maximum_flow_gph / 5800 * (.25 + .75 * current->throttle) * s.rpm : 0,
        SignalId::fuel_flow, Unit::gallon_per_hour);
    put(e.fuel_left_gal, p.fuel_left_gal * fuel_ratio, SignalId::fuel_quantity_left, Unit::gallon);
    put(e.fuel_right_gal, p.fuel_right_gal * fuel_ratio, SignalId::fuel_quantity_right,
        Unit::gallon);
    const bool master = !complete && current->master;
    const bool battery_available = s.battery_used < p.battery_ah;
    put(e.bus_voltage_v,
        !master              ? 0
        : alternator         ? 14.2
        : !battery_available ? 0
                             : 12.6 - 2.6 * s.battery_used / p.battery_ah,
        SignalId::bus_voltage, Unit::volt);
    put(e.alternator_current_a,
        alternator                    ? p.load_a
        : master && battery_available ? -p.load_a
                                      : 0,
        SignalId::alternator_current, Unit::ampere);
    for (std::size_t i = 0; i < 4; ++i) {
        const double spread = static_cast<double>((seed_ + i) % 7) * .25;
        put(e.cht_c[i], s.cht + spread * (enabled ? 1 : 0),
            static_cast<SignalId>(static_cast<unsigned>(SignalId::cylinder_head_temperature_1) +
                                  static_cast<unsigned>(i)),
            Unit::celsius);
        put(e.egt_c[i], s.egt + spread * (enabled ? 2 : 0),
            static_cast<SignalId>(static_cast<unsigned>(SignalId::exhaust_gas_temperature_1) +
                                  static_cast<unsigned>(i)),
            Unit::celsius);
    }
    auto& air = frame.airframe;
    air.master_on = master;
    air.engine_running = enabled && s.rpm > 300;
    air.alternator_on = alternator;
    air.fuel_pump_on = enabled && (current->phase == FlightPhase::engine_start ||
                                   current->phase == FlightPhase::takeoff ||
                                   current->phase == FlightPhase::approach);
    air.pitot_heat_on = master && gs > 10;
    air.gear_nose_locked = air.gear_left_locked = air.gear_right_locked =
        s.gear >= .999 && !active(MissionFault::gear_not_locked, t);
    air.flap_angle_deg = s.flap;
    air.flap_command_deg = current->flap;
    if (active(MissionFault::low_oil_pressure, t)) {
        e.oil_pressure_psi.value = e.oil_pressure_psi.raw = 10;
        e.health = engine::EngineHealth::warning;
        e.injected_fault = engine::EngineFault::low_oil_pressure;
    }
    if (active(MissionFault::high_cht, t)) {
        e.cht_c[2].value = e.cht_c[2].raw = 245;
        e.health = engine::EngineHealth::warning;
        e.injected_fault = engine::EngineFault::high_cht;
    }
    if (active(MissionFault::alternator_failure, t)) {
        e.health = engine::EngineHealth::caution;
        e.injected_fault = engine::EngineFault::alternator_failure;
    }
    if (active(MissionFault::low_fuel_pressure, t)) {
        e.fuel_pressure_psi.value = e.fuel_pressure_psi.raw = 1;
        e.health = engine::EngineHealth::warning;
        e.injected_fault = engine::EngineFault::low_fuel_pressure;
    }
    if (active(MissionFault::ias_lost, t))
        f.air_data.indicated_airspeed_kt.validity = Validity::unavailable;
    if (active(MissionFault::baro_lost, t))
        f.air_data.pressure_altitude_ft.validity = f.air_data.indicated_altitude_ft.validity =
            f.air_data.vertical_speed_fpm.validity = Validity::unavailable;
    if (active(MissionFault::gnss_lost, t))
        f.position.latitude_deg.validity = f.position.longitude_deg.validity =
            f.position.ground_speed_kt.validity = f.position.gnss_vertical_speed_fpm.validity =
                f.navigation.cdi_deviation_dots.validity = Validity::unavailable;
    f.health.unavailable_fields = (active(MissionFault::ias_lost, t) ? 1U : 0U) +
                                  (active(MissionFault::baro_lost, t) ? 3U : 0U) +
                                  (active(MissionFault::gnss_lost, t) ? 5U : 0U);
    f.health.level = f.health.unavailable_fields > 0 ? HealthLevel::warning : HealthLevel::normal;
    frame.weather.category = "UNAVAILABLE";
    frame.weather.metar = "SIM METAR UNAVAILABLE";
    frame.weather.taf = "SIM TAF UNAVAILABLE";
    frame.events = {"MODEL:SYNTHETIC-KINEMATIC-V1",
                    std::string{"PHASE:"} + phase_name(frame.phase)};
    return frame;
}
} // namespace efis::simulation
