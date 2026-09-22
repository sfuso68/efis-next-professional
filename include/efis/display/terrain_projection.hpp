#pragma once

namespace efis::display {

struct TerrainWorldPoint final {
    double x_m{};
    double z_m{};
};

struct TerrainProjection final {
    static constexpr double reference_latitude_deg = 46.5;
    static constexpr double reference_longitude_deg = 12.5;
    static constexpr double tile_width_m = 77150.0;
    static constexpr double tile_depth_m = 111320.0;
    static constexpr double high_detail_enter_m = 18000.0;
    static constexpr double high_detail_exit_m = 24000.0;
    static constexpr double preload_margin_m = 45000.0;
};

[[nodiscard]] TerrainWorldPoint terrain_world_point(double latitude_deg,
                                                    double longitude_deg) noexcept;

// Distanza minima sul piano X/Z fra la camera e il rettangolo coperto dalla
// tile. Una camera dentro la tile o esattamente sul bordo ha distanza zero.
// Usare la distanza dal centro rendeva entrambe le tile a bassa risoluzione
// proprio sul confine N45E012/N46E012.
[[nodiscard]] double
terrain_tile_footprint_distance(double camera_x_m, double camera_z_m, double tile_center_x_m,
                                double tile_center_z_m,
                                double tile_width_m = TerrainProjection::tile_width_m,
                                double tile_depth_m = TerrainProjection::tile_depth_m) noexcept;

// La selezione LOD usa due soglie per non oscillare quando la camera resta
// vicina al limite. Un valore non finito fallisce chiuso verso il dettaglio
// basso e non abilita il caricamento.
[[nodiscard]] bool terrain_high_detail(double footprint_distance_m,
                                       bool currently_high_detail) noexcept;
[[nodiscard]] bool terrain_tile_preloaded(double footprint_distance_m) noexcept;

} // namespace efis::display
