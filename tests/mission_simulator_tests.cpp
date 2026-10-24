#include "efis/simulation/mission_simulator.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void rejects(F fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "invalid model/control must be rejected");
}
double total(const efis::simulation::SimulationFrame& f) {
    return f.engine.fuel_left_gal.value + f.engine.fuel_right_gal.value;
}
double distance(const efis::simulation::SimulationFrame& a,
                const efis::simulation::SimulationFrame& b) {
    constexpr double rad = 3.14159265358979323846 / 180;
    const double la = a.flight.position.latitude_deg.value * rad,
                 lb = b.flight.position.latitude_deg.value * rad;
    const double dl =
        (b.flight.position.longitude_deg.value - a.flight.position.longitude_deg.value) * rad;
    const double h = std::pow(std::sin((lb - la) / 2), 2) +
                     std::cos(la) * std::cos(lb) * std::pow(std::sin(dl / 2), 2);
    return 6371000 * 2 * std::asin(std::sqrt(h));
}
} // namespace
int main() {
    using namespace efis::simulation;
    using namespace std::chrono_literals;
    const efis::MonoTime epoch{};
    MissionSimulator sim;
    require(sim.duration() > 600s && sim.duration() < 2h,
            "mission duration must derive from route distance, not a compressed ten-minute movie");
    const auto first = sim.sample(epoch, 0ms), parked = sim.sample(epoch, 10s);
    require(first.weather.category == "UNAVAILABLE",
            "unimplemented operational weather must not be presented as VFR/clear");
    require(first.flight.health.level == efis::HealthLevel::normal &&
                first.flight.health.unavailable_fields == 0,
            "nominal frame health must agree with its usable samples");
    require(sim.sample(epoch, 123457ms).scenario_time == 123457ms,
            "scenario time must preserve exact integer milliseconds, not a floating round-trip");
    require(distance(first, parked) < 1e-6 && parked.flight.position.ground_speed_kt.value == 0,
            "cold aircraft must not move along route");
    require(total(first) == total(parked) && parked.engine.fuel_flow_gph.value == 0,
            "cold aircraft must not consume fuel");
    require(first.engine.oil_temperature_c.value == 15 && first.engine.cht_c[0].value == 15,
            "cold engine temperatures must start at ambient");
    for (int second = 1; std::chrono::seconds{second} < sim.duration(); ++second) {
        const auto at = std::chrono::seconds{second};
        const auto a = sim.sample(epoch, at), b = sim.sample(epoch, at + 10ms);
        const double observed_gs = distance(a, b) / .01 / (1852.0 / 3600);
        require(std::abs(observed_gs - a.flight.position.ground_speed_kt.value) < .05,
                "position derivative must agree with indicated GS");
        const double observed_vs = (b.flight.air_data.indicated_altitude_ft.value -
                                    a.flight.air_data.indicated_altitude_ft.value) /
                                   .01 * 60;
        require(std::abs(observed_vs - a.flight.air_data.vertical_speed_fpm.value) < .01,
                "altitude derivative must agree with VS");
        const double flow = (total(a) - total(b)) / .01 * 3600;
        require(std::abs(flow - a.engine.fuel_flow_gph.value) < .02,
                "tank depletion derivative must agree with actual simulated flow");
        require(a.engine.rpm.is_simulated() && a.flight.position.latitude_deg.is_simulated(),
                "all mission domains must preserve SIM provenance");
        require(a.engine.rpm.sequence == a.flight.sequence &&
                    a.engine.sequence == a.flight.sequence,
                "one sampled frame must share a sequence");
        require(std::isfinite(total(a)) && total(a) >= 0, "finite fuel invariant");
    }
    const auto finish = sim.sample(epoch, sim.duration());
    require(finish.complete && finish.phase == FlightPhase::complete, "complete state missing");
    require(std::abs(finish.flight.position.latitude_deg.value - sim.profile().destination_lat) <
                    1e-8 &&
                std::abs(finish.flight.position.longitude_deg.value -
                         sim.profile().destination_lon) < 1e-8,
            "destination must be reached exactly");
    require(finish.flight.air_data.indicated_altitude_ft.value == sim.profile().destination_ft &&
                finish.flight.air_data.vertical_speed_fpm.value == 0,
            "arrival altitude and VS must agree");
    require(finish.engine.fuel_flow_gph.value == 0 && !finish.airframe.engine_running &&
                !finish.airframe.master_on,
            "complete engine and master state must be off");
    const auto future = sim.sample(epoch, sim.duration() + 1h);
    require(total(future) == total(finish), "completion must clamp state");
    const auto back = sim.sample(epoch + 10h, 200s);
    MissionSimulator fresh;
    const auto reference = fresh.sample(epoch, 200s);
    require(total(back) == total(reference) &&
                back.engine.oil_temperature_c.value == reference.engine.oil_temperature_c.value &&
                back.airframe.flap_angle_deg == reference.airframe.flap_angle_deg,
            "seek must reproduce state independently of host time and sampling history");
    sim.reset(ScenarioId::full_flight);
    require(total(sim.sample(epoch, 0ms)) == total(first), "reset fuel mismatch");
    SimulationClock clock{sim.duration()};
    clock.advance(1s);
    require(clock.time() == 0ms, "clock starts paused");
    clock.set_paused(false);
    clock.advance(100ms);
    clock.set_speed(5);
    clock.advance(100ms);
    require(clock.time() == 600ms, "speed control mismatch");
    clock.set_paused(true);
    clock.advance(1h);
    require(clock.time() == 600ms, "pause must freeze scenario clock");
    clock.seek(10s);
    require(clock.time() == 10s, "seek mismatch");
    clock.set_paused(false);
    clock.advance(24h);
    require(clock.time() == sim.duration(), "clock completion clamp missing");
    rejects([&] { clock.advance(-1ms); });
    rejects([&] { clock.seek(-1ms); });
    rejects([&] { clock.seek(sim.duration() + 1ms); });
    rejects([&] { clock.set_speed(std::numeric_limits<double>::quiet_NaN()); });
    rejects([&] { clock.set_speed(0); });
    rejects([] { SimulationClock bad{0ms}; });
    SimulationClock fractional{1s};
    fractional.set_paused(false);
    fractional.set_speed(.25);
    for (int i = 0; i < 2000; ++i)
        fractional.advance(500us);
    require(fractional.time() == 250ms,
            "sub-millisecond host deltas must not be truncated at every tick");
    MissionProfile invalid;
    invalid.climb_fpm = std::numeric_limits<double>::infinity();
    rejects([&] { MissionSimulator bad{invalid}; });
    invalid = {};
    invalid.destination_lat = invalid.departure_lat;
    invalid.destination_lon = invalid.departure_lon;
    rejects([&] { MissionSimulator bad{invalid}; });
    invalid = {};
    invalid.id = "not/a/path";
    rejects([&] { MissionSimulator bad{invalid}; });
    invalid = {};
    invalid.fuel_left_gal = -1;
    rejects([&] { MissionSimulator bad{invalid}; });
    rejects([&] { sim.set_faults({{MissionFault::ias_lost, 10, 5}}); });
    rejects([&] { sim.set_faults({{static_cast<MissionFault>(255), 1, 2}}); });
    rejects([&] { sim.set_faults(std::vector<FaultWindow>(17, {MissionFault::ias_lost, 1, 2})); });
    for (const auto fault : {MissionFault::low_oil_pressure, MissionFault::high_cht,
                             MissionFault::low_fuel_pressure}) {
        sim.set_faults({{fault, 100, 120}});
        const auto injected = sim.sample(epoch, 110s);
        require(injected.engine.health == efis::engine::EngineHealth::warning &&
                    injected.engine.injected_fault != efis::engine::EngineFault::none,
                "engine fault must expose non-normal health and identity");
        require(sim.sample(epoch, 120s).engine.injected_fault == efis::engine::EngineFault::none,
                "engine fault window must recover at its exclusive endpoint");
    }
    sim.set_faults({{MissionFault::ias_lost, 100, 120},
                    {MissionFault::baro_lost, 100, 120},
                    {MissionFault::gnss_lost, 100, 120}});
    const auto failed = sim.sample(epoch, 110s), recovered = sim.sample(epoch, 120s);
    require(failed.flight.health.unavailable_fields == 9 &&
                failed.flight.health.level == efis::HealthLevel::warning &&
                recovered.flight.health.unavailable_fields == 0,
            "aggregate health must agree with injected dependent channel validity");
    require(
        failed.flight.air_data.indicated_airspeed_kt.validity == efis::Validity::unavailable &&
            failed.flight.air_data.pressure_altitude_ft.validity == efis::Validity::unavailable &&
            failed.flight.air_data.indicated_altitude_ft.validity == efis::Validity::unavailable &&
            failed.flight.position.latitude_deg.validity == efis::Validity::unavailable,
        "sensor fault masks must propagate to dependent channels");
    require(failed.flight.air_data.indicated_airspeed_kt.is_simulated() &&
                recovered.flight.air_data.indicated_airspeed_kt.usable(),
            "fault recovery must preserve SIM provenance");
    sim.set_faults(
        {{MissionFault::engine_stop, 100, 200}, {MissionFault::gear_not_locked, 100, 200}});
    const auto stopped = sim.sample(epoch, 150s);
    require(stopped.engine.rpm.value < 1 && stopped.engine.fuel_flow_gph.value == 0 &&
                !stopped.airframe.gear_nose_locked,
            "engine/gear faults ineffective");
    require(total(stopped) > total(reference), "stopped engine must preserve remaining fuel");
    sim.set_faults({{MissionFault::flaps_jammed, 70, 200}});
    require(sim.sample(epoch, 100s).airframe.flap_angle_deg == 0 &&
                sim.sample(epoch, 100s).airframe.flap_command_deg == 10,
            "jam must separate measured from commanded flap");
    MissionProfile dry;
    dry.fuel_left_gal = dry.fuel_right_gal = .001;
    MissionSimulator starved{dry};
    const auto empty = starved.sample(epoch, 200s);
    require(total(empty) < 1e-10 && empty.engine.rpm.value < 1 &&
                empty.engine.fuel_flow_gph.value == 0,
            "fuel exhaustion must stop the synthetic engine without negative quantities");
    MissionSimulator electric{{}, ScenarioId::electrical_failure};
    const auto electrical = electric.sample(epoch, electric.duration() / 2);
    require(!electrical.airframe.alternator_on && electrical.engine.alternator_current_a.value < 0,
            "electrical failure must expose load discharge");
    MissionProfile wind;
    wind.wind_east_kt = 20;
    MissionSimulator windy{wind};
    const auto cross = windy.sample(epoch, 300s);
    require(std::abs(cross.flight.attitude.heading_deg.value -
                     reference.flight.attitude.heading_deg.value) > 1,
            "wind must affect air-relative heading");
    std::cout << "mission simulator: kinematics, coupled fuel/thermal, faults, clock, seek and "
                 "validation PASS\n";
}
