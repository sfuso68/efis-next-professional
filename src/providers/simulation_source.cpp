#include "efis/providers/simulation_source.hpp"

namespace efis {

FlightSnapshot SimulationSource::poll(MonoTime now, std::chrono::milliseconds elapsed) {
    auto snapshot = provider_.sample(now, elapsed);

    // Il guasto rende la grandezza indisponibile, non la porta a zero. Uno zero
    // e' un valore, e un valore si mostra: sarebbe il dato plausibile ma falso
    // che il paragrafo 22 dell'handoff vieta. Indisponibile invece si vede.
    switch (fault_) {
        case InjectedFault::none:
            break;
        case InjectedFault::airspeed:
            snapshot.air_data.indicated_airspeed_kt.validity = Validity::unavailable;
            break;
        case InjectedFault::altitude:
            snapshot.air_data.pressure_altitude_ft.validity = Validity::unavailable;
            break;
        case InjectedFault::navigation:
            snapshot.navigation.cdi_deviation_dots.validity = Validity::unavailable;
            break;
    }
    return snapshot;
}

void SimulationSource::cycle_fault() noexcept {
    switch (fault_) {
        case InjectedFault::none: fault_ = InjectedFault::airspeed; break;
        case InjectedFault::airspeed: fault_ = InjectedFault::altitude; break;
        case InjectedFault::altitude: fault_ = InjectedFault::navigation; break;
        case InjectedFault::navigation: fault_ = InjectedFault::none; break;
    }
}

}  // namespace efis
