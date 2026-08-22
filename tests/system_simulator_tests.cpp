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
