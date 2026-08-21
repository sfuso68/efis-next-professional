#pragma once

#include "efis/core/flight_source.hpp"
#include "efis/providers/simulation_provider.hpp"

#include <cstdint>

namespace efis {

// Guasto iniettato sul dato simulato.
//
// Serve a esercitare la degradazione senza scollegare nulla: e' l'unico modo di
// provare a terra cosa fa il display quando una grandezza sparisce. Vive qui e
// non nel controller perche' e' un comportamento della sorgente simulata, e
// perche' cosi' si puo' verificare senza avviare Qt.
enum class InjectedFault : std::uint8_t { none, airspeed, altitude, navigation };

[[nodiscard]] constexpr const char* injected_fault_name(InjectedFault fault) noexcept {
    switch (fault) {
        case InjectedFault::none: return "NONE";
        case InjectedFault::airspeed: return "AIRSPEED";
        case InjectedFault::altitude: return "ALTITUDE";
        case InjectedFault::navigation: return "NAVIGATION";
    }
    return "NONE";
}

// La simulazione vista come una sorgente qualunque.
//
// Il display non la distingue dalle altre: la riconosce soltanto dal marcatore
// che i suoi campioni portano, che e' esattamente come deve essere.
class SimulationSource final : public FlightSource {
public:
    explicit SimulationSource(std::uint64_t seed = 42) noexcept : provider_(seed) {}

    [[nodiscard]] SourceId source() const noexcept override {
        return SourceId::simulation;
    }

    [[nodiscard]] FlightSnapshot poll(MonoTime now,
                                      std::chrono::milliseconds elapsed) override;

    void set_fault(InjectedFault fault) noexcept { fault_ = fault; }
    [[nodiscard]] InjectedFault fault() const noexcept { return fault_; }

    // Passa al guasto successivo, ciclicamente. E' il comando che il banco
    // espone all'operatore.
    void cycle_fault() noexcept;

private:
    SimulationProvider provider_;
    InjectedFault fault_{InjectedFault::none};
};

}  // namespace efis
