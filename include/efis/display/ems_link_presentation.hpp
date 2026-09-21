#pragma once

#include "efis/protocol/edp_v1.hpp"

#include <string_view>

namespace efis::display {

struct EmsLinkPresentation final {
    std::string_view engine_state;
    std::string_view data_state;
    std::string_view sensor_state;
    bool use_network{};
    bool clear_age{};
};

// Pure presentation policy shared by the operational controller and its native
// behavioral test. No UI framework is involved, so every EDP transition is
// testable without reading source text or relying on a rendered label.
[[nodiscard]] constexpr EmsLinkPresentation present_ems_link(
    protocol::edp::LinkState state, bool simulated) noexcept {
    switch (state) {
        case protocol::edp::LinkState::live:
            return simulated
                ? EmsLinkPresentation{"LIVE • SIM", "SIM", "BLACKBOX 47 • SIM", true, false}
                : EmsLinkPresentation{"LIVE", "LIVE", "BLACKBOX 47", true, false};
        case protocol::edp::LinkState::stale:
            return simulated
                ? EmsLinkPresentation{"STALE • SIM", "STALE", "BLACKBOX STALE • SIM", true, false}
                : EmsLinkPresentation{"STALE", "STALE", "BLACKBOX STALE", true, false};
        case protocol::edp::LinkState::waiting:
            return {"SIM FALLBACK • WAITING", "LOST", "BLACKBOX WAITING", false, true};
        case protocol::edp::LinkState::lost:
            return {"SIM FALLBACK • LINK LOST", "LOST", "BLACKBOX LINK LOST", false, false};
    }
    return {"SIM FALLBACK • LINK LOST", "LOST", "BLACKBOX LINK LOST", false, false};
}

}  // namespace efis::display
