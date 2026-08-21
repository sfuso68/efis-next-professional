#pragma once

#include "efis/core/flight_snapshot.hpp"

namespace efis {

class AlertEngine final {
public:
    [[nodiscard]] AlertState evaluate(const FlightSnapshot& snapshot) const;
};

}  // namespace efis
