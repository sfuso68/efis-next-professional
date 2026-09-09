#!/usr/bin/env python3
"""Build and validate EFIS Catalog V1 using only the Python standard library."""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
from pathlib import Path

STATES = {"SIMULATION", "DRAFT", "VERIFIED", "APPROVED"}
SPEEDS = {"VSO", "VS1", "VFE", "VX", "VY", "VA", "VNO", "VNE", "BEST_GLIDE", "VREF"}
OPERATIONAL_STATES = STATES | {"RETIRED"}
SCOPE_TYPES = {"MODEL", "VARIANT", "ENGINE_MODEL", "PROPELLER_MODEL", "INSTALLED_AIRCRAFT"}
DISPOSITIONS = {"VALUE", "UNKNOWN", "NOT_APPLICABLE", "NOT_INSTALLED", "INHERIT"}
APPROVAL_STATUSES = {"INDICATIVE", "DOCUMENTED", "VERIFIED", "APPROVED", "OWNER_OVERRIDE"}
VALUE_TYPES = {"NUMBER", "INTEGER", "BOOLEAN", "TEXT", "ENUM", "DATE", "DURATION", "TABLE", "CURVE", "JSON"}
REQUIREMENTS = {"REQUIRED", "CONDITIONAL", "OPTIONAL"}
CRITICALITIES = {"SAFETY_CRITICAL", "OPERATIONAL", "INFORMATIONAL", "MAINTENANCE"}


def read_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def flatten_parameter_dictionary(dictionary: dict) -> tuple[list[dict], list[str]]:
    """Validate and flatten the authoritative operational field dictionary."""
    errors: list[str] = []
    definitions: list[dict] = []
    seen: set[str] = set()
    if dictionary.get("schema") != "efis-next.operational-parameter-dictionary.v2":
        errors.append("invalid operational dictionary schema")
    if not dictionary.get("policy", {}).get("no_silent_omissions"):
        errors.append("dictionary must enable no_silent_omissions")
    for domain_index, domain in enumerate(dictionary.get("domains", [])):
        domain_id = domain.get("id", "")
        defaults = domain.get("defaults", {})
        if not domain_id:
            errors.append(f"domain {domain_index} has no id")
            continue
        for parameter_index, raw in enumerate(domain.get("parameters", [])):
            item = dict(defaults)
            item.update(raw)
            key = f"{domain_id}.{item.get('id', '')}"
            if key in seen:
                errors.append(f"duplicate parameter: {key}")
            seen.add(key)
            value_type = item.get("type")
            requirement = item.get("requirement")
            criticality = item.get("criticality")
            if not item.get("id") or not item.get("label"):
                errors.append(f"invalid parameter identity: {key}")
            if value_type not in VALUE_TYPES:
                errors.append(f"invalid value type for {key}: {value_type}")
            if requirement not in REQUIREMENTS:
                errors.append(f"invalid requirement for {key}: {requirement}")
            if criticality not in CRITICALITIES:
                errors.append(f"invalid criticality for {key}: {criticality}")
            if value_type in {"NUMBER", "DURATION"} and not item.get("unit"):
                errors.append(f"missing canonical unit for {key}")
            if value_type == "ENUM" and not item.get("enum"):
                errors.append(f"missing enum values for {key}")
            item.update({"key": key, "domain": domain_id, "sort_order": domain_index * 1000 + parameter_index})
            definitions.append(item)
    if not definitions:
        errors.append("operational dictionary is empty")
    return definitions, errors


def _value_matches(value: object, definition: dict) -> bool:
    kind = definition["type"]
    if kind == "NUMBER":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if kind == "INTEGER":
        return isinstance(value, int) and not isinstance(value, bool)
    if kind == "BOOLEAN":
        return isinstance(value, bool)
    if kind in {"TEXT", "DATE"}:
        return isinstance(value, str) and bool(value.strip())
    if kind == "DURATION":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if kind == "ENUM":
        return value in definition.get("enum", [])
    if kind in {"TABLE", "CURVE"}:
        return isinstance(value, list)
    return isinstance(value, (dict, list, str, int, float, bool)) or value is None


def materialize_operational_profile(dictionary: dict, profile: dict) -> tuple[dict[str, dict], list[str]]:
    definitions, errors = flatten_parameter_dictionary(dictionary)
    known = {item["key"]: item for item in definitions}
    values = profile.get("values", {})
    if not isinstance(values, dict):
        errors.append("operational profile values must be an object")
        values = {}
    for key in values:
        if key not in known:
            errors.append(f"unknown operational parameter: {key}")
    default_disposition = profile.get("default_disposition", "UNKNOWN")
    if default_disposition not in DISPOSITIONS:
        errors.append(f"invalid default disposition: {default_disposition}")
        default_disposition = "UNKNOWN"
    if not profile.get("parent_profile") and default_disposition == "INHERIT":
        errors.append("root operational profile cannot default to INHERIT")
    default_status = profile.get("default_approval_status", "INDICATIVE")
    if default_status not in APPROVAL_STATUSES:
        errors.append(f"invalid default approval status: {default_status}")
        default_status = "INDICATIVE"
    materialized = {
        key: {"disposition": default_disposition, "approval_status": default_status}
        for key in known
    }
    for key, entry in values.items():
        if key in known:
            if not isinstance(entry, dict):
                errors.append(f"{key}: profile entry must be an object")
                continue
            materialized[key] = dict(entry)
            materialized[key].setdefault("approval_status", default_status)
    return materialized, errors


def validate_operational_profile(dictionary: dict, profile: dict, parent: dict | None = None) -> tuple[list[str], dict]:
    """Validate field typing, completeness and release blockers."""
    errors: list[str] = []
    for field in ("schema", "profile_key", "scope_type", "scope_ref", "title", "revision", "lifecycle_state", "created_at", "updated_at", "values"):
        if field not in profile:
            errors.append(f"missing operational profile field: {field}")
    if profile.get("schema") != "efis-next.operational-profile.v2":
        errors.append("invalid operational profile schema")
    if profile.get("scope_type") not in SCOPE_TYPES:
        errors.append(f"invalid scope type: {profile.get('scope_type')}")
    if profile.get("lifecycle_state") not in OPERATIONAL_STATES:
        errors.append(f"invalid lifecycle state: {profile.get('lifecycle_state')}")
    if not isinstance(profile.get("revision"), int) or isinstance(profile.get("revision"), bool) or profile.get("revision", 0) < 1:
        errors.append("operational profile revision must be a positive integer")
    definitions, dictionary_errors = flatten_parameter_dictionary(dictionary)
    errors.extend(dictionary_errors)
    known = {item["key"]: item for item in definitions}
    materialized, materialize_errors = materialize_operational_profile(dictionary, profile)
    errors.extend(materialize_errors)
    inherited = None
    if parent is not None:
        inherited, parent_errors = materialize_operational_profile(dictionary, parent)
        errors.extend(f"parent: {error}" for error in parent_errors)
    required_total = required_resolved = conditional_unresolved = safety_blockers = 0
    disposition_counts = {name: 0 for name in sorted(DISPOSITIONS)}
    blockers: list[str] = []
    release_state = profile.get("lifecycle_state") in {"VERIFIED", "APPROVED"}
    for key, entry in materialized.items():
        definition = known[key]
        disposition = entry.get("disposition")
        if disposition not in DISPOSITIONS:
            errors.append(f"{key}: invalid disposition {disposition}")
            continue
        disposition_counts[disposition] += 1
        status = entry.get("approval_status", profile.get("default_approval_status", "INDICATIVE"))
        if status not in APPROVAL_STATUSES:
            errors.append(f"{key}: invalid approval status {status}")
        if disposition == "VALUE":
            if "value" not in entry:
                errors.append(f"{key}: VALUE requires value")
            elif not _value_matches(entry["value"], definition):
                errors.append(f"{key}: value does not match {definition['type']}")
            canonical_unit = definition.get("unit")
            if canonical_unit and entry.get("unit", canonical_unit) != canonical_unit:
                errors.append(f"{key}: unit must be canonical {canonical_unit}")
        elif "value" in entry:
            errors.append(f"{key}: {disposition} must not contain value")
        if disposition in {"NOT_APPLICABLE", "NOT_INSTALLED"} and not entry.get("rationale"):
            errors.append(f"{key}: {disposition} requires rationale")
        resolved = disposition in {"VALUE", "NOT_APPLICABLE", "NOT_INSTALLED"}
        if disposition == "INHERIT" and inherited is not None:
            resolved = inherited[key].get("disposition") in {"VALUE", "NOT_APPLICABLE", "NOT_INSTALLED"}
        if definition["requirement"] == "REQUIRED":
            required_total += 1
            required_resolved += int(resolved)
        if definition["requirement"] == "CONDITIONAL" and not resolved:
            conditional_unresolved += 1
        if definition["criticality"] == "SAFETY_CRITICAL" and not resolved:
            safety_blockers += 1
            blockers.append(key)
        if release_state and not resolved and definition["requirement"] == "REQUIRED":
            errors.append(f"{key}: unresolved required parameter blocks {profile.get('lifecycle_state')}")
        if release_state and disposition == "VALUE" and definition["criticality"] == "SAFETY_CRITICAL":
            if not entry.get("source") or not entry.get("source_document") or not entry.get("source_page"):
                errors.append(f"{key}: released safety-critical value requires source, document and page")
    unmapped = materialized.get("data_governance.unmapped_source_fields", {})
    if release_state and unmapped.get("disposition") == "VALUE" and unmapped.get("value"):
        errors.append("unmapped source fields block VERIFIED/APPROVED release")
    report = {
        "schema": "efis-next.operational-validation-report.v2",
        "profile_key": profile.get("profile_key"),
        "parameter_total": len(definitions),
        "materialized_total": len(materialized),
        "dispositions": disposition_counts,
        "required_total": required_total,
        "required_resolved": required_resolved,
        "conditional_unresolved": conditional_unresolved,
        "safety_blockers": safety_blockers,
        "blocker_keys": blockers,
        "result": "PASS" if not errors else "FAIL",
        "release_ready": bool(
            not errors
            and release_state
            and required_resolved == required_total
            and safety_blockers == 0
        ),
        "errors": errors,
    }
    return errors, report


def validate_operational_seed(dictionary: dict, seed: dict) -> tuple[list[str], list[dict]]:
    errors: list[str] = []
    reports: list[dict] = []
    profiles = seed.get("profiles", [])
    keys = [profile.get("profile_key") for profile in profiles]
    duplicates = sorted({key for key in keys if key and keys.count(key) > 1})
    if duplicates:
        errors.append("duplicate operational profile keys: " + ", ".join(duplicates))
    by_key = {profile.get("profile_key"): profile for profile in profiles if profile.get("profile_key")}
    for profile in profiles:
        parent_key = profile.get("parent_profile")
        if parent_key and parent_key not in by_key:
            errors.append(f"{profile.get('profile_key')}: unknown parent profile {parent_key}")
            continue
        profile_errors, report = validate_operational_profile(dictionary, profile, by_key.get(parent_key))
        errors.extend(f"{profile.get('profile_key')}: {error}" for error in profile_errors)
        reports.append(report)
    for starting_key in by_key:
        visited: set[str] = set()
        key = starting_key
        while key:
            if key in visited:
                errors.append(f"operational profile inheritance cycle at {key}")
                break
            visited.add(key)
            key = by_key.get(key, {}).get("parent_profile")
    return errors, reports


def write_parameter_register(dictionary: dict, output: Path) -> int:
    definitions, errors = flatten_parameter_dictionary(dictionary)
    if errors:
        raise ValueError("invalid operational dictionary: " + "; ".join(errors))

    def cell(value: object) -> str:
        return str(value or "—").replace("|", "\\|").replace("\n", " ")

    lines = [
        "# EFIS NEXT — Operational Parameter Register V2",
        "",
        "> Generated from `data/catalog/operational-parameter-dictionary-v2.json`.",
        "> Do not edit manually. Every identifier below becomes one row per materialized profile.",
        "",
        f"**Parameters:** {len(definitions)}  ",
        f"**Domains:** {len(dictionary['domains'])}  ",
        f"**Dictionary version:** {dictionary['version']}",
        "",
    ]
    for domain in dictionary["domains"]:
        domain_definitions = [definition for definition in definitions if definition["domain"] == domain["id"]]
        lines += [
            f"## {cell(domain.get('label'))} (`{domain['id']}`) — {len(domain_definitions)}",
            "",
            "| Parameter | Label | Type | Unit | Requirement | Criticality |",
            "|---|---|---|---|---|---|",
        ]
        for definition in domain_definitions:
            lines.append(
                f"| `{definition['key']}` | {cell(definition['label'])} | {definition['type']} | "
                f"{cell(definition.get('unit'))} | {definition['requirement']} | {definition['criticality']} |"
            )
        lines.append("")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="utf-8")
    return len(definitions)


def validate_profile(profile: dict) -> list[str]:
    errors: list[str] = []
    for key in ("schema_version", "profile_id", "revision", "state", "aircraft", "engine", "speeds_kt", "units", "evidence"):
        if key not in profile:
            errors.append(f"missing field: {key}")
    if errors:
        return errors
    if profile["state"] not in STATES:
        errors.append(f"invalid state: {profile['state']}")
    aircraft = profile["aircraft"]
    if aircraft.get("empty_weight_kg", 0) >= aircraft.get("mtow_kg", 0):
        errors.append("empty_weight_kg must be lower than mtow_kg")
    speeds = profile["speeds_kt"]
    missing = SPEEDS - set(speeds)
    if missing:
        errors.append("missing speeds: " + ", ".join(sorted(missing)))
        return errors
    if not 0 < speeds["VSO"] <= speeds["VFE"]:
        errors.append("expected 0 < VSO <= VFE")
    if not 0 < speeds["VS1"] < speeds["VNO"] < speeds["VNE"]:
        errors.append("expected 0 < VS1 < VNO < VNE")
    if not speeds["VSO"] <= speeds["VREF"] < speeds["VNO"]:
        errors.append("VREF must be between VSO and VNO")
    if not speeds["VSO"] <= speeds["VX"] <= speeds["VNE"]:
        errors.append("VX outside display domain")
    if not speeds["VSO"] <= speeds["VY"] <= speeds["VNE"]:
        errors.append("VY outside display domain")
    if profile["state"] in {"VERIFIED", "APPROVED"} and profile["evidence"].get("status") not in {"VERIFIED", "APPROVED"}:
        errors.append("verified/approved profile requires verified evidence")
    return errors


def manufacturer_id(db: sqlite3.Connection, name: str) -> int:
    db.execute("INSERT OR IGNORE INTO manufacturers(name) VALUES (?)", (name,))
    row = db.execute("SELECT id FROM manufacturers WHERE name=?", (name,)).fetchone()
    assert row
    return int(row[0])


def split_engine_name(name: str) -> tuple[str, str]:
    cleaned = name.strip()
    if not cleaned or cleaned.lower().startswith("nessuno"):
        return "", ""
    parts = cleaned.split(maxsplit=1)
    return (parts[0], parts[1] if len(parts) > 1 else "Custom")


def build_catalog(
    schema: Path,
    seed_path: Path,
    output: Path,
    legacy_path: Path | None = None,
    operational_schema: Path | None = None,
    dictionary_path: Path | None = None,
    operational_seed_path: Path | None = None,
) -> dict[str, int]:
    seed = read_json(seed_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    staging_output = output.with_name(output.name + ".building")
    if staging_output.exists():
        staging_output.unlink()
    db = sqlite3.connect(staging_output)
    build_ok = False
    try:
        db.executescript(schema.read_text(encoding="utf-8"))
        if operational_schema:
            db.executescript(operational_schema.read_text(encoding="utf-8"))
        db.execute("INSERT INTO catalog_meta(key,value) VALUES('schema_version','1')")
        db.execute("INSERT INTO catalog_meta(key,value) VALUES('catalog_version',?)", (seed["catalog_version"],))
        source_ids: dict[str, int] = {}
        for source in seed["sources"]:
            cur = db.execute("INSERT INTO sources(title,publisher,document_ref,revision,url,accessed_at,source_class) VALUES(?,?,?,?,?,?,?)",
                             (source["title"], source.get("publisher"), source.get("document_ref"), source.get("revision"), source.get("url"), source.get("retrieved_at") or source.get("accessed_at"), source["source_class"]))
            source_ids[source["key"]] = int(cur.lastrowid)
        engine_ids: dict[str, int] = {}
        for engine in seed["engines"]:
            mid = manufacturer_id(db, engine["manufacturer"])
            cur = db.execute("INSERT INTO engines(manufacturer_id,family,model,variant,cylinders,power_hp,rpm_max,fuel_type,status) VALUES(?,?,?,?,?,?,?,?,?)",
                             (mid, engine["family"], engine["model"], engine.get("variant", ""), engine.get("cylinders"), engine.get("power_hp"), engine.get("rpm_max"), engine.get("fuel_type"), engine["status"]))
            key = f"{engine['manufacturer']}|{engine['model']}|{engine.get('variant','')}"
            engine_ids[key] = int(cur.lastrowid)
        for aircraft in seed["aircraft"]:
            mid = manufacturer_id(db, aircraft["manufacturer"])
            cur = db.execute("INSERT INTO aircraft(manufacturer_id,model,variant,category,icao_type,seats,empty_weight_kg,mtow_kg,fuel_capacity_l,cruise_kt,default_engine_id,status) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)",
                             (mid, aircraft["model"], aircraft.get("variant", ""), aircraft["category"], aircraft.get("icao_type"), aircraft.get("seats"), aircraft.get("empty_weight_kg"), aircraft.get("mtow_kg"), aircraft.get("fuel_capacity_l"), aircraft.get("cruise_kt"), engine_ids.get(aircraft.get("default_engine")), aircraft["status"]))
            aid = int(cur.lastrowid)
            source_id = source_ids[aircraft["source"]]
            for code, value in aircraft["speeds"].items():
                db.execute("INSERT INTO aircraft_speeds(aircraft_id,code,value_kt,source_id,status) VALUES(?,?,?,?,?)", (aid, code, value, source_id, aircraft["status"]))
        imported = {"aircraft": 0, "engines": 0, "duplicates": 0}
        if legacy_path:
            legacy = read_json(legacy_path)
            legacy_source = db.execute("SELECT id FROM sources WHERE source_class='LEGACY' ORDER BY id LIMIT 1").fetchone()[0]
            legacy_engine_ids: dict[str, int] = {}

            def ensure_legacy_engine(full_name: str, details: dict | None = None) -> int | None:
                key = full_name.strip().casefold()
                if not key or key.startswith("nessuno"):
                    return None
                if key in legacy_engine_ids:
                    return legacy_engine_ids[key]
                maker, model = split_engine_name(full_name)
                mid = manufacturer_id(db, maker)
                existing = db.execute("SELECT id FROM engines WHERE manufacturer_id=? AND lower(model)=lower(?) AND variant=''", (mid, model)).fetchone()
                if existing:
                    legacy_engine_ids[key] = int(existing[0])
                    return int(existing[0])
                details = details or {}
                cur = db.execute("INSERT INTO engines(manufacturer_id,family,model,variant,cylinders,power_hp,rpm_max,fuel_type,status) VALUES(?,?,?,?,?,?,?,?,?)",
                                 (mid, model.split()[0], model, "", details.get("cyl"), details.get("hp"), details.get("rpm_max"), details.get("fuel_type"), "INDICATIVE"))
                legacy_engine_ids[key] = int(cur.lastrowid)
                imported["engines"] += 1
                return int(cur.lastrowid)

            for engine in legacy.get("engines", []):
                ensure_legacy_engine(engine["name"], engine)
            for aircraft in legacy.get("aircraft", []):
                mid = manufacturer_id(db, aircraft["manufacturer"])
                duplicate = db.execute("SELECT id FROM aircraft WHERE manufacturer_id=? AND lower(model)=lower(?) AND variant=''", (mid, aircraft["name"])).fetchone()
                if duplicate:
                    imported["duplicates"] += 1
                    continue
                engine_id = ensure_legacy_engine(aircraft.get("engine_name", ""), {"hp": aircraft.get("power_hp"), "rpm_max": aircraft.get("rpm_max")})
                cur = db.execute("INSERT INTO aircraft(manufacturer_id,model,variant,category,icao_type,seats,wing_position,gear_type,empty_weight_kg,mtow_kg,fuel_capacity_l,cruise_kt,default_engine_id,status) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                                 (mid, aircraft["name"], "", aircraft["category"], aircraft.get("icao_type"), aircraft.get("seats"), aircraft.get("wing_position"), aircraft.get("gear_type"), aircraft.get("empty_weight_kg"), aircraft.get("mtow_kg"), aircraft.get("fuel_capacity_l"), aircraft.get("cruise_kt"), engine_id, "INDICATIVE"))
                aid = int(cur.lastrowid)
                imported["aircraft"] += 1
                for code, value in aircraft.get("speeds", {}).items():
                    if value and value > 0:
                        db.execute("INSERT INTO aircraft_speeds(aircraft_id,code,value_kt,source_id,status) VALUES(?,?,?,?,?)", (aid, code, value, legacy_source, "INDICATIVE"))
                if engine_id:
                    db.execute("INSERT OR IGNORE INTO aircraft_engine_compatibility(aircraft_id,engine_id,source_id,status) VALUES(?,?,?,?)", (aid, engine_id, legacy_source, "INDICATIVE"))
        if any((operational_schema, dictionary_path, operational_seed_path)):
            if not all((operational_schema, dictionary_path, operational_seed_path)):
                raise ValueError("operational schema, dictionary and seed must be supplied together")
            dictionary = read_json(dictionary_path)
            definitions, definition_errors = flatten_parameter_dictionary(dictionary)
            if definition_errors:
                raise ValueError("invalid operational dictionary: " + "; ".join(definition_errors))
            db.execute("UPDATE catalog_meta SET value='2' WHERE key='schema_version'")
            db.execute("INSERT INTO catalog_meta(key,value) VALUES('operational_dictionary_version',?)", (dictionary["version"],))
            for order, domain in enumerate(dictionary["domains"]):
                db.execute(
                    "INSERT INTO operational_domains(id,label,sort_order) VALUES(?,?,?)",
                    (domain["id"], domain["label"], order),
                )
            for definition in definitions:
                db.execute(
                    "INSERT INTO operational_parameter_definitions(key,domain,label,description,value_type,canonical_unit,requirement,criticality,enum_values_json,applicability_text,sort_order) VALUES(?,?,?,?,?,?,?,?,?,?,?)",
                    (definition["key"], definition["domain"], definition["label"], definition.get("description", ""), definition["type"], definition.get("unit"), definition["requirement"], definition["criticality"], json.dumps(definition.get("enum"), ensure_ascii=False) if definition.get("enum") else None, definition.get("applicability", ""), definition["sort_order"]),
                )
            operational_seed = read_json(operational_seed_path)
            seed_errors, _ = validate_operational_seed(dictionary, operational_seed)
            if seed_errors:
                raise ValueError("invalid operational seed: " + "; ".join(seed_errors))
            profiles_by_key = {profile["profile_key"]: profile for profile in operational_seed.get("profiles", [])}
            profile_ids: dict[str, int] = {}
            pending = list(profiles_by_key.values())
            while pending:
                progress = False
                for profile in pending[:]:
                    parent_key = profile.get("parent_profile")
                    if parent_key and parent_key not in profile_ids:
                        if parent_key not in profiles_by_key:
                            raise ValueError(f"unknown parent profile: {parent_key}")
                        continue
                    parent = profiles_by_key.get(parent_key) if parent_key else None
                    profile_errors, validation_report = validate_operational_profile(dictionary, profile, parent)
                    if profile_errors:
                        raise ValueError(f"invalid operational profile {profile['profile_key']}: " + "; ".join(profile_errors))
                    cur = db.execute(
                        "INSERT INTO operational_profiles(profile_key,parent_id,scope_type,scope_ref,title,revision,lifecycle_state,created_at,updated_at,notes) VALUES(?,?,?,?,?,?,?,?,?,?)",
                        (profile["profile_key"], profile_ids.get(parent_key), profile["scope_type"], profile["scope_ref"], profile["title"], profile["revision"], profile["lifecycle_state"], profile["created_at"], profile["updated_at"], profile.get("notes", "")),
                    )
                    profile_id = int(cur.lastrowid)
                    profile_ids[profile["profile_key"]] = profile_id
                    materialized, _ = materialize_operational_profile(dictionary, profile)
                    for key, entry in materialized.items():
                        source_key = entry.get("source")
                        if source_key and source_key not in source_ids:
                            raise ValueError(f"{profile['profile_key']}:{key}: unknown source key {source_key}")
                        db.execute(
                            "INSERT INTO operational_parameter_values(profile_id,parameter_key,disposition,value_json,unit,approval_status,source_id,source_document,source_revision,source_page,applicability,condition_json,uncertainty_json,rationale,reviewed_by,reviewed_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                            (profile_id, key, entry["disposition"], json.dumps(entry.get("value"), ensure_ascii=False, sort_keys=True) if entry["disposition"] == "VALUE" else None, entry.get("unit"), entry.get("approval_status", profile.get("default_approval_status", "INDICATIVE")), source_ids.get(source_key), entry.get("source_document"), entry.get("source_revision"), entry.get("source_page"), entry.get("applicability", ""), json.dumps(entry.get("condition"), ensure_ascii=False, sort_keys=True) if "condition" in entry else None, json.dumps(entry.get("uncertainty"), ensure_ascii=False, sort_keys=True) if "uncertainty" in entry else None, entry.get("rationale", ""), entry.get("reviewed_by"), entry.get("reviewed_at")),
                        )
                    db.execute(
                        "INSERT INTO operational_validation_runs(profile_id,run_at,validator_version,result,required_total,required_resolved,conditional_unresolved,safety_blockers,report_json) VALUES(?,?,?,?,?,?,?,?,?)",
                        (profile_id, profile["updated_at"], f"catalog-tool/{dictionary['version']}", validation_report["result"], validation_report["required_total"], validation_report["required_resolved"], validation_report["conditional_unresolved"], validation_report["safety_blockers"], json.dumps(validation_report, ensure_ascii=False, sort_keys=True)),
                    )
                    pending.remove(profile)
                    progress = True
                if not progress:
                    raise ValueError("operational profile inheritance cycle")
        integrity = db.execute("PRAGMA integrity_check").fetchone()[0]
        if integrity != "ok":
            raise RuntimeError(f"catalog integrity check failed: {integrity}")
        db.commit()
        build_ok = True
    finally:
        db.close()
        if not build_ok and staging_output.exists():
            staging_output.unlink()
    staging_output.replace(output)
    return imported


def profile_sha256(profile: dict) -> str:
    payload = json.dumps(profile, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def write_catalog_index(database: Path) -> Path:
    index_path = database.with_name("efis-catalog-index.json")
    db = sqlite3.connect(database)
    db.row_factory = sqlite3.Row
    try:
        version = db.execute("SELECT value FROM catalog_meta WHERE key='catalog_version'").fetchone()[0]
        aircraft = []
        for row in db.execute("SELECT a.id,m.name AS manufacturer,a.model,a.variant,a.category,a.icao_type,a.seats,a.empty_weight_kg,a.mtow_kg,a.fuel_capacity_l,a.status,em.name AS engine_manufacturer,e.model AS engine_model FROM aircraft a JOIN manufacturers m ON m.id=a.manufacturer_id LEFT JOIN engines e ON e.id=a.default_engine_id LEFT JOIN manufacturers em ON em.id=e.manufacturer_id ORDER BY m.name,a.model,a.variant"):
            item = dict(row)
            item["speeds_kt"] = {speed["code"]: speed["value_kt"] for speed in db.execute("SELECT code,value_kt FROM aircraft_speeds WHERE aircraft_id=? ORDER BY code", (item["id"],))}
            aircraft.append(item)
        engines = [dict(row) for row in db.execute("SELECT e.id,m.name AS manufacturer,e.family,e.model,e.variant,e.cylinders,e.power_hp,e.rpm_max,e.fuel_type,e.status FROM engines e JOIN manufacturers m ON m.id=e.manufacturer_id ORDER BY m.name,e.model,e.variant")]
        operational = []
        operational_domains = []
        operational_parameters = []
        tables = {row[0] for row in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        if "operational_profiles" in tables:
            operational_domains = [dict(row) for row in db.execute(
                "SELECT d.id,d.label,d.sort_order,count(p.key) AS parameter_count FROM operational_domains d LEFT JOIN operational_parameter_definitions p ON p.domain=d.id GROUP BY d.id,d.label,d.sort_order ORDER BY d.sort_order"
            )]
            operational_parameters = [dict(row) for row in db.execute(
                "SELECT key,domain,label,description,value_type,canonical_unit,requirement,criticality,enum_values_json,applicability_text,sort_order FROM operational_parameter_definitions WHERE active=1 ORDER BY sort_order,key"
            )]
            for definition in operational_parameters:
                enum_values_json = definition.pop("enum_values_json")
                definition["enum_values"] = json.loads(enum_values_json) if enum_values_json else []
            profile_rows = list(db.execute(
                "SELECT p.id,p.parent_id,p.profile_key,p.scope_type,p.scope_ref,p.title,p.revision,p.lifecycle_state,parent.profile_key AS parent_profile FROM operational_profiles p LEFT JOIN operational_profiles parent ON parent.id=p.parent_id ORDER BY p.profile_key"
            ))
            values_by_profile: dict[int, dict[str, dict]] = {}
            for profile_row in profile_rows:
                profile_id = int(profile_row["id"])
                value_rows: dict[str, dict] = {}
                for value_row in db.execute(
                    "SELECT v.parameter_key,v.disposition,v.value_json,v.unit,v.approval_status,s.title AS source_title,v.source_document,v.source_revision,v.source_page,v.applicability,v.condition_json,v.uncertainty_json,v.rationale,v.reviewed_by,v.reviewed_at FROM operational_parameter_values v LEFT JOIN sources s ON s.id=v.source_id WHERE v.profile_id=? ORDER BY v.parameter_key",
                    (profile_id,),
                ):
                    value = dict(value_row)
                    for name in ("value_json", "condition_json", "uncertainty_json"):
                        if value[name] is not None:
                            value[name[:-5]] = json.loads(value[name])
                        del value[name]
                    value_rows[value["parameter_key"]] = value
                values_by_profile[profile_id] = value_rows

            profiles_by_id = {int(row["id"]): row for row in profile_rows}

            def effective_value(profile_id: int, parameter_key: str) -> tuple[dict, int]:
                visited: set[int] = set()
                current_id = profile_id
                while current_id and current_id not in visited:
                    visited.add(current_id)
                    value = values_by_profile[current_id][parameter_key]
                    if value["disposition"] != "INHERIT":
                        return value, current_id
                    current_id = int(profiles_by_id[current_id]["parent_id"] or 0)
                return {"disposition": "UNKNOWN", "rationale": "Unresolved inheritance"}, profile_id

            for row in profile_rows:
                profile_id = int(row["id"])
                item = {key: row[key] for key in row.keys() if key not in {"id", "parent_id"}}
                counts = db.execute("SELECT disposition,count(*) AS count FROM operational_parameter_values WHERE profile_id=? GROUP BY disposition", (profile_id,)).fetchall()
                item["parameter_counts"] = {count["disposition"]: count["count"] for count in counts}
                validation = db.execute("SELECT result,required_total,required_resolved,conditional_unresolved,safety_blockers,report_json FROM operational_validation_runs WHERE profile_id=? ORDER BY id DESC LIMIT 1", (profile_id,)).fetchone()
                if validation:
                    item["validation"] = {key: validation[key] for key in ("result", "required_total", "required_resolved", "conditional_unresolved", "safety_blockers")}
                    item["validation"]["release_ready"] = bool(json.loads(validation["report_json"])["release_ready"])
                parameters = []
                for definition in operational_parameters:
                    key = definition["key"]
                    raw = dict(values_by_profile[profile_id][key])
                    effective, origin_id = effective_value(profile_id, key)
                    raw["effective_disposition"] = effective["disposition"]
                    if "value" in effective:
                        raw["effective_value"] = effective["value"]
                    raw["effective_approval_status"] = effective.get("approval_status", "INDICATIVE")
                    raw["origin_profile"] = profiles_by_id[origin_id]["profile_key"]
                    parameters.append(raw)
                item["parameters"] = parameters
                operational.append(item)
    finally:
        db.close()
    index_path.write_text(json.dumps({"schema_version": 2 if operational else 1, "catalog_version": version, "aircraft": aircraft, "engines": engines, "operational_domains": operational_domains, "operational_parameters": operational_parameters, "operational_profiles": operational}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return index_path


def main() -> int:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser("build")
    build.add_argument("--schema", type=Path, required=True)
    build.add_argument("--seed", type=Path, required=True)
    build.add_argument("--output", type=Path, required=True)
    build.add_argument("--legacy", type=Path)
    build.add_argument("--operational-schema", type=Path)
    build.add_argument("--parameter-dictionary", type=Path)
    build.add_argument("--operational-seed", type=Path)
    validate = commands.add_parser("validate-profile")
    validate.add_argument("profile", type=Path)
    validate_operational = commands.add_parser("validate-operational-profile")
    validate_operational.add_argument("profile", type=Path)
    validate_operational.add_argument("--dictionary", type=Path, required=True)
    validate_operational.add_argument("--parent", type=Path)
    dictionary_check = commands.add_parser("validate-parameter-dictionary")
    dictionary_check.add_argument("dictionary", type=Path)
    seed_check = commands.add_parser("validate-operational-seed")
    seed_check.add_argument("seed", type=Path)
    seed_check.add_argument("--dictionary", type=Path, required=True)
    render_register = commands.add_parser("render-parameter-register")
    render_register.add_argument("dictionary", type=Path)
    render_register.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "build":
        report = build_catalog(args.schema, args.seed, args.output, args.legacy, args.operational_schema, args.parameter_dictionary, args.operational_seed)
        index_path = write_catalog_index(args.output)
        print(f"catalog built: {args.output}")
        print(f"catalog index: {index_path}")
        print("import report: " + json.dumps(report, sort_keys=True))
        return 0
    if args.command == "validate-parameter-dictionary":
        definitions, errors = flatten_parameter_dictionary(read_json(args.dictionary))
        if errors:
            for error in errors:
                print(f"ERROR: {error}")
            return 1
        print(f"operational dictionary valid: {len(definitions)} parameters")
        return 0
    if args.command == "validate-operational-profile":
        profile = read_json(args.profile)
        parent = read_json(args.parent) if args.parent else None
        errors, report = validate_operational_profile(read_json(args.dictionary), profile, parent)
        print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
        return 1 if errors else 0
    if args.command == "validate-operational-seed":
        errors, reports = validate_operational_seed(read_json(args.dictionary), read_json(args.seed))
        result = {"result": "PASS" if not errors else "FAIL", "profiles": reports, "errors": errors}
        print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))
        return 1 if errors else 0
    if args.command == "render-parameter-register":
        count = write_parameter_register(read_json(args.dictionary), args.output)
        print(f"operational parameter register written: {args.output} ({count} parameters)")
        return 0
    profile = read_json(args.profile)
    errors = validate_profile(profile)
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print(f"profile valid: {profile_sha256(profile)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
