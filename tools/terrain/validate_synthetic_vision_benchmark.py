#!/usr/bin/env python3
"""Fail-closed validator for the AVIONIX synthetic-vision benchmark."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CONTRACT = ROOT / "config/terrain/avionix-synthetic-vision-benchmark-v1.json"
HEX40 = re.compile(r"^[0-9a-f]{40}$")
HEX64 = re.compile(r"^[0-9a-f]{64}$")
CHECKPOINT_IDS = [
    "LIPH_DEPARTURE", "VENETO_PLAIN", "PREALPS_ENTRY", "LIDB_APPROACH", "LIDB_FINAL"
]
CHECKPOINTS = [
    {"id": "LIPH_DEPARTURE", "latitude_deg": 45.648399, "longitude_deg": 12.1944,
     "altitude_ft": 1200, "heading_deg": 4.2},
    {"id": "VENETO_PLAIN", "latitude_deg": 45.82, "longitude_deg": 12.213,
     "altitude_ft": 3000, "heading_deg": 4.2},
    {"id": "PREALPS_ENTRY", "latitude_deg": 46.0, "longitude_deg": 12.232,
     "altitude_ft": 5000, "heading_deg": 4.2},
    {"id": "LIDB_APPROACH", "latitude_deg": 46.13, "longitude_deg": 12.246,
     "altitude_ft": 3500, "heading_deg": 4.2},
    {"id": "LIDB_FINAL", "latitude_deg": 46.166549, "longitude_deg": 12.250389,
     "altitude_ft": 1800, "heading_deg": 4.2},
]
ARTIFACT_TYPES = {
    "FRAME_TIMING", "THERMAL", "TILE_TRANSITION", "MISSING_TILE_FAULT",
    "CHECKPOINT_SCREENSHOTS", "OPERATOR_SIGNOFF",
}


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def load_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def validate_contract(data: object, root: Path = ROOT) -> list[str]:
    errors: list[str] = []
    expected_keys = {
        "schema", "benchmark_id", "status", "flight_approval", "scenario", "platform",
        "roles", "required_tiles", "checkpoints", "run_requirements", "acceptance",
        "required_artifacts", "data_policy",
    }
    if not isinstance(data, dict):
        return ["contract root must be an object"]
    if set(data) != expected_keys:
        errors.append("contract keys differ from the closed V1 schema")
    if data.get("schema") != "avionix.synthetic-vision-benchmark.v1":
        errors.append("contract schema is not V1")
    if data.get("benchmark_id") != "SV-TREVISO-BELLUNO-DOLOMITES-V1":
        errors.append("benchmark identity changed")
    if data.get("status") != "PREPARED_SW_NOT_RUN" or data.get("flight_approval") is not False:
        errors.append("baseline must remain PREPARED_SW_NOT_RUN and not flight-approved")
    if data.get("platform") != "RPI5_AARCH64" or data.get("roles") != ["PFD", "MFD"]:
        errors.append("platform/roles must remain PFD and MFD on RPi5 AArch64")
    if data.get("required_tiles") != ["N45E012", "N46E012"]:
        errors.append("the frozen two-tile route changed")

    if data.get("checkpoints") != CHECKPOINTS:
        errors.append("five ordered visual checkpoints and their coordinates are frozen")

    run = data.get("run_requirements")
    expected_run = {
        "duration_seconds": 3600, "modes": ["DAY", "NIGHT"], "cold_cache": True,
        "warm_cache": True, "tile_boundary_crossing": "N45E012_TO_N46E012",
        "missing_tile_fault": True, "operator_signoff": True,
    }
    if run != expected_run:
        errors.append("run requirements differ from the 60-minute cold/warm/fault baseline")

    acceptance = data.get("acceptance")
    expected_acceptance = {
        "minimum_fps": 40.0, "maximum_p95_frame_ms": 25.0,
        "maximum_frame_ms": 100.0, "maximum_boundary_discontinuity_m": 1.0,
        "maximum_missing_samples": 0, "maximum_unlabelled_fallbacks": 0,
        "required_throttled": "0x0", "finite_projection_required": True,
        "near_field_visual_pass_required": True, "horizon_continuity_pass_required": True,
        "explicit_fallback_required": True,
    }
    if acceptance != expected_acceptance:
        errors.append("acceptance thresholds differ from the frozen V1 baseline")
    if set(data.get("required_artifacts", [])) != ARTIFACT_TYPES:
        errors.append("required artifact set is incomplete")
    if data.get("data_policy") != {
        "terrain_status": "DEVELOPMENT_NON_CERT", "vertical_datum": "EGM2008",
        "navigation_use": False, "obstacle_clearance_use": False,
    }:
        errors.append("terrain data policy must remain development-only")

    scenario_path = root / str(data.get("scenario", ""))
    try:
        scenario = load_json(scenario_path)
    except (OSError, json.JSONDecodeError):
        errors.append("scenario is missing or invalid")
    else:
        if not isinstance(scenario, dict) or scenario.get("id") != "LIPH-LIDB-DEVELOPMENT":
            errors.append("scenario identity does not match the benchmark")
        route = scenario.get("route", {}) if isinstance(scenario, dict) else {}
        if route.get("terrain_tiles") != data.get("required_tiles"):
            errors.append("scenario terrain tiles differ from the benchmark")
        if scenario.get("status") != "simulation-non-certified":
            errors.append("scenario must remain explicitly non-certified")
    return errors


def _artifact_path(root: Path, relative: object) -> Path | None:
    if not isinstance(relative, str) or not relative or Path(relative).is_absolute():
        return None
    candidate = root / relative
    resolved = candidate.resolve()
    try:
        resolved.relative_to(root.resolve())
    except ValueError:
        return None
    return candidate


def validate_results(data: object, contract: dict, artifact_root: Path,
                     contract_digest: str | None = None) -> list[str]:
    errors: list[str] = []
    keys = {
        "schema", "benchmark_id", "contract_sha256", "source_commit", "platform", "role",
        "release_id", "status", "flight_approval", "duration_seconds", "modes",
        "measurements", "checkpoints", "artifacts", "operator",
    }
    if not isinstance(data, dict):
        return ["results root must be an object"]
    if set(data) != keys:
        errors.append("results keys differ from the closed V1 schema")
    if data.get("schema") != "avionix.synthetic-vision-benchmark-results.v1":
        errors.append("results schema is not V1")
    if data.get("benchmark_id") != contract["benchmark_id"]:
        errors.append("results benchmark identity differs from contract")
    if data.get("contract_sha256") != (contract_digest or digest(CONTRACT)):
        errors.append("contract digest mismatch")
    if not isinstance(data.get("source_commit"), str) or not HEX40.fullmatch(data["source_commit"]):
        errors.append("source commit must be a full lowercase object id")
    if data.get("platform") != contract["platform"] or data.get("role") not in contract["roles"]:
        errors.append("result platform/role is outside the contract")
    if not isinstance(data.get("release_id"), str) or not data["release_id"].strip():
        errors.append("release identity is required")
    if data.get("status") != "PASS" or data.get("flight_approval") is not False:
        errors.append("final results must be PASS while flight_approval remains false")
    if not isinstance(data.get("duration_seconds"), (int, float)) or data["duration_seconds"] < 3600:
        errors.append("run duration is below 60 minutes")
    if data.get("modes") != ["DAY", "NIGHT"]:
        errors.append("DAY and NIGHT evidence is required in frozen order")

    m = data.get("measurements")
    measurement_keys = {
        "minimum_fps", "p95_frame_ms", "maximum_frame_ms", "maximum_temperature_c",
        "throttled", "boundary_discontinuity_m", "missing_samples", "unlabelled_fallbacks",
        "finite_projection", "near_field_visual_pass", "horizon_continuity_pass",
        "tile_boundary_crossed", "missing_tile_fallback_explicit",
    }
    if not isinstance(m, dict) or set(m) != measurement_keys:
        errors.append("measurement object is incomplete or open-ended")
    else:
        limits = contract["acceptance"]
        numeric_limits = (
            ("minimum_fps", lambda value: value >= limits["minimum_fps"]),
            ("p95_frame_ms", lambda value: value <= limits["maximum_p95_frame_ms"]),
            ("maximum_frame_ms", lambda value: value <= limits["maximum_frame_ms"]),
            ("boundary_discontinuity_m", lambda value: value <= limits["maximum_boundary_discontinuity_m"]),
            ("missing_samples", lambda value: value <= limits["maximum_missing_samples"]),
            ("unlabelled_fallbacks", lambda value: value <= limits["maximum_unlabelled_fallbacks"]),
        )
        for name, accepted in numeric_limits:
            value = m.get(name)
            if (not isinstance(value, (int, float)) or isinstance(value, bool) or
                    not math.isfinite(value) or not accepted(value)):
                errors.append(f"measurement {name} is outside its acceptance limit")
        temperature = m.get("maximum_temperature_c")
        if (not isinstance(temperature, (int, float)) or isinstance(temperature, bool) or
                not math.isfinite(temperature)):
            errors.append("maximum_temperature_c must be recorded numerically")
        if m.get("throttled") != "0x0":
            errors.append("throttling/undervoltage history is not clean")
        for name in ("finite_projection", "near_field_visual_pass", "horizon_continuity_pass",
                     "tile_boundary_crossed", "missing_tile_fallback_explicit"):
            if m.get(name) is not True:
                errors.append(f"required boolean measurement {name} did not pass")

    checkpoints = data.get("checkpoints")
    if not isinstance(checkpoints, list) or [item.get("id") for item in checkpoints if isinstance(item, dict)] != CHECKPOINT_IDS:
        errors.append("results do not cover all five ordered checkpoints")
    elif any(set(item) != {"id", "day_observed", "night_observed"} or
             item.get("day_observed") is not True or item.get("night_observed") is not True
             for item in checkpoints):
        errors.append("each checkpoint requires positive DAY and NIGHT observation")

    artifacts = data.get("artifacts")
    if not isinstance(artifacts, dict) or set(artifacts) != ARTIFACT_TYPES:
        errors.append("artifact map is incomplete")
    else:
        for kind, record in artifacts.items():
            if not isinstance(record, dict) or set(record) != {"path", "sha256"}:
                errors.append(f"artifact {kind} metadata is invalid")
                continue
            path = _artifact_path(artifact_root, record.get("path"))
            if path is None or not path.is_file() or path.is_symlink() or path.stat().st_size == 0:
                errors.append(f"artifact {kind} is missing, empty, symlinked or outside the root")
                continue
            if not isinstance(record.get("sha256"), str) or not HEX64.fullmatch(record["sha256"]) or digest(path) != record["sha256"]:
                errors.append(f"artifact {kind} digest mismatch")
    if not isinstance(data.get("operator"), str) or not data["operator"].strip():
        errors.append("named operator signoff is required")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--contract", type=Path, default=CONTRACT)
    parser.add_argument("--results", type=Path)
    parser.add_argument("--artifact-root", type=Path)
    args = parser.parse_args()
    try:
        contract = load_json(args.contract)
    except (OSError, json.JSONDecodeError) as exc:
        print(f"SYNTHETIC VISION BENCHMARK: FAIL: {exc}")
        return 1
    errors = validate_contract(contract)
    if args.results is not None:
        if args.artifact_root is None:
            errors.append("--artifact-root is required with --results")
        else:
            try:
                results = load_json(args.results)
            except (OSError, json.JSONDecodeError) as exc:
                errors.append(f"results are unreadable: {exc}")
            else:
                errors.extend(validate_results(
                    results, contract, args.artifact_root, digest(args.contract)))
    if errors:
        print("SYNTHETIC VISION BENCHMARK: FAIL")
        for error in errors:
            print(f"  - {error}")
        return 1
    state = "PASS" if args.results is not None else "PREPARED_SW_NOT_RUN"
    print(f"SYNTHETIC VISION BENCHMARK: {state}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
