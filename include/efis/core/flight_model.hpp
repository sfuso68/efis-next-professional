#pragma once

#include "efis/core/flight_snapshot.hpp"

#include <chrono>

namespace efis {

class FlightModel final {
public:
    explicit FlightModel(std::chrono::milliseconds stale_after =
                             std::chrono::milliseconds{500});

    [[nodiscard]] FlightSnapshot normalize(FlightSnapshot input,
                                           MonoTime now) const;

private:
    std::chrono::milliseconds stale_after_;
};

}  // namespace efis
