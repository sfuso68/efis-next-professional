#!/usr/bin/env python3
"""Assemble an immutable HMI hardware evidence manifest from operator results."""

from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile

try:
    from .validate_hmi_hardware_acceptance import validate as validate_manifest
except ImportError:
    from validate_hmi_hardware_acceptance import validate as validate_manifest


ROOT_KEYS = {"schema", "status", "operator_confirmed", "operator", "captured_utc", "runs"}
RUN_KEYS = {"role", "node", "release", "measurements", "artifacts"}
MEASUREMENT_KEYS = {"duration_minutes", "minimum_fps", "p95_frame_time_ms", "maximum_temperature_c", "throttled", "cold_boot_pass", "reboot_pass", "scenarios_passed", "lighting_modes_passed", "minimum_touch_px", "transport_replay_pass", "ethernet_fault_recovery_pass"}
KINDS = ["SCREENSHOT", "FRAME_TIMING", "THERMAL", "TOUCH_LOG", "BOOT_LOG", "TRANSPORT_REPLAY", "FAULT_RECOVERY", "OPERATOR_SIGNOFF"]
ROLES = ["PFD", "MFD", "EMS"]
DIGEST = re.compile(r"[0-9a-f]{64}")
RELEASE = re.compile(r"(?:[0-9a-f]{40}|[0-9a-f]{64})")


def _error(errors: list[str], control: int, message: str) -> None:
    errors.append(f"[HMI-P20-{control:02d}] {message}")


def _utc(value: object) -> bool:
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        return parsed.utcoffset() is not None and parsed.utcoffset().total_seconds() == 0
    except (TypeError, ValueError):
        return False


def load_results(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError("results root must be an object")
    return value


def validate_results(data: dict, bundle_dir: Path, allow_not_run: bool = False) -> list[str]:
    errors: list[str] = []
    if data.get("schema") != "avionix.hmi-hardware-results.v1":
        _error(errors, 1, "results schema is invalid")
    if set(data) != ROOT_KEYS:
        _error(errors, 2, "results root properties differ from the closed schema")
    if allow_not_run and data.get("status") == "NOT_RUN":
        if data.get("operator_confirmed") is not False:
            _error(errors, 3, "NOT_RUN must not be operator confirmed")
        return errors
    if data.get("status") != "READY_FOR_ASSEMBLY" or data.get("operator_confirmed") is not True:
        _error(errors, 3, "results are not explicitly ready and operator confirmed")
    if not isinstance(data.get("operator"), str) or not data["operator"].strip() or not _utc(data.get("captured_utc")):
        _error(errors, 4, "operator or UTC capture timestamp is invalid")
    runs = data.get("runs")
    if not isinstance(runs, list) or [item.get("role") for item in runs if isinstance(item, dict)] != ROLES:
        _error(errors, 5, "runs must be exactly PFD, MFD and EMS in order")
        return errors
    if any(not isinstance(item, dict) or set(item) != RUN_KEYS for item in runs):
        _error(errors, 6, "run properties differ from the closed schema")
        return errors
    base = bundle_dir.resolve()
    for run in runs:
        role = run["role"]
        if not isinstance(run.get("node"), str) or not run["node"].strip():
            _error(errors, 7, f"{role} node identity is missing")
        if not isinstance(run.get("release"), str) or not RELEASE.fullmatch(run["release"]):
            _error(errors, 8, f"{role} release must be a full Git object ID")
        measured = run.get("measurements")
        if not isinstance(measured, dict) or set(measured) != MEASUREMENT_KEYS:
            _error(errors, 9, f"{role} measurement properties differ from the closed schema")
            continue
        if not isinstance(measured["duration_minutes"], (int, float)) or measured["duration_minutes"] < 60:
            _error(errors, 10, f"{role} duration is below 60 minutes")
        if not isinstance(measured["minimum_fps"], (int, float)) or measured["minimum_fps"] < 40:
            _error(errors, 11, f"{role} minimum FPS is below 40")
        if not isinstance(measured["p95_frame_time_ms"], (int, float)) or measured["p95_frame_time_ms"] > 25:
            _error(errors, 12, f"{role} p95 frame time exceeds 25 ms")
        if not isinstance(measured["maximum_temperature_c"], (int, float)) or isinstance(measured["maximum_temperature_c"], bool):
            _error(errors, 13, f"{role} maximum temperature was not measured")
        if measured["throttled"] != "0x0":
            _error(errors, 14, f"{role} throttling/undervoltage history is not zero")
        if not isinstance(measured["minimum_touch_px"], (int, float)) or measured["minimum_touch_px"] < 44:
            _error(errors, 15, f"{role} touch target is below 44 px")
        if measured["scenarios_passed"] != ["LIVE", "STALE", "DEGRADED", "LOST", "FAIL", "SIM"]:
            _error(errors, 16, f"{role} state coverage is incomplete")
        if measured["lighting_modes_passed"] != ["DAY", "NIGHT"]:
            _error(errors, 17, f"{role} lighting coverage is incomplete")
        if measured["cold_boot_pass"] is not True or measured["reboot_pass"] is not True:
            _error(errors, 18, f"{role} boot/reboot evidence is incomplete")
        if measured["transport_replay_pass"] is not True or measured["ethernet_fault_recovery_pass"] is not True:
            _error(errors, 19, f"{role} replay or fault recovery is incomplete")
        artifacts = run.get("artifacts")
        if not isinstance(artifacts, dict) or set(artifacts) != set(KINDS):
            _error(errors, 20, f"{role} artifact map is incomplete")
            continue
        for kind, name in artifacts.items():
            if not isinstance(name, str) or Path(name).is_absolute():
                _error(errors, 20, f"{role}/{kind} artifact path is unsafe")
                break
            candidate = base / name
            path = candidate.resolve()
            if base not in path.parents or candidate.is_symlink() or not path.is_file() or path.stat().st_size == 0:
                _error(errors, 20, f"{role}/{kind} artifact is missing, empty or unsafe")
                break
    return errors


def assemble(root: Path, results_path: Path, output: Path) -> list[str]:
    data = load_results(results_path)
    bundle_dir = results_path.resolve().parent
    errors = validate_results(data, bundle_dir)
    if output.exists():
        _error(errors, 20, "output manifest already exists; evidence is immutable")
    if errors:
        return errors
    stamp = data["captured_utc"]
    runs = []
    for source in data["runs"]:
        artifacts = []
        for kind, name in source["artifacts"].items():
            path = bundle_dir / name
            artifacts.append({"kind": kind, "file": os.path.relpath(path, output.parent).replace("\\", "/"), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "generated_utc": stamp})
        runs.append({"role": source["role"], "node": source["node"], "release": source["release"], "result": "PASS", "measurements": source["measurements"], "artifacts": artifacts})
    manifest = {"schema": "avionix.hmi-hardware-evidence.v1", "status": "PASS", "hardware_verified": True, "flight_approval": False, "generated_utc": stamp, "operator": data["operator"], "runs": runs}
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=output.parent, delete=False) as handle:
        temporary = Path(handle.name)
        json.dump(manifest, handle, indent=2)
        handle.write("\n")
    temporary.replace(output)
    downstream = validate_manifest(root, output)
    if downstream:
        output.unlink(missing_ok=True)
        return [f"[HMI-P20-20] downstream validation failed: {item}" for item in downstream]
    return []


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--results", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        errors = assemble(args.root, args.results, args.output)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        errors = [f"[HMI-P20-01] unable to read results: {exc}"]
    if errors:
        print("AVIONIX HMI evidence assembly FAILED")
        for error in errors:
            print(f"  - {error}")
        return 1
    print(f"AVIONIX HMI evidence bundle PASS: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
