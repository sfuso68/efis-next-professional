#pragma once

#include "efis/core/types.hpp"

namespace efis {

template <class T>
struct Sample final {
    T value{};
    SourceId source{SourceId::unknown};
    MonoTime sampled_at{};
    MonoTime received_at{};
    Quality quality{Quality::unknown};
    Validity validity{Validity::unavailable};
    std::uint64_t sequence{};

    [[nodiscard]] constexpr bool usable() const noexcept {
        return is_usable(validity);
    }
};

}  // namespace efis
