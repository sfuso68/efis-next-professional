#pragma once

#include "efis/core/flight_snapshot.hpp"
#include "efis/engine/engine_snapshot.hpp"
#include "efis/providers/engine_simulation_provider.hpp"
#include "efis/providers/simulation_source.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace efis::simulation {

enum class ScenarioId : std::uint8_t {
    full_flight,
    engine_emergency,
    electrical_failure,
    sensor_degradation
};

enum class FlightPhase : std::uint8_t {
    cold_and_dark,
    engine_start,
    taxi,
    takeoff,
    climb,
    cruise,
    descent,
    approach,
    landing,
    shutdown,
    complete
};

struct AirframeState final {
    bool master_on{};
    bool engine_running{};
    bool alternator_on{};
    bool fuel_pump_on{};
    bool pitot_heat_on{};
    bool gear_nose_locked{true};
    bool gear_left_locked{true};
    bool gear_right_locked{true};
    double flap_angle_deg{};
    double flap_command_deg{};
};

struct WeatherState final {
    std::string category{"VFR"};
    std::string metar{"SIM METAR LIPH 230900Z 04005KT CAVOK 21/13 Q1017"};
    std::string taf{"SIM TAF LIPH 230800Z 2309/2409 05006KT CAVOK"};
    bool radar_precipitation{};
    int radar_intensity{};
};

struct SimulationFrame final {
    FlightSnapshot flight;
    engine::EngineSnapshot engine;
    AirframeState airframe;
    WeatherState weather;
    ScenarioId scenario{ScenarioId::full_flight};
    FlightPhase phase{FlightPhase::cold_and_dark};
    std::chrono::milliseconds scenario_time{};
    double progress{};
    bool complete{};
    std::vector<std::string> events;
};

class SystemSimulator final {
public:
    explicit SystemSimulator(ScenarioId scenario = ScenarioId::full_flight,
                             std::uint64_t seed = 20260823) noexcept;

    void reset(ScenarioId scenario) noexcept;
    [[nodiscard]] SimulationFrame sample(MonoTime now,
                                         std::chrono::milliseconds elapsed);
    [[nodiscard]] ScenarioId scenario() const noexcept { return scenario_; }
    [[nodiscard]] std::chrono::milliseconds duration() const noexcept;

private:
    ScenarioId scenario_;
    SimulationSource flight_source_;
    EngineSimulationProvider engine_provider_;
    FlightPhase previous_phase_{FlightPhase::complete};
    engine::EngineFault previous_engine_fault_{engine::EngineFault::none};
    InjectedFault previous_flight_fault_{InjectedFault::none};
};

[[nodiscard]] const char* scenario_name(ScenarioId scenario) noexcept;
[[nodiscard]] const char* phase_name(FlightPhase phase) noexcept;
[[nodiscard]] ScenarioId parse_scenario(std::string_view name);

}  // namespace efis::simulation
