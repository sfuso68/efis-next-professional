PRAGMA foreign_keys = ON;

CREATE TABLE catalog_meta (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE sources (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    publisher TEXT,
    document_ref TEXT,
    revision TEXT,
    url TEXT,
    accessed_at TEXT,
    source_class TEXT NOT NULL CHECK(source_class IN ('OFFICIAL','SECONDARY','LEGACY','OWNER'))
);

CREATE TABLE manufacturers (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    country TEXT
);

CREATE TABLE engines (
    id INTEGER PRIMARY KEY,
    manufacturer_id INTEGER NOT NULL REFERENCES manufacturers(id),
    family TEXT NOT NULL,
    model TEXT NOT NULL,
    variant TEXT NOT NULL DEFAULT '',
    cylinders INTEGER,
    power_hp REAL,
    rpm_max REAL,
    fuel_type TEXT,
    status TEXT NOT NULL CHECK(status IN ('INDICATIVE','DOCUMENTED','VERIFIED','APPROVED')),
    UNIQUE(manufacturer_id, model, variant)
);

CREATE TABLE engine_limits (
    id INTEGER PRIMARY KEY,
    engine_id INTEGER NOT NULL REFERENCES engines(id) ON DELETE CASCADE,
    code TEXT NOT NULL,
    minimum REAL,
    green_min REAL,
    green_max REAL,
    caution_min REAL,
    caution_max REAL,
    warning_min REAL,
    warning_max REAL,
    maximum REAL,
    unit TEXT NOT NULL,
    source_id INTEGER REFERENCES sources(id),
    status TEXT NOT NULL CHECK(status IN ('INDICATIVE','DOCUMENTED','VERIFIED','APPROVED','OWNER_OVERRIDE')),
    UNIQUE(engine_id, code)
);

CREATE TABLE aircraft (
    id INTEGER PRIMARY KEY,
    manufacturer_id INTEGER NOT NULL REFERENCES manufacturers(id),
    model TEXT NOT NULL,
    variant TEXT NOT NULL DEFAULT '',
    category TEXT NOT NULL CHECK(category IN ('VDS','ULM','LSA','EXPERIMENTAL','GA','GLIDER')),
    icao_type TEXT,
    seats INTEGER,
    wing_position TEXT,
    gear_type TEXT,
    empty_weight_kg REAL,
    mtow_kg REAL,
    fuel_capacity_l REAL,
    cruise_kt REAL,
    default_engine_id INTEGER REFERENCES engines(id),
    status TEXT NOT NULL CHECK(status IN ('INDICATIVE','DOCUMENTED','VERIFIED','APPROVED')),
    UNIQUE(manufacturer_id, model, variant)
);

CREATE TABLE aircraft_speeds (
    id INTEGER PRIMARY KEY,
    aircraft_id INTEGER NOT NULL REFERENCES aircraft(id) ON DELETE CASCADE,
    code TEXT NOT NULL CHECK(code IN ('VSO','VS1','VFE','VX','VY','VA','VNO','VNE','BEST_GLIDE','VREF')),
    value_kt REAL NOT NULL,
    condition_text TEXT NOT NULL DEFAULT '',
    source_id INTEGER REFERENCES sources(id),
    status TEXT NOT NULL CHECK(status IN ('INDICATIVE','DOCUMENTED','VERIFIED','APPROVED','OWNER_OVERRIDE')),
    UNIQUE(aircraft_id, code, condition_text)
);

CREATE TABLE aircraft_engine_compatibility (
    aircraft_id INTEGER NOT NULL REFERENCES aircraft(id) ON DELETE CASCADE,
    engine_id INTEGER NOT NULL REFERENCES engines(id) ON DELETE CASCADE,
    source_id INTEGER REFERENCES sources(id),
    status TEXT NOT NULL CHECK(status IN ('INDICATIVE','DOCUMENTED','VERIFIED','APPROVED')),
    PRIMARY KEY(aircraft_id, engine_id)
);

CREATE TABLE profile_revisions (
    id INTEGER PRIMARY KEY,
    profile_id TEXT NOT NULL,
    revision INTEGER NOT NULL,
    sha256 TEXT NOT NULL,
    activated_at TEXT,
    catalog_version TEXT NOT NULL,
    UNIQUE(profile_id, revision)
);

CREATE INDEX aircraft_category_idx ON aircraft(category);
CREATE INDEX aircraft_model_idx ON aircraft(model);
CREATE INDEX engine_model_idx ON engines(model);
CREATE INDEX speed_aircraft_idx ON aircraft_speeds(aircraft_id);

