#!/usr/bin/env python3
"""Validate the HMI hardware-acceptance plan and evidence fail-closed."""

from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re


PLAN_KEYS = {"schema", "status", "authority", "duration_minutes", "targets", "scenarios", "lighting_modes", "acceptance", "evidence"}
TARGET_KEYS = {"role", "platform", "resolution", "transport", "port"}
ACCEPTANCE_KEYS = {"minimum_touch_px", "minimum_fps", "maximum_p95_frame_time_ms", "required_throttled_value", "temperature_recording_required", "cold_boot_required", "reboot_required", "transport_replay_required", "ethernet_fault_recovery_required", "operator_visual_signoff_required", "operator_touch_signoff_required"}
EVIDENCE_KEYS = {"manifest_schema", "required_artifact_kinds", "digest", "node_identity_required", "utc_timestamp_required", "pass_requires_all_targets"}
MANIFEST_KEYS = {"schema", "status", "hardware_verified", "generated_utc", "operator", "runs"}
RUN_KEYS = {"role", "node", "result", "measurements", "artifacts"}
ARTIFACT_KEYS = {"kind", "file", "sha256", "generated_utc"}
MEASUREMENT_KEYS = {"duration_minutes", "minimum_fps", "p95_frame_time_ms", "maximum_temperature_c", "throttled", "cold_boot_pass", "reboot_pass", "scenarios_passed", "lighting_modes_passed", "minimum_touch_px", "transport_replay_pass", "ethernet_fault_recovery_pass"}
DIGEST = re.compile(r"[0-9a-f]{64}")


def _error(errors: list[str], control: int, message: str) -> None:
    errors.append(f"[HMI-A20-{control:02d}] {message}")


def _load(path: Path, control: int, errors: list[str]) -> dict | None:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        _error(errors, control, f"invalid or missing JSON {path}: {exc}")
        return None
    if not isinstance(data, dict):
        _error(errors, control, "JSON root must be an object")
        return None
    return data


def _utc(value: object) -> bool:
    try:
        parsed = datetime.fromisoformat(str(value).replace("Z", "+00:00"))
        return parsed.utcoffset() is not None and parsed.utcoffset().total_seconds() == 0
    except (TypeError, ValueError):
        return False


def validate(root: Path, evidence_path: Path | None = None) -> list[str]:
    errors: list[str] = []
    plan = _load(root / "config/ui/avionix-hmi-hardware-acceptance-v1.json", 1, errors)
    if plan is None:
        return errors
    if plan.get("schema") != "avionix.hmi-hardware-acceptance.v1":
        _error(errors, 1, "plan schema is invalid")
    if set(plan) != PLAN_KEYS:
        _error(errors, 2, "plan properties differ from the closed schema")
    if plan.get("status") != "PREPARED_SW_NOT_RUN" or "NOT_RUN" not in str(plan.get("status")):
        _error(errors, 3, "plan must remain prepared software, not hardware acceptance")
    if plan.get("authority") != "READ_ONLY_DISPLAY_ACCEPTANCE_NO_CONTROL_AUTHORITY":
        _error(errors, 4, "acceptance must have no control authority")
    targets = plan.get("targets")
    expected_targets = [
        {"role": "PFD", "platform": "RPi5", "resolution": [1024, 600], "transport": "DDP", "port": 49002},
        {"role": "MFD", "platform": "RPi5", "resolution": [1024, 600], "transport": "DDP", "port": 49002},
        {"role": "EMS", "platform": "RPi4", "resolution": [1024, 600], "transport": "EDP", "port": 49003},
    ]
    if targets != expected_targets or any(set(item) != TARGET_KEYS for item in targets or []):
        _error(errors, 5, "PFD/MFD/EMS target mapping is invalid")
    if plan.get("scenarios") != ["LIVE", "STALE", "DEGRADED", "LOST", "FAIL", "SIM"]:
        _error(errors, 6, "required data-state scenarios are incomplete")
    if plan.get("lighting_modes") != ["DAY", "NIGHT"]:
        _error(errors, 7, "day/night visual assessment is incomplete")
    acceptance = plan.get("acceptance")
    if not isinstance(acceptance, dict) or set(acceptance) != ACCEPTANCE_KEYS:
        _error(errors, 8, "acceptance properties differ from the closed schema")
        acceptance = {}
    if acceptance.get("minimum_touch_px") != 44:
        _error(errors, 9, "touch target floor must be 44 px")
    if acceptance.get("minimum_fps") != 40 or acceptance.get("maximum_p95_frame_time_ms") != 25.0:
        _error(errors, 10, "frame-rate and p95 frame-time limits are invalid")
    if acceptance.get("required_throttled_value") != "0x0" or acceptance.get("temperature_recording_required") is not True:
        _error(errors, 11, "thermal/throttling evidence requirements are invalid")
    if acceptance.get("cold_boot_required") is not True or acceptance.get("reboot_required") is not True or plan.get("duration_minutes") != 60:
        _error(errors, 12, "60-minute boot/reboot acceptance is incomplete")
    if acceptance.get("transport_replay_required") is not True:
        _error(errors, 13, "DDP/EDP replay evidence is required")
    if acceptance.get("ethernet_fault_recovery_required") is not True:
        _error(errors, 14, "Ethernet fault and recovery evidence is required")
    if acceptance.get("operator_visual_signoff_required") is not True:
        _error(errors, 15, "operator sunlight/night visual signoff is required")
    if acceptance.get("operator_touch_signoff_required") is not True:
        _error(errors, 16, "operator touch signoff is required")
    declaration = plan.get("evidence")
    kinds = ["SCREENSHOT", "FRAME_TIMING", "THERMAL", "TOUCH_LOG", "BOOT_LOG", "TRANSPORT_REPLAY", "FAULT_RECOVERY", "OPERATOR_SIGNOFF"]
    expected_declaration = {"manifest_schema": "avionix.hmi-hardware-evidence.v1", "required_artifact_kinds": kinds, "digest": "SHA-256", "node_identity_required": True, "utc_timestamp_required": True, "pass_requires_all_targets": True}
    if not isinstance(declaration, dict) or set(declaration) != EVIDENCE_KEYS or declaration != expected_declaration:
        _error(errors, 17, "evidence provenance and integrity declaration is invalid")
    manifest_path = evidence_path or root / "config/ui/avionix-hmi-hardware-evidence-not-run-v1.json"
    manifest = _load(manifest_path, 18, errors)
    if manifest is None:
        return errors
    if set(manifest) != MANIFEST_KEYS or manifest.get("schema") != "avionix.hmi-hardware-evidence.v1":
        _error(errors, 18, "evidence manifest schema is invalid")
        return errors
    runs = manifest.get("runs")
    if not isinstance(runs, list) or [run.get("role") for run in runs if isinstance(run, dict)] != ["PFD", "MFD", "EMS"] or any(set(run) != RUN_KEYS for run in runs if isinstance(run, dict)):
        _error(errors, 19, "evidence must contain exactly PFD, MFD and EMS runs")
        return errors
    status = manifest.get("status")
    if status == "NOT_RUN":
        if manifest.get("hardware_verified") is not False or manifest.get("generated_utc") is not None or manifest.get("operator") is not None or any(run.get("result") != "NOT_RUN" or run.get("node") is not None or run.get("measurements") != {} or run.get("artifacts") != [] for run in runs):
            _error(errors, 20, "NOT_RUN template contains a hardware claim or evidence")
        return errors
    if status != "PASS" or manifest.get("hardware_verified") is not True or not _utc(manifest.get("generated_utc")) or not isinstance(manifest.get("operator"), str) or not manifest["operator"].strip():
        _error(errors, 20, "hardware PASS lacks signed UTC provenance")
        return errors
    base = manifest_path.parent.resolve()
    for run in runs:
        if run.get("result") != "PASS" or not isinstance(run.get("node"), str) or not run["node"].strip():
            _error(errors, 20, f"{run.get('role')} does not have a passing identified node")
            continue
        measured = run.get("measurements")
        if not isinstance(measured, dict) or set(measured) != MEASUREMENT_KEYS:
            _error(errors, 20, f"{run['role']} measurement set is incomplete")
            continue
        if (not isinstance(measured["duration_minutes"], (int, float)) or measured["duration_minutes"] < 60 or
                not isinstance(measured["minimum_fps"], (int, float)) or measured["minimum_fps"] < 40 or
                not isinstance(measured["p95_frame_time_ms"], (int, float)) or measured["p95_frame_time_ms"] > 25.0 or
                not isinstance(measured["maximum_temperature_c"], (int, float)) or
                measured["throttled"] != "0x0" or
                not isinstance(measured["minimum_touch_px"], (int, float)) or measured["minimum_touch_px"] < 44 or
                measured["scenarios_passed"] != ["LIVE", "STALE", "DEGRADED", "LOST", "FAIL", "SIM"] or
                measured["lighting_modes_passed"] != ["DAY", "NIGHT"] or
                any(measured[key] is not True for key in ("cold_boot_pass", "reboot_pass", "transport_replay_pass", "ethernet_fault_recovery_pass"))):
            _error(errors, 20, f"{run['role']} measurements do not meet the acceptance limits")
            continue
        artifacts = run.get("artifacts")
        if not isinstance(artifacts, list) or {item.get("kind") for item in artifacts if isinstance(item, dict)} != set(kinds):
            _error(errors, 20, f"{run['role']} artifact set is incomplete")
            continue
        for item in artifacts:
            if set(item) != ARTIFACT_KEYS or not _utc(item.get("generated_utc")) or not DIGEST.fullmatch(str(item.get("sha256"))):
                _error(errors, 20, f"{run['role']} artifact metadata is invalid")
                break
            name = item.get("file")
            path = (base / str(name)).resolve()
            if not isinstance(name, str) or Path(name).is_absolute() or base not in path.parents or not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != item["sha256"]:
                _error(errors, 20, f"{run['role']} artifact is unsafe, missing or has a digest mismatch")
                break
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--evidence", type=Path)
    args = parser.parse_args()
    errors = validate(args.root, args.evidence)
    if errors:
        print("AVIONIX HMI hardware acceptance FAILED")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("AVIONIX HMI acceptance PREPARED-SW: hardware NOT_RUN; 20 controls PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
