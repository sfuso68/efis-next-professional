#pragma once

#include "efis/core/sample.hpp"

#include <array>
#include <cstdint>

namespace efis::engine {

enum class EngineHealth : std::uint8_t { normal, caution, warning };
enum class EngineFault : std::uint8_t {
    none, low_oil_pressure, high_cht, alternator_failure, low_fuel_pressure
};

struct EngineSnapshot final {
    std::uint64_t sequence{};
    MonoTime timestamp{};
    Sample<double> rpm;
    Sample<double> manifold_pressure_inhg;
    Sample<double> oil_pressure_psi;
    Sample<double> oil_temperature_c;
    Sample<double> fuel_pressure_psi;
    Sample<double> fuel_flow_gph;
    Sample<double> fuel_left_gal;
    Sample<double> fuel_right_gal;
    Sample<double> bus_voltage_v;
    Sample<double> alternator_current_a;
    std::array<Sample<double>, 4> cht_c;
    std::array<Sample<double>, 4> egt_c;
    EngineHealth health{EngineHealth::normal};
    EngineFault injected_fault{EngineFault::none};
};

}  // namespace efis::engine
