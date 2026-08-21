#include "efis/core/flight_model.hpp"
#include "efis/providers/simulation_provider.hpp"

#include <chrono>
#include <iostream>

int main() {
    using namespace std::chrono_literals;
    efis::SimulationProvider provider{1};
    efis::FlightModel model{500ms};
    const auto start = efis::MonoClock::now();

    for (int frame = 0; frame < 5; ++frame) {
        const auto elapsed = frame * 100ms;
        const auto now = start + elapsed;
        const auto snapshot = model.normalize(provider.sample(now, elapsed), now);
        std::cout << "seq=" << snapshot.sequence
                  << " ias=" << snapshot.air_data.indicated_airspeed_kt.value
                  << " alt=" << snapshot.air_data.pressure_altitude_ft.value
                  << " hdg=" << snapshot.attitude.heading_deg.value << '\n';
    }
}
