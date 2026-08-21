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


def read_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


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


def build_catalog(schema: Path, seed_path: Path, output: Path, legacy_path: Path | None = None) -> dict[str, int]:
    seed = read_json(seed_path)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        output.unlink()
    db = sqlite3.connect(output)
    try:
        db.executescript(schema.read_text(encoding="utf-8"))
        db.execute("INSERT INTO catalog_meta(key,value) VALUES('schema_version','1')")
        db.execute("INSERT INTO catalog_meta(key,value) VALUES('catalog_version',?)", (seed["catalog_version"],))
        source_ids: dict[str, int] = {}
        for source in seed["sources"]:
            cur = db.execute("INSERT INTO sources(title,publisher,document_ref,source_class) VALUES(?,?,?,?)",
                             (source["title"], source.get("publisher"), source.get("document_ref"), source["source_class"]))
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
        integrity = db.execute("PRAGMA integrity_check").fetchone()[0]
        if integrity != "ok":
            raise RuntimeError(f"catalog integrity check failed: {integrity}")
        db.commit()
    finally:
        db.close()
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
    finally:
        db.close()
    index_path.write_text(json.dumps({"schema_version": 1, "catalog_version": version, "aircraft": aircraft, "engines": engines}, ensure_ascii=False, indent=2), encoding="utf-8")
    return index_path


def main() -> int:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser("build")
    build.add_argument("--schema", type=Path, required=True)
    build.add_argument("--seed", type=Path, required=True)
    build.add_argument("--output", type=Path, required=True)
    build.add_argument("--legacy", type=Path)
    validate = commands.add_parser("validate-profile")
    validate.add_argument("profile", type=Path)
    args = parser.parse_args()
    if args.command == "build":
        report = build_catalog(args.schema, args.seed, args.output, args.legacy)
        index_path = write_catalog_index(args.output)
        print(f"catalog built: {args.output}")
        print(f"catalog index: {index_path}")
        print("import report: " + json.dumps(report, sort_keys=True))
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
