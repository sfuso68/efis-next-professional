#pragma once

#include "efis/core/flight_snapshot.hpp"

#include <chrono>
#include <cstdint>

namespace efis {

class SimulationProvider final {
public:
    explicit SimulationProvider(std::uint64_t seed = 1) noexcept;

    [[nodiscard]] FlightSnapshot sample(MonoTime now,
                                        std::chrono::milliseconds elapsed);

private:
    std::uint64_t seed_;
    std::uint64_t sequence_{};
};

}  // namespace efis
