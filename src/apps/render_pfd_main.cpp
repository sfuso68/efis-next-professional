#include "efis/core/flight_model.hpp"
#include "efis/display/display_model.hpp"
#include "efis/display/svg_pfd_renderer.hpp"
#include "efis/providers/simulation_provider.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    using namespace std::chrono_literals;
    if (argc != 4) {
        std::cerr << "usage: efis-render-pfd <output.svg> <width> <height>\n";
        return 2;
    }
    const efis::display::RenderSize size{std::stoi(argv[2]), std::stoi(argv[3])};
    efis::SimulationProvider provider{42};
    const auto now = efis::MonoTime{} + 30s;
    efis::FlightModel model;
    const auto snapshot = model.normalize(provider.sample(now, 30s), now);
    const auto view = efis::display::make_pfd_view(
        snapshot, efis::display::DisplayProfile::professional);
    const auto svg = efis::display::SvgPfdRenderer{}.render(view, size);
    std::ofstream output(argv[1], std::ios::binary);
    if (!output) return 3;
    output << svg;
    std::cout << "rendered " << argv[1] << " at " << size.width << 'x' << size.height << '\n';
}
