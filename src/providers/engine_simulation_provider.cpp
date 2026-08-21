#include "efis/providers/engine_simulation_provider.hpp"

#include <algorithm>
#include <cmath>

namespace efis {
namespace {
Sample<double> simulated(double value, MonoTime now, std::uint64_t sequence) {
    return {value, SourceId::simulation, now, now, Quality::high,
            Validity::simulated, sequence};
}
}

EngineSimulationProvider::EngineSimulationProvider(std::uint64_t seed) noexcept
    : seed_(seed) {}

engine::EngineSnapshot EngineSimulationProvider::sample(
    MonoTime now, std::chrono::milliseconds elapsed) {
    ++sequence_;
    const double seconds = static_cast<double>(elapsed.count()) / 1000.0;
    const double phase = seconds + static_cast<double>(seed_ % 90U);
    engine::EngineSnapshot out;
    out.sequence = sequence_; out.timestamp = now; out.injected_fault = fault_;
    out.rpm = simulated(2380.0 + 35.0 * std::sin(phase * 0.22), now, sequence_);
    out.manifold_pressure_inhg = simulated(23.7 + 0.25 * std::sin(phase * 0.13), now, sequence_);
    out.oil_pressure_psi = simulated(72.0 + 1.5 * std::sin(phase * 0.31), now, sequence_);
    out.oil_temperature_c = simulated(94.0 + 2.0 * std::sin(phase * 0.07), now, sequence_);
    out.fuel_pressure_psi = simulated(5.2 + 0.08 * std::sin(phase * 0.41), now, sequence_);
    out.fuel_flow_gph = simulated(9.6 + 0.15 * std::sin(phase * 0.17), now, sequence_);
    out.fuel_left_gal = simulated(std::max(0.0, 18.0 - seconds / 7200.0), now, sequence_);
    out.fuel_right_gal = simulated(std::max(0.0, 18.0 - seconds / 7200.0), now, sequence_);
    out.bus_voltage_v = simulated(14.1 + 0.08 * std::sin(phase * 0.19), now, sequence_);
    out.alternator_current_a = simulated(18.0 + 1.2 * std::sin(phase * 0.27), now, sequence_);
    for (std::size_t index = 0; index < out.cht_c.size(); ++index) {
        out.cht_c[index] = simulated(178.0 + static_cast<double>(index) * 3.0 +
            2.0 * std::sin(phase * 0.09 + static_cast<double>(index)), now, sequence_);
        out.egt_c[index] = simulated(730.0 + static_cast<double>(index) * 6.0 +
            5.0 * std::sin(phase * 0.11 + static_cast<double>(index)), now, sequence_);
    }
    switch (fault_) {
        case engine::EngineFault::none: break;
        case engine::EngineFault::low_oil_pressure:
            out.oil_pressure_psi.value = 22.0; out.health = engine::EngineHealth::warning; break;
        case engine::EngineFault::high_cht:
            out.cht_c[2].value = 245.0; out.health = engine::EngineHealth::caution; break;
        case engine::EngineFault::alternator_failure:
            out.bus_voltage_v.value = 11.7; out.alternator_current_a.value = 0.0;
            out.health = engine::EngineHealth::caution; break;
        case engine::EngineFault::low_fuel_pressure:
            out.fuel_pressure_psi.value = 1.2; out.health = engine::EngineHealth::warning; break;
    }
    return out;
}

}  // namespace efis
