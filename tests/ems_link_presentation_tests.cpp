#include "efis/display/ems_link_presentation.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    using efis::display::present_ems_link;
    using efis::protocol::edp::LinkState;

    const auto live = present_ems_link(LinkState::live, false);
    require(live.use_network && live.data_state == "LIVE" &&
            live.engine_state == "LIVE" && live.sensor_state == "BLACKBOX 47",
            "real live EDP must be presented as live network data");

    const auto live_sim = present_ems_link(LinkState::live, true);
    require(live_sim.use_network && live_sim.data_state == "SIM" &&
            live_sim.engine_state == "LIVE • SIM" &&
            live_sim.sensor_state == "BLACKBOX 47 • SIM",
            "simulated live EDP must retain the simulation marker");

    const auto stale = present_ems_link(LinkState::stale, false);
    require(stale.use_network && stale.data_state == "STALE" &&
            stale.engine_state == "STALE" && stale.sensor_state == "BLACKBOX STALE",
            "stale EDP must remain visible but explicitly stale");

    const auto stale_sim = present_ems_link(LinkState::stale, true);
    require(stale_sim.use_network && stale_sim.data_state == "STALE" &&
            stale_sim.engine_state == "STALE • SIM" &&
            stale_sim.sensor_state == "BLACKBOX STALE • SIM",
            "stale simulated EDP must retain both stale and simulation semantics");

    const auto waiting = present_ems_link(LinkState::waiting, false);
    require(!waiting.use_network && waiting.data_state == "LOST" &&
            waiting.engine_state == "SIM FALLBACK • WAITING" &&
            waiting.sensor_state == "BLACKBOX WAITING",
            "waiting link must fail closed to an explicit simulation fallback");

    const auto lost = present_ems_link(LinkState::lost, false);
    require(!lost.use_network && lost.data_state == "LOST" &&
            lost.engine_state == "SIM FALLBACK • LINK LOST" &&
            lost.sensor_state == "BLACKBOX LINK LOST",
            "lost link must not appear live or retain network authority");

    std::cout << "EMS link presentation behavioral tests passed\n";
    return EXIT_SUCCESS;
}
