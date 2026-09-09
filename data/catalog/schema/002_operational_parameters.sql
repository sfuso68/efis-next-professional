PRAGMA foreign_keys = ON;

-- EFIS Catalog V2 operational layer.  The V1 aircraft/engine catalogue remains
-- intact; these tables add complete, field-level model/variant/airframe data.

CREATE TABLE operational_domains (
    id TEXT PRIMARY KEY,
    label TEXT NOT NULL,
    sort_order INTEGER NOT NULL UNIQUE
);

CREATE TABLE operational_parameter_definitions (
    key TEXT PRIMARY KEY,
    domain TEXT NOT NULL REFERENCES operational_domains(id),
    label TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    value_type TEXT NOT NULL CHECK(value_type IN (
        'NUMBER','INTEGER','BOOLEAN','TEXT','ENUM','DATE','DURATION',
        'TABLE','CURVE','JSON'
    )),
    canonical_unit TEXT,
    requirement TEXT NOT NULL CHECK(requirement IN ('REQUIRED','CONDITIONAL','OPTIONAL')),
    criticality TEXT NOT NULL CHECK(criticality IN (
        'SAFETY_CRITICAL','OPERATIONAL','INFORMATIONAL','MAINTENANCE'
    )),
    enum_values_json TEXT,
    applicability_text TEXT NOT NULL DEFAULT '',
    sort_order INTEGER NOT NULL DEFAULT 0,
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1))
);

CREATE TABLE operational_profiles (
    id INTEGER PRIMARY KEY,
    profile_key TEXT NOT NULL UNIQUE,
    parent_id INTEGER REFERENCES operational_profiles(id),
    scope_type TEXT NOT NULL CHECK(scope_type IN (
        'MODEL','VARIANT','ENGINE_MODEL','PROPELLER_MODEL','INSTALLED_AIRCRAFT'
    )),
    scope_ref TEXT NOT NULL,
    title TEXT NOT NULL,
    revision INTEGER NOT NULL DEFAULT 1 CHECK(revision > 0),
    lifecycle_state TEXT NOT NULL CHECK(lifecycle_state IN (
        'SIMULATION','DRAFT','VERIFIED','APPROVED','RETIRED'
    )),
    created_at TEXT NOT NULL,
    updated_at TEXT NOT NULL,
    notes TEXT NOT NULL DEFAULT ''
);

CREATE TABLE operational_parameter_values (
    profile_id INTEGER NOT NULL REFERENCES operational_profiles(id) ON DELETE CASCADE,
    parameter_key TEXT NOT NULL REFERENCES operational_parameter_definitions(key),
    disposition TEXT NOT NULL CHECK(disposition IN (
        'VALUE','UNKNOWN','NOT_APPLICABLE','NOT_INSTALLED','INHERIT'
    )),
    value_json TEXT,
    unit TEXT,
    approval_status TEXT NOT NULL CHECK(approval_status IN (
        'INDICATIVE','DOCUMENTED','VERIFIED','APPROVED','OWNER_OVERRIDE'
    )),
    source_id INTEGER REFERENCES sources(id),
    source_document TEXT,
    source_revision TEXT,
    source_page TEXT,
    applicability TEXT NOT NULL DEFAULT '',
    condition_json TEXT,
    uncertainty_json TEXT,
    rationale TEXT NOT NULL DEFAULT '',
    reviewed_by TEXT,
    reviewed_at TEXT,
    PRIMARY KEY(profile_id, parameter_key),
    CHECK(
        (disposition = 'VALUE' AND value_json IS NOT NULL) OR
        (disposition <> 'VALUE' AND value_json IS NULL)
    ),
    CHECK(
        disposition NOT IN ('NOT_APPLICABLE','NOT_INSTALLED') OR
        length(trim(rationale)) > 0
    )
);

CREATE TABLE operational_profile_releases (
    id INTEGER PRIMARY KEY,
    profile_id INTEGER NOT NULL REFERENCES operational_profiles(id),
    release_number INTEGER NOT NULL CHECK(release_number > 0),
    content_sha256 TEXT NOT NULL CHECK(length(content_sha256) = 64),
    dictionary_sha256 TEXT NOT NULL CHECK(length(dictionary_sha256) = 64),
    released_at TEXT NOT NULL,
    released_by TEXT NOT NULL,
    approval_reference TEXT NOT NULL,
    rollback_release_id INTEGER REFERENCES operational_profile_releases(id),
    UNIQUE(profile_id, release_number)
);

CREATE TABLE operational_validation_runs (
    id INTEGER PRIMARY KEY,
    profile_id INTEGER NOT NULL REFERENCES operational_profiles(id),
    run_at TEXT NOT NULL,
    validator_version TEXT NOT NULL,
    result TEXT NOT NULL CHECK(result IN ('PASS','FAIL')),
    required_total INTEGER NOT NULL,
    required_resolved INTEGER NOT NULL,
    conditional_unresolved INTEGER NOT NULL,
    safety_blockers INTEGER NOT NULL,
    report_json TEXT NOT NULL
);

CREATE INDEX operational_parameter_domain_idx
    ON operational_parameter_definitions(domain, sort_order, key);
CREATE INDEX operational_profile_parent_idx
    ON operational_profiles(parent_id);
CREATE INDEX operational_value_status_idx
    ON operational_parameter_values(profile_id, disposition, approval_status);
CREATE INDEX operational_release_profile_idx
    ON operational_profile_releases(profile_id, release_number);
