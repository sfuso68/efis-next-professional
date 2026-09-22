#include "terrain_projection_adapter.hpp"

#include "efis/display/terrain_projection.hpp"

double TerrainProjectionAdapter::tileWidth() const noexcept {
    return efis::display::TerrainProjection::tile_width_m;
}

double TerrainProjectionAdapter::tileDepth() const noexcept {
    return efis::display::TerrainProjection::tile_depth_m;
}

double TerrainProjectionAdapter::highDetailEnter() const noexcept {
    return efis::display::TerrainProjection::high_detail_enter_m;
}

double TerrainProjectionAdapter::highDetailExit() const noexcept {
    return efis::display::TerrainProjection::high_detail_exit_m;
}

double TerrainProjectionAdapter::preloadMargin() const noexcept {
    return efis::display::TerrainProjection::preload_margin_m;
}

double TerrainProjectionAdapter::worldX(double latitude, double longitude) const noexcept {
    return efis::display::terrain_world_point(latitude, longitude).x_m;
}

double TerrainProjectionAdapter::worldZ(double latitude, double longitude) const noexcept {
    return efis::display::terrain_world_point(latitude, longitude).z_m;
}

double TerrainProjectionAdapter::footprintDistance(double cameraX, double cameraZ, double tileX,
                                                   double tileZ) const noexcept {
    return efis::display::terrain_tile_footprint_distance(cameraX, cameraZ, tileX, tileZ);
}

bool TerrainProjectionAdapter::highDetailForDistance(double distance,
                                                     bool currentlyHighDetail) const noexcept {
    return efis::display::terrain_high_detail(distance, currentlyHighDetail);
}

bool TerrainProjectionAdapter::tileEnabledForDistance(double distance) const noexcept {
    return efis::display::terrain_tile_preloaded(distance);
}
