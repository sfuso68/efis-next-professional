#include "efis/core/engine_runtime.hpp"

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    efis::engine::RuntimeTracker tracker{{500.0, 5000.0}, {10.0, 8.0, 7.0, 6.0}};
    tracker.update(0.0, 3600.0, false);
    require(!tracker.engine_running(), "master power alone must not count as engine time");
    require(tracker.totals().master_hours == 11.0 && tracker.totals().engine_hours == 8.0,
            "master and engine hours must remain independent");
    tracker.update(2500.0, 3600.0, true);
    require(tracker.engine_running(), "rpm above threshold must count as running");
    require(tracker.totals().engine_hours == 9.0, "engine time must advance at wall-clock rate");
    require(tracker.totals().tach_hours == 7.5, "tach time must be rpm weighted");
    require(tracker.totals().flight_hours == 7.0, "flight time must remain independent");
    tracker.update(6000.0, -1.0, true);
    require(tracker.totals().master_hours == 12.0, "invalid elapsed time must be ignored");
    std::cout << "engine runtime tests passed\n";
}
