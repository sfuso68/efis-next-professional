#pragma once

#include "efis/core/engine_source.hpp"
#include "efis/providers/engine_simulation_provider.hpp"

#include <cstdint>

namespace efis {

// La simulazione motore vista come una sorgente qualunque.
//
// Il guasto iniettato resta qui, dove appartiene: e' un comportamento della
// sorgente simulata, non del quadro che la mostra.
class EngineSimulationSource final : public EngineSource {
public:
    explicit EngineSimulationSource(std::uint64_t seed = 84) noexcept
        : provider_(seed) {}

    [[nodiscard]] SourceId source() const noexcept override {
        return SourceId::simulation;
    }

    [[nodiscard]] engine::EngineSnapshot poll(
        MonoTime now, std::chrono::milliseconds elapsed) override {
        return provider_.sample(now, elapsed);
    }

    void set_fault(engine::EngineFault fault) noexcept { provider_.set_fault(fault); }
    [[nodiscard]] engine::EngineFault fault() const noexcept { return provider_.fault(); }

private:
    EngineSimulationProvider provider_;
};

}  // namespace efis
