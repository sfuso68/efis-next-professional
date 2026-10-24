#pragma once

#include "efis/simulation/system_simulator.hpp"

namespace efis::simulation {

// Synthetic engineering fixture, never an approved aircraft performance model.
struct MissionProfile final {
    std::string id{"synthetic-lsa-kinematic-v1"};
    double departure_lat{45.648399}, departure_lon{12.194400}, departure_ft{59.0};
    double destination_lat{46.166549}, destination_lon{12.250389}, destination_ft{1240.0};
    double cruise_ft{5500.0}, climb_fpm{750.0}, descent_fpm{600.0};
    double cruise_ground_kt{118.0};
    double wind_north_kt{}, wind_east_kt{}, ambient_c{15.0};
    double fuel_left_gal{10.57}, fuel_right_gal{10.57}, maximum_flow_gph{6.8};
    double battery_ah{18.0}, load_a{12.0};
};

enum class MissionFault : std::uint8_t {
    low_oil_pressure,
    high_cht,
    alternator_failure,
    ias_lost,
    baro_lost,
    gnss_lost,
    flaps_jammed,
    gear_not_locked,
    low_fuel_pressure,
    engine_stop
};
struct FaultWindow final {
    MissionFault fault{};
    double begin_seconds{}, end_seconds{};
};

// Caller supplies monotonic deltas; no OS clock, sockets or actuator access.
class SimulationClock final {
  public:
    explicit SimulationClock(std::chrono::milliseconds duration);
    void advance(std::chrono::nanoseconds delta);
    void seek(std::chrono::milliseconds time);
    void set_speed(double speed);
    void set_paused(bool paused) noexcept { paused_ = paused; }
    [[nodiscard]] bool paused() const noexcept { return paused_; }
    [[nodiscard]] std::chrono::milliseconds time() const noexcept;

  private:
    long double elapsed_ms_{};
    std::chrono::milliseconds duration_;
    double speed_{1.0};
    bool paused_{true};
};

class MissionSimulator final {
  public:
    explicit MissionSimulator(MissionProfile profile = {},
                              ScenarioId scenario = ScenarioId::full_flight,
                              std::uint64_t seed = 20261024);
    void reset(ScenarioId scenario);
    void set_faults(std::vector<FaultWindow> faults);
    [[nodiscard]] SimulationFrame sample(MonoTime received_at, std::chrono::milliseconds elapsed);
    [[nodiscard]] std::chrono::milliseconds duration() const noexcept { return duration_; }
    [[nodiscard]] const MissionProfile& profile() const noexcept { return profile_; }
    [[nodiscard]] ScenarioId scenario() const noexcept { return scenario_; }

  private:
    struct Leg {
        FlightPhase phase;
        double seconds, start_ft, end_ft, start_kt, end_kt, rpm, throttle, flap;
        bool gear_down, master, powered;
    };
    MissionProfile profile_;
    ScenarioId scenario_;
    std::uint64_t seed_, sequence_{};
    std::vector<Leg> legs_;
    std::vector<FaultWindow> faults_;
    std::chrono::milliseconds duration_{};
    double route_angle_{};
};

} // namespace efis::simulation
