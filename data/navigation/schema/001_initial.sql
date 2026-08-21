PRAGMA foreign_keys = ON;

CREATE TABLE nav_meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE data_sources (
    id INTEGER PRIMARY KEY, name TEXT NOT NULL, provider TEXT NOT NULL,
    edition TEXT, effective_from TEXT, effective_to TEXT, licence TEXT,
    source_class TEXT NOT NULL CHECK(source_class IN ('AUTHORISED','OFFICIAL_PUBLIC','OPEN','DEVELOPMENT'))
);
CREATE TABLE airports (
    id INTEGER PRIMARY KEY, icao TEXT NOT NULL UNIQUE, iata TEXT, name TEXT NOT NULL,
    city TEXT, country TEXT NOT NULL, latitude_deg REAL NOT NULL,
    longitude_deg REAL NOT NULL, elevation_ft REAL NOT NULL,
    airport_type TEXT NOT NULL, source_id INTEGER NOT NULL REFERENCES data_sources(id),
    status TEXT NOT NULL CHECK(status IN ('ACTIVE','LIMITED','CLOSED','DEVELOPMENT'))
);
CREATE TABLE runways (
    id INTEGER PRIMARY KEY, airport_id INTEGER NOT NULL REFERENCES airports(id) ON DELETE CASCADE,
    designator_low TEXT NOT NULL, designator_high TEXT NOT NULL,
    true_heading_low_deg REAL NOT NULL, true_heading_high_deg REAL NOT NULL,
    length_m REAL NOT NULL, width_m REAL NOT NULL, surface TEXT,
    threshold_low_lat REAL, threshold_low_lon REAL, threshold_low_elevation_ft REAL,
    threshold_high_lat REAL, threshold_high_lon REAL, threshold_high_elevation_ft REAL,
    source_id INTEGER NOT NULL REFERENCES data_sources(id), status TEXT NOT NULL,
    UNIQUE(airport_id, designator_low, designator_high)
);
CREATE TABLE obstacles (
    id INTEGER PRIMARY KEY, obstacle_uid TEXT NOT NULL UNIQUE,
    latitude_deg REAL NOT NULL, longitude_deg REAL NOT NULL,
    elevation_msl_m REAL, height_agl_m REAL, obstacle_type TEXT NOT NULL,
    lighting TEXT, marking TEXT, source_id INTEGER NOT NULL REFERENCES data_sources(id),
    effective_from TEXT, effective_to TEXT, status TEXT NOT NULL
);
CREATE TABLE terrain_tiles (
    id INTEGER PRIMARY KEY, tile_key TEXT NOT NULL UNIQUE, min_lat REAL NOT NULL,
    min_lon REAL NOT NULL, max_lat REAL NOT NULL, max_lon REAL NOT NULL,
    horizontal_resolution_m REAL NOT NULL, vertical_datum TEXT NOT NULL,
    storage_path TEXT NOT NULL, sha256 TEXT NOT NULL, source_id INTEGER NOT NULL REFERENCES data_sources(id),
    status TEXT NOT NULL
);
CREATE INDEX airport_position_idx ON airports(latitude_deg,longitude_deg);
CREATE INDEX runway_airport_idx ON runways(airport_id);
CREATE INDEX obstacle_position_idx ON obstacles(latitude_deg,longitude_deg);
CREATE INDEX terrain_bounds_idx ON terrain_tiles(min_lat,min_lon,max_lat,max_lon);
