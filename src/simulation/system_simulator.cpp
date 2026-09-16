#include "efis/simulation/system_simulator.hpp"

#include <algorithm>
#include <stdexcept>

namespace efis::simulation {
namespace {

constexpr std::chrono::milliseconds full_duration{600000};

FlightPhase phase_at(double seconds) noexcept {
    if (seconds < 15.0) return FlightPhase::cold_and_dark;
    if (seconds < 30.0) return FlightPhase::engine_start;
    if (seconds < 75.0) return FlightPhase::taxi;
    if (seconds < 105.0) return FlightPhase::takeoff;
    if (seconds < 190.0) return FlightPhase::climb;
    if (seconds < 380.0) return FlightPhase::cruise;
    if (seconds < 475.0) return FlightPhase::descent;
    if (seconds < 545.0) return FlightPhase::approach;
    if (seconds < 580.0) return FlightPhase::landing;
    if (seconds < 600.0) return FlightPhase::shutdown;
    return FlightPhase::complete;
}

engine::EngineFault engine_fault_at(ScenarioId scenario, double seconds) noexcept {
    switch (scenario) {
        case ScenarioId::engine_emergency:
            if (seconds >= 260.0 && seconds < 330.0)
                return engine::EngineFault::low_oil_pressure;
            if (seconds >= 330.0 && seconds < 390.0)
                return engine::EngineFault::high_cht;
            break;
        case ScenarioId::electrical_failure:
            if (seconds >= 210.0 && seconds < 430.0)
                return engine::EngineFault::alternator_failure;
            break;
        default: break;
    }
    return engine::EngineFault::none;
}

InjectedFault flight_fault_at(ScenarioId scenario, double seconds) noexcept {
    if (scenario != ScenarioId::sensor_degradation) return InjectedFault::none;
    if (seconds >= 180.0 && seconds < 260.0) return InjectedFault::airspeed;
    if (seconds >= 300.0 && seconds < 380.0) return InjectedFault::altitude;
    if (seconds >= 420.0 && seconds < 500.0) return InjectedFault::navigation;
    return InjectedFault::none;
}

AirframeState airframe_at(FlightPhase phase, ScenarioId scenario,
                          double seconds) noexcept {
    AirframeState state;
    state.master_on = phase != FlightPhase::cold_and_dark && phase != FlightPhase::complete;
    state.engine_running = phase >= FlightPhase::engine_start && phase < FlightPhase::shutdown;
    state.alternator_on = state.engine_running;
    state.fuel_pump_on = phase == FlightPhase::engine_start || phase == FlightPhase::takeoff ||
                         phase == FlightPhase::approach || phase == FlightPhase::landing;
    state.pitot_heat_on = phase >= FlightPhase::taxi && phase <= FlightPhase::landing;
    const bool airborne = phase >= FlightPhase::climb && phase <= FlightPhase::descent;
    state.gear_nose_locked = !airborne;
    state.gear_left_locked = !airborne;
    state.gear_right_locked = !airborne;
    if (phase == FlightPhase::takeoff) state.flap_angle_deg = 10.0;
    if (phase == FlightPhase::approach) state.flap_angle_deg = 15.0;
    if (phase == FlightPhase::landing) state.flap_angle_deg = 30.0;
    state.flap_command_deg = state.flap_angle_deg;
    if (scenario == ScenarioId::electrical_failure && seconds >= 210.0 && seconds < 430.0)
        state.alternator_on = false;
    return state;
}

WeatherState weather_at(double seconds) {
    WeatherState weather;
    if (seconds >= 300.0 && seconds < 470.0) {
        weather.category = "MVFR";
        weather.metar = "SIM METAR LIDB 230900Z 08012KT 6000 SHRA BKN018 17/14 Q1015";
        weather.taf = "SIM TAF LIDB 230800Z 2309/2318 09012KT 6000 SHRA BKN018 TEMPO TSRA";
        weather.radar_precipitation = true;
        weather.radar_intensity = seconds < 390.0 ? 2 : 3;
    }
    return weather;
}

void set_value(Sample<double>& sample, double value) noexcept {
    sample.value = value;
    sample.raw = value;
}

double interpolate(double value, double from, double to,
                   double start, double end) noexcept {
    const double ratio = std::clamp((value - start) / (end - start), 0.0, 1.0);
    return from + (to - from) * ratio;
}

void apply_flight_phase(FlightSnapshot& flight, FlightPhase phase,
                        double seconds) noexcept {
    double ias = 0.0;
    double altitude = 59.0;
    double vertical_speed = 0.0;
    switch (phase) {
        case FlightPhase::cold_and_dark:
        case FlightPhase::engine_start: break;
        case FlightPhase::taxi: ias = 8.0; break;
        case FlightPhase::takeoff:
            ias = interpolate(seconds, 25.0, 75.0, 75.0, 105.0);
            altitude = interpolate(seconds, 59.0, 800.0, 75.0, 105.0);
            vertical_speed = 900.0;
            break;
        case FlightPhase::climb:
            ias = 90.0;
            altitude = interpolate(seconds, 800.0, 5500.0, 105.0, 190.0);
            vertical_speed = 900.0;
            break;
        case FlightPhase::cruise: ias = 118.0; altitude = 5500.0; break;
        case FlightPhase::descent:
            ias = 105.0;
            altitude = interpolate(seconds, 5500.0, 3000.0, 380.0, 475.0);
            vertical_speed = -750.0;
            break;
        case FlightPhase::approach:
            ias = interpolate(seconds, 85.0, 62.0, 475.0, 545.0);
            altitude = interpolate(seconds, 3000.0, 1400.0, 475.0, 545.0);
            vertical_speed = -500.0;
            break;
        case FlightPhase::landing:
            ias = interpolate(seconds, 62.0, 15.0, 545.0, 580.0);
            altitude = interpolate(seconds, 1400.0, 1240.0, 545.0, 580.0);
            vertical_speed = seconds < 558.0 ? -250.0 : 0.0;
            break;
        case FlightPhase::shutdown:
        case FlightPhase::complete: altitude = 1240.0; break;
    }
    set_value(flight.air_data.indicated_airspeed_kt, ias);
    set_value(flight.position.ground_speed_kt,
              phase == FlightPhase::taxi ? 8.0 : std::max(0.0, ias + (ias > 20.0 ? 4.0 : 0.0)));
    set_value(flight.air_data.pressure_altitude_ft, altitude);
    set_value(flight.air_data.vertical_speed_fpm, vertical_speed);
    set_value(flight.position.gnss_vertical_speed_fpm, vertical_speed);
    constexpr double departure_lat = 45.648399;
    constexpr double departure_lon = 12.194400;
    constexpr double destination_lat = 46.166549;
    constexpr double destination_lon = 12.250389;
    const double route_progress = std::clamp(seconds / 600.0, 0.0, 1.0);
    const double smooth_route = route_progress * route_progress *
                                (3.0 - 2.0 * route_progress);
    set_value(flight.position.latitude_deg,
              departure_lat + (destination_lat - departure_lat) * smooth_route);
    set_value(flight.position.longitude_deg,
              departure_lon + (destination_lon - departure_lon) * smooth_route);
    if (ias < 20.0) {
        set_value(flight.attitude.roll_deg, 0.0);
        set_value(flight.attitude.pitch_deg, 0.0);
        set_value(flight.attitude.turn_rate_deg_s, 0.0);
        set_value(flight.attitude.lateral_acceleration_g, 0.0);
    }
}

void apply_engine_phase(engine::EngineSnapshot& engine, FlightPhase phase,
                        double seconds) noexcept {
    double rpm = 0.0;
    double manifold = 0.0;
    double fuel_flow = 0.0;
    switch (phase) {
        case FlightPhase::cold_and_dark:
        case FlightPhase::complete: break;
        case FlightPhase::engine_start:
            rpm = interpolate(seconds, 0.0, 1800.0, 15.0, 30.0);
            manifold = 15.0; fuel_flow = 2.0; break;
        case FlightPhase::taxi: rpm = 2000.0; manifold = 16.0; fuel_flow = 2.2; break;
        case FlightPhase::takeoff: rpm = 5800.0; manifold = 27.5; fuel_flow = 6.8; break;
        case FlightPhase::climb: rpm = 5500.0; manifold = 26.0; fuel_flow = 6.0; break;
        case FlightPhase::cruise: rpm = 5000.0; manifold = 24.1; fuel_flow = 4.8; break;
        case FlightPhase::descent: rpm = 4200.0; manifold = 18.0; fuel_flow = 3.1; break;
        case FlightPhase::approach: rpm = 4500.0; manifold = 20.0; fuel_flow = 3.6; break;
        case FlightPhase::landing: rpm = 3500.0; manifold = 15.0; fuel_flow = 2.5; break;
        case FlightPhase::shutdown:
            rpm = interpolate(seconds, 1800.0, 0.0, 580.0, 600.0);
            manifold = 12.0; fuel_flow = rpm > 100.0 ? 1.8 : 0.0; break;
    }
    set_value(engine.rpm, rpm);
    set_value(engine.manifold_pressure_inhg, manifold);
    set_value(engine.fuel_flow_gph, fuel_flow);
}

}  // namespace

SystemSimulator::SystemSimulator(ScenarioId scenario, std::uint64_t seed) noexcept
    : scenario_(scenario), flight_source_(seed), engine_provider_(seed + 1U) {}

void SystemSimulator::reset(ScenarioId scenario) noexcept {
    scenario_ = scenario;
    flight_source_.set_fault(InjectedFault::none);
    engine_provider_.set_fault(engine::EngineFault::none);
    previous_phase_ = FlightPhase::complete;
    previous_engine_fault_ = engine::EngineFault::none;
    previous_flight_fault_ = InjectedFault::none;
}

std::chrono::milliseconds SystemSimulator::duration() const noexcept {
    return full_duration;
}

SimulationFrame SystemSimulator::sample(MonoTime now,
                                         std::chrono::milliseconds elapsed) {
    const auto bounded = std::clamp(elapsed, std::chrono::milliseconds{0}, duration());
    const double seconds = static_cast<double>(bounded.count()) / 1000.0;
    const auto phase = phase_at(seconds);
    const auto engine_fault = engine_fault_at(scenario_, seconds);
    const auto flight_fault = flight_fault_at(scenario_, seconds);
    engine_provider_.set_fault(engine_fault);
    flight_source_.set_fault(flight_fault);

    SimulationFrame frame;
    frame.scenario = scenario_;
    frame.phase = phase;
    frame.scenario_time = bounded;
    frame.progress = static_cast<double>(bounded.count()) /
                     static_cast<double>(duration().count());
    frame.complete = phase == FlightPhase::complete;
    frame.flight = flight_source_.poll(now, bounded);
    frame.engine = engine_provider_.sample(now, bounded);
    frame.airframe = airframe_at(phase, scenario_, seconds);
    frame.weather = weather_at(seconds);

    apply_flight_phase(frame.flight, phase, seconds);
    apply_engine_phase(frame.engine, phase, seconds);

    if (!frame.airframe.engine_running) {
        frame.engine.rpm.value = 0.0;
        frame.engine.rpm.raw = 0.0;
        frame.engine.alternator_current_a.value = 0.0;
        frame.engine.alternator_current_a.raw = 0.0;
        frame.engine.bus_voltage_v.value = frame.airframe.master_on ? 12.6 : 0.0;
        frame.engine.bus_voltage_v.raw = frame.engine.bus_voltage_v.value;
        set_value(frame.engine.oil_pressure_psi, 0.0);
        set_value(frame.engine.fuel_pressure_psi, 0.0);
    }
    if (phase != previous_phase_)
        frame.events.push_back(std::string{"PHASE:"} + phase_name(phase));
    if (engine_fault != previous_engine_fault_)
        frame.events.push_back(std::string{"ENGINE_FAULT:"} +
                               (engine_fault == engine::EngineFault::none ? "CLEARED" : "ACTIVE"));
    if (flight_fault != previous_flight_fault_)
        frame.events.push_back(std::string{"FLIGHT_DATA_FAULT:"} + injected_fault_name(flight_fault));
    previous_phase_ = phase;
    previous_engine_fault_ = engine_fault;
    previous_flight_fault_ = flight_fault;
    return frame;
}

const char* scenario_name(ScenarioId scenario) noexcept {
    switch (scenario) {
        case ScenarioId::full_flight: return "full-flight";
        case ScenarioId::engine_emergency: return "engine-emergency";
        case ScenarioId::electrical_failure: return "electrical-failure";
        case ScenarioId::sensor_degradation: return "sensor-degradation";
    }
    return "full-flight";
}

const char* phase_name(FlightPhase phase) noexcept {
    switch (phase) {
        case FlightPhase::cold_and_dark: return "COLD_AND_DARK";
        case FlightPhase::engine_start: return "ENGINE_START";
        case FlightPhase::taxi: return "TAXI";
        case FlightPhase::takeoff: return "TAKEOFF";
        case FlightPhase::climb: return "CLIMB";
        case FlightPhase::cruise: return "CRUISE";
        case FlightPhase::descent: return "DESCENT";
        case FlightPhase::approach: return "APPROACH";
        case FlightPhase::landing: return "LANDING";
        case FlightPhase::shutdown: return "SHUTDOWN";
        case FlightPhase::complete: return "COMPLETE";
    }
    return "COMPLETE";
}

ScenarioId parse_scenario(std::string_view name) {
    if (name == "full-flight") return ScenarioId::full_flight;
    if (name == "engine-emergency") return ScenarioId::engine_emergency;
    if (name == "electrical-failure") return ScenarioId::electrical_failure;
    if (name == "sensor-degradation") return ScenarioId::sensor_degradation;
    throw std::invalid_argument("unknown simulation scenario");
}

}  // namespace efis::simulation
