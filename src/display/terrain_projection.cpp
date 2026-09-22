#include "efis/display/terrain_projection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace efis::display {

TerrainWorldPoint terrain_world_point(double latitude_deg, double longitude_deg) noexcept {
    if (!std::isfinite(latitude_deg) || !std::isfinite(longitude_deg)) {
        const double invalid = std::numeric_limits<double>::quiet_NaN();
        return {invalid, invalid};
    }
    return {
        (longitude_deg - TerrainProjection::reference_longitude_deg) *
            TerrainProjection::tile_width_m,
        -(latitude_deg - TerrainProjection::reference_latitude_deg) *
            TerrainProjection::tile_depth_m,
    };
}

double terrain_tile_footprint_distance(double camera_x_m, double camera_z_m, double tile_center_x_m,
                                       double tile_center_z_m, double tile_width_m,
                                       double tile_depth_m) noexcept {
    if (!std::isfinite(camera_x_m) || !std::isfinite(camera_z_m) ||
        !std::isfinite(tile_center_x_m) || !std::isfinite(tile_center_z_m) ||
        !std::isfinite(tile_width_m) || !std::isfinite(tile_depth_m) || tile_width_m <= 0.0 ||
        tile_depth_m <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    const double dx = std::max(std::abs(camera_x_m - tile_center_x_m) - tile_width_m * 0.5, 0.0);
    const double dz = std::max(std::abs(camera_z_m - tile_center_z_m) - tile_depth_m * 0.5, 0.0);
    return std::hypot(dx, dz);
}

bool terrain_high_detail(double footprint_distance_m, bool currently_high_detail) noexcept {
    if (!std::isfinite(footprint_distance_m) || footprint_distance_m < 0.0) {
        return false;
    }
    const double threshold = currently_high_detail ? TerrainProjection::high_detail_exit_m
                                                   : TerrainProjection::high_detail_enter_m;
    return footprint_distance_m < threshold;
}

bool terrain_tile_preloaded(double footprint_distance_m) noexcept {
    return std::isfinite(footprint_distance_m) && footprint_distance_m >= 0.0 &&
           footprint_distance_m < TerrainProjection::preload_margin_m;
}

} // namespace efis::display
