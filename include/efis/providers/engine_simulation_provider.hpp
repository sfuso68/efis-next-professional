#pragma once

#include "efis/engine/engine_snapshot.hpp"

#include <chrono>
#include <cstdint>

namespace efis {

class EngineSimulationProvider final {
public:
    explicit EngineSimulationProvider(std::uint64_t seed = 84) noexcept;
    [[nodiscard]] engine::EngineSnapshot sample(MonoTime now,
        std::chrono::milliseconds elapsed);
    void set_fault(engine::EngineFault fault) noexcept { fault_ = fault; }
    [[nodiscard]] engine::EngineFault fault() const noexcept { return fault_; }

private:
    std::uint64_t seed_{};
    std::uint64_t sequence_{};
    engine::EngineFault fault_{engine::EngineFault::none};
};

}  // namespace efis
