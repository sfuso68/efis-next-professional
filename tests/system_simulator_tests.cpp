#include "efis/simulation/system_simulator.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    using namespace std::chrono_literals;
    using namespace efis::simulation;
    const auto start = efis::MonoClock::now();

    SystemSimulator nominal{ScenarioId::full_flight};
    const auto dark = nominal.sample(start, 0ms);
    require(dark.phase == FlightPhase::cold_and_dark, "scenario must start cold and dark");
    require(dark.engine.rpm.value == 0.0, "engine must be stopped before start");
    require(dark.engine.oil_pressure_psi.value == 0.0,
            "stopped engine must not indicate oil pressure");
    require(dark.flight.air_data.indicated_airspeed_kt.value == 0.0,
            "cold aircraft must not indicate airspeed");
    require(dark.flight.air_data.indicated_airspeed_kt.is_simulated(), "flight data must be marked simulated");
    const auto fuel_total = [](const SimulationFrame& frame) {
        return frame.engine.fuel_left_gal.value + frame.engine.fuel_right_gal.value;
    };
    const auto near = [](double actual, double expected) {
        return std::abs(actual - expected) < 1e-9;
    };
    const auto parked = nominal.sample(start + 10s, 10s);
    require(near(fuel_total(dark), fuel_total(parked)) && parked.engine.fuel_flow_gph.value == 0.0,
            "stopped cold aircraft must not consume fuel");
    // Independent hand integral for the first three powered segments.
    const auto climb_start = nominal.sample(start + 105s, 105s);
    require(near(fuel_total(dark) - fuel_total(climb_start),
                 (15.0 * 2.0 + 45.0 * 2.2 + 30.0 * 6.8) / 3600.0),
            "tank depletion must equal integrated scenario fuel flow across phase boundaries");
    const auto cruise_a = nominal.sample(start + 200s, 200s);
    const auto cruise_b = nominal.sample(start + 210s, 210s);
    require(near(fuel_total(cruise_a) - fuel_total(cruise_b),
                 cruise_a.engine.fuel_flow_gph.value * 10.0 / 3600.0),
            "constant-flow depletion must match indicated total flow, not a fixed provider rate");
    const auto seek_back = nominal.sample(start + 900s, 200s);
    require(near(fuel_total(seek_back), fuel_total(cruise_a)),
            "backward seek and wall-clock changes must not accumulate extra fuel consumption");
    SystemSimulator sparse;
    require(near(fuel_total(sparse.sample(start, 210s)), fuel_total(cruise_b)),
            "tank quantity must not depend on sampling history or polling frequency");
    nominal.reset(ScenarioId::full_flight);
    require(near(fuel_total(nominal.sample(start, 0ms)), fuel_total(dark)),
            "scenario reset must restore the demonstration fuel quantity");
    const auto stopped = nominal.sample(start + 580s, 580s);
    const auto finished = nominal.sample(start + 600s, 600s);
    require(stopped.engine.rpm.value == 0.0 && stopped.engine.fuel_flow_gph.value == 0.0 &&
                near(fuel_total(stopped), fuel_total(finished)),
            "shutdown and completion must not retain fuel flow or deplete tanks");
    require(stopped.engine.fuel_left_gal.is_simulated() &&
                near(stopped.engine.fuel_left_gal.raw, stopped.engine.fuel_left_gal.value),
            "derived tank quantities must preserve SIM provenance and consistent raw values");
    for (const auto scenario : {ScenarioId::full_flight, ScenarioId::engine_emergency,
                               ScenarioId::electrical_failure, ScenarioId::sensor_degradation}) {
        SystemSimulator sampled{scenario};
        for (int second = 0; second <= 600; ++second) {
            const auto frame = sampled.sample(start + std::chrono::seconds{second},
                                              std::chrono::seconds{second});
            require(std::isfinite(fuel_total(frame)) && fuel_total(frame) >= 0.0,
                    "all scenarios must retain finite non-negative fuel quantities");
        }
        require(near(fuel_total(sampled.sample(start, 600s)), fuel_total(finished)),
                "dense sampling and non-fuel faults must preserve nominal integrated consumption");
        require(near(fuel_total(sampled.sample(start, -1s)), fuel_total(dark)) &&
                    near(fuel_total(sampled.sample(start, 900s)), fuel_total(finished)),
                "elapsed time outside scenario bounds must clamp fuel consistently");
    }
    const auto takeoff = nominal.sample(start + 90s, 90s);
    require(takeoff.phase == FlightPhase::takeoff, "takeoff phase missing");
    require(takeoff.airframe.flap_angle_deg == 10.0, "takeoff flap setting missing");
    require(takeoff.engine.rpm.value > 2700.0, "takeoff power missing");
    const auto approach = nominal.sample(start + 520s, 520s);
    require(approach.phase == FlightPhase::approach, "approach phase missing");
    require(approach.airframe.flap_angle_deg == 15.0, "approach flap setting missing");
    require(approach.weather.radar_precipitation == false, "weather cell must have cleared on approach");
    const auto complete = nominal.sample(start + 600s, 600s);
    require(std::abs(complete.flight.position.latitude_deg.value - 46.166549) < 0.000001,
            "route must finish at Belluno");

    SystemSimulator engine{ScenarioId::engine_emergency};
    const auto oil = engine.sample(start + 280s, 280s);
    require(oil.engine.injected_fault == efis::engine::EngineFault::low_oil_pressure,
            "oil pressure emergency missing");
    require(oil.engine.oil_pressure_psi.value < 30.0, "oil pressure fault ineffective");
    const auto hot = engine.sample(start + 350s, 350s);
    require(hot.engine.injected_fault == efis::engine::EngineFault::high_cht,
            "high CHT emergency missing");

    SystemSimulator electrical{ScenarioId::electrical_failure};
    const auto alternator = electrical.sample(start + 240s, 240s);
    require(!alternator.airframe.alternator_on, "alternator must be offline");
    require(alternator.engine.bus_voltage_v.value < 12.0, "failed bus voltage not simulated");

    SystemSimulator sensors{ScenarioId::sensor_degradation};
    const auto airspeed = sensors.sample(start + 200s, 200s);
    require(airspeed.flight.air_data.indicated_airspeed_kt.validity == efis::Validity::unavailable,
            "airspeed failure missing");
    const auto recovered = sensors.sample(start + 270s, 270s);
    require(recovered.flight.air_data.indicated_airspeed_kt.validity == efis::Validity::simulated,
            "airspeed recovery missing");

    std::cout << "system simulator tests passed\n";
}
