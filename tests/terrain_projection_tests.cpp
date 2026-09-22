#include "efis/display/terrain_projection.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

bool close_to(double got, double expected, double tolerance = 1e-6) {
    return std::abs(got - expected) <= tolerance;
}

} // namespace

int main() {
    using namespace efis::display;

    const auto origin = terrain_world_point(46.5, 12.5);
    require(close_to(origin.x_m, 0.0) && close_to(origin.z_m, 0.0),
            "the frozen projection origin must remain the N46E012 centre");

    const auto n45e012 = terrain_world_point(45.5, 12.5);
    require(close_to(n45e012.x_m, 0.0) && close_to(n45e012.z_m, TerrainProjection::tile_depth_m),
            "N45E012 must be exactly one tile south of N46E012");

    const auto n46e011 = terrain_world_point(46.5, 11.5);
    require(close_to(n46e011.x_m, -TerrainProjection::tile_width_m) && close_to(n46e011.z_m, 0.0),
            "N46E011 must be exactly one tile west of N46E012");

    const auto boundary = terrain_world_point(46.0, 12.5);
    const double north_distance =
        terrain_tile_footprint_distance(boundary.x_m, boundary.z_m, origin.x_m, origin.z_m);
    const double south_distance =
        terrain_tile_footprint_distance(boundary.x_m, boundary.z_m, n45e012.x_m, n45e012.z_m);
    require(close_to(north_distance, 0.0) && close_to(south_distance, 0.0),
            "both tiles must retain high detail at the N45/N46 seam");
    require(terrain_high_detail(north_distance, false) &&
                terrain_high_detail(south_distance, false),
            "the seam must never fall into the former low-detail hole");

    const auto liph = terrain_world_point(45.6484, 12.1944);
    require(close_to(terrain_tile_footprint_distance(liph.x_m, liph.z_m, n45e012.x_m, n45e012.z_m),
                     0.0),
            "Treviso must be inside the N45E012 high-detail footprint");

    require(terrain_high_detail(TerrainProjection::high_detail_enter_m - 1.0, false),
            "low-to-high transition must occur inside the enter threshold");
    require(!terrain_high_detail(TerrainProjection::high_detail_enter_m + 1.0, false),
            "low detail must not enter the hysteresis band prematurely");
    require(terrain_high_detail(TerrainProjection::high_detail_exit_m - 1.0, true),
            "high detail must remain stable inside the exit threshold");
    require(!terrain_high_detail(TerrainProjection::high_detail_exit_m + 1.0, true),
            "high detail must exit beyond the hysteresis band");

    require(terrain_tile_preloaded(TerrainProjection::preload_margin_m - 1.0),
            "a tile just inside the preload margin must remain available");
    require(!terrain_tile_preloaded(TerrainProjection::preload_margin_m + 1.0),
            "a tile beyond the preload margin must not consume GPU resources");

    const double invalid = std::numeric_limits<double>::quiet_NaN();
    const auto invalid_point = terrain_world_point(invalid, 12.5);
    require(!std::isfinite(invalid_point.x_m) && !std::isfinite(invalid_point.z_m),
            "invalid navigation must not be projected onto a plausible origin");
    require(!terrain_high_detail(invalid, true) && !terrain_tile_preloaded(invalid),
            "non-finite camera geometry must fail closed");
    require(std::isinf(terrain_tile_footprint_distance(invalid, 0.0, 0.0, 0.0)),
            "non-finite geometry must produce an unusable distance");

    std::cout << "All terrain projection tests passed\n";
}
