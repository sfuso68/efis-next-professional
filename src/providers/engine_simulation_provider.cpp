#include "efis/providers/engine_simulation_provider.hpp"

#include <algorithm>
#include <cmath>

namespace efis {
namespace {
Sample<double> simulated(double value, SignalId signal, Unit unit, MonoTime now,
                         std::uint64_t sequence) {
    Sample<double> sample{value, SourceId::simulation, now, now, Quality::high,
                          Validity::simulated, sequence};
    sample.origin.signal = signal;
    sample.origin.unit = unit;
    sample.origin.simulated = true;
    sample.raw = value;
    return sample;
}

// I quattro cilindri condividono una famiglia contigua di identificativi, cosi'
// l'indice del ciclo corrisponde direttamente al segnale registrato.
SignalId cylinder_signal(SignalId first, std::size_t index) {
    return static_cast<SignalId>(
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(first) +
                                   static_cast<std::uint16_t>(index)));
}

// La simulazione di guasto altera la grandezza misurata, quindi deve alterare
// anche il valore grezzo: lasciare raw al valore nominale descriverebbe un
// sensore sano e una conversione sbagliata, che e' un guasto diverso.
void inject(Sample<double>& sample, double value) {
    sample.value = value;
    sample.raw = value;
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
    out.rpm = simulated(2380.0 + 35.0 * std::sin(phase * 0.22), SignalId::engine_rpm, Unit::revolutions_per_minute, now, sequence_);
    out.manifold_pressure_inhg = simulated(23.7 + 0.25 * std::sin(phase * 0.13), SignalId::manifold_pressure, Unit::inch_mercury, now, sequence_);
    out.oil_pressure_psi = simulated(72.0 + 1.5 * std::sin(phase * 0.31), SignalId::oil_pressure, Unit::pound_per_square_inch, now, sequence_);
    out.oil_temperature_c = simulated(94.0 + 2.0 * std::sin(phase * 0.07), SignalId::oil_temperature, Unit::celsius, now, sequence_);
    out.fuel_pressure_psi = simulated(5.2 + 0.08 * std::sin(phase * 0.41), SignalId::fuel_pressure, Unit::pound_per_square_inch, now, sequence_);
    out.fuel_flow_gph = simulated(9.6 + 0.15 * std::sin(phase * 0.17), SignalId::fuel_flow, Unit::gallon_per_hour, now, sequence_);
    out.fuel_left_gal = simulated(std::max(0.0, 18.0 - seconds / 7200.0), SignalId::fuel_quantity_left, Unit::gallon, now, sequence_);
    out.fuel_right_gal = simulated(std::max(0.0, 18.0 - seconds / 7200.0), SignalId::fuel_quantity_right, Unit::gallon, now, sequence_);
    out.bus_voltage_v = simulated(14.1 + 0.08 * std::sin(phase * 0.19), SignalId::bus_voltage, Unit::volt, now, sequence_);
    out.alternator_current_a = simulated(18.0 + 1.2 * std::sin(phase * 0.27), SignalId::alternator_current, Unit::ampere, now, sequence_);
    for (std::size_t index = 0; index < out.cht_c.size(); ++index) {
        out.cht_c[index] = simulated(178.0 + static_cast<double>(index) * 3.0 +
            2.0 * std::sin(phase * 0.09 + static_cast<double>(index)),
            cylinder_signal(SignalId::cylinder_head_temperature_1, index),
            Unit::celsius, now, sequence_);
        out.egt_c[index] = simulated(730.0 + static_cast<double>(index) * 6.0 +
            5.0 * std::sin(phase * 0.11 + static_cast<double>(index)),
            cylinder_signal(SignalId::exhaust_gas_temperature_1, index),
            Unit::celsius, now, sequence_);
    }
    switch (fault_) {
        case engine::EngineFault::none: break;
        case engine::EngineFault::low_oil_pressure:
            inject(out.oil_pressure_psi, 22.0); out.health = engine::EngineHealth::warning; break;
        case engine::EngineFault::high_cht:
            inject(out.cht_c[2], 245.0); out.health = engine::EngineHealth::caution; break;
        case engine::EngineFault::alternator_failure:
            inject(out.bus_voltage_v, 11.7); inject(out.alternator_current_a, 0.0);
            out.health = engine::EngineHealth::caution; break;
        case engine::EngineFault::low_fuel_pressure:
            inject(out.fuel_pressure_psi, 1.2); out.health = engine::EngineHealth::warning; break;
    }
    return out;
}

}  // namespace efis
