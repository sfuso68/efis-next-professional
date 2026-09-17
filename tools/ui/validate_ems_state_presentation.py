#!/usr/bin/env python3
"""Validate the fail-closed EMS degraded-state presentation contract."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


ROOT_KEYS = {"schema", "status", "role", "platform", "resolution", "states", "presentation", "golden"}
STATE_KEYS = {"name", "age_ms", "severity", "show_age", "normal_green_allowed"}
STATE_NAMES = ["LIVE", "STALE", "DEGRADED", "LOST", "FAIL", "SIM"]
STATE_AGES = [0, 600, 600, 1500, 0, 0]
STATE_SEVERITIES = ["NORMAL", "CAUTION", "CAUTION", "WARNING", "WARNING", "SIMULATION"]
PRESENTATION_KEYS = {"normal_ranges_hidden_for", "last_value_policy", "cas_caution_states",
                     "cas_warning_states", "derived_functions_blocked_for",
                     "colour_is_not_only_cue", "sim_always_visible", "no_control_authority"}
GOLDEN_KEYS = {"schema", "directory", "filename_template", "platform", "interactive"}
MANIFEST_KEYS = {"schema", "status", "generated_utc", "platform", "role", "resolution", "images"}
IMAGE_KEYS = {"state", "age_ms", "file", "bytes", "sha256"}
DIGEST = re.compile(r"[0-9a-f]{64}")


def _error(errors: list[str], control: int, message: str) -> None:
    errors.append(f"[HMI-E20-{control:02d}] {message}")


def _png_size(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError("not a PNG")
    return struct.unpack(">II", header[16:24])


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate(data: object) -> list[str]:
    errors: list[str] = []
    if not isinstance(data, dict) or data.get("schema") != "avionix.ems-state-presentation.v1":
        _error(errors, 1, "root object or schema is invalid")
        if not isinstance(data, dict):
            return errors
    if set(data) != ROOT_KEYS or data.get("status") != "SOFTWARE_REFERENCE_NOT_HARDWARE_ACCEPTANCE":
        _error(errors, 2, "closed root properties or non-flight status differ")
    if data.get("role") != "EMS" or data.get("platform") != "RPi4":
        _error(errors, 3, "role/platform allocation differs from ADR-013")
    if data.get("resolution") != [800, 480]:
        _error(errors, 4, "compact acceptance resolution must be 800x480")

    states = data.get("states")
    if not isinstance(states, list) or [row.get("name") for row in states if isinstance(row, dict)] != STATE_NAMES:
        _error(errors, 5, "ordered state set differs from baseline")
        states = []
    if states and (not all(isinstance(row, dict) and set(row) == STATE_KEYS for row in states)):
        _error(errors, 6, "state properties differ from closed schema")
    if states and [row.get("age_ms") for row in states] != STATE_AGES:
        _error(errors, 7, "deterministic state ages differ")
    if states and [row.get("severity") for row in states] != STATE_SEVERITIES:
        _error(errors, 8, "state severity mapping differs")
    if states and [row.get("show_age") for row in states] != [False, True, True, True, False, False]:
        _error(errors, 9, "age visibility policy differs")
    if states and [row.get("normal_green_allowed") for row in states] != [True, False, False, False, False, True]:
        _error(errors, 10, "normal-green policy differs")

    presentation = data.get("presentation")
    if not isinstance(presentation, dict) or set(presentation) != PRESENTATION_KEYS:
        _error(errors, 11, "presentation properties differ from closed schema")
        presentation = {}
    if presentation.get("normal_ranges_hidden_for") != ["STALE", "DEGRADED", "LOST", "FAIL"]:
        _error(errors, 12, "normal ranges are not suppressed for every degraded state")
    if presentation.get("last_value_policy") != "MAY_RETAIN_WITH_EXPLICIT_INVALIDITY":
        _error(errors, 13, "last-value policy differs")
    if (presentation.get("cas_caution_states") != ["STALE", "DEGRADED"] or
            presentation.get("cas_warning_states") != ["LOST", "FAIL"]):
        _error(errors, 14, "CAS severity sets differ")
    if presentation.get("derived_functions_blocked_for") != ["LOST", "FAIL"]:
        _error(errors, 15, "derived-function inhibit set differs")
    if presentation.get("colour_is_not_only_cue") is not True:
        _error(errors, 16, "colour cannot be the only degraded-state cue")
    if presentation.get("sim_always_visible") is not True:
        _error(errors, 17, "SIM visibility invariant is disabled")
    if presentation.get("no_control_authority") is not True:
        _error(errors, 18, "HMI must remain read-only")

    golden = data.get("golden")
    if (not isinstance(golden, dict) or set(golden) != GOLDEN_KEYS or
            golden.get("schema") != "avionix.ems-state-golden.v1" or
            golden.get("platform") != "windows" or golden.get("interactive") is not False):
        _error(errors, 19, "golden fixture policy differs")
    if (not isinstance(golden, dict) or
            golden.get("directory") != "tests/golden/ems-states" or
            golden.get("filename_template") != "ems-800x480-{state}.png"):
        _error(errors, 20, "golden evidence location or naming differs")
    return errors


def validate_repository(root: Path, data: dict | None = None) -> list[str]:
    errors = validate(data if data is not None else json.loads(
        (root / "config/ui/avionix-ems-state-presentation-v1.json").read_text(encoding="utf-8")))
    snippets = {
        "qml/HmiDataStateBadge.qml": ['dataState === "DEGRADED"'],
        "qml/HmiArcGaugeV4.qml": ["normalRangesVisible: gauge.usable && !gauge.degraded"],
        "qml/HmiFuelStatus.qml": ["root.dataDegraded?root.caution"],
        "qml/HmiElectricalSynoptic.qml": ["root.dataDegraded?root.caution"],
        "qml/HmiAirframePanel.qml": ["root.dataDegraded?root.caution"],
        "qml/HmiCylinderMatrixV2.qml": ["root.dataDegraded?root.caution", '"CHT UNAVAILABLE"', '"EGT UNAVAILABLE"'],
        "qml/AdaptiveEmsView.qml": ['controller.dataSourceState||"LOST"', 'controller.dataSourceState; ageMs:root.dataAgeMs',
                                    "text:root.dataUsable?root.fuelEnduranceHours"],
    }
    missing = []
    for relative, required in snippets.items():
        text = (root / relative).read_text(encoding="utf-8")
        missing.extend(f"{relative}: {snippet}" for snippet in required if snippet not in text)
    manifest_path = root / "tests/golden/ems-states/manifest.json"
    if not manifest_path.is_file():
        missing.append("tests/golden/ems-states/manifest.json")
    if missing:
        _error(errors, 20, "repository evidence missing: " + "; ".join(missing))
        return errors

    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        _error(errors, 20, f"invalid state golden manifest: {error}")
        return errors
    images = manifest.get("images") if isinstance(manifest, dict) else None
    if (not isinstance(manifest, dict) or set(manifest) != MANIFEST_KEYS or
            manifest.get("schema") != "avionix.ems-state-golden.v1" or
            manifest.get("status") != "SOFTWARE_REFERENCE_NOT_HARDWARE_ACCEPTANCE" or
            manifest.get("role") != "ems" or manifest.get("resolution") != [800, 480] or
            not isinstance(images, list) or len(images) != len(STATE_NAMES)):
        _error(errors, 20, "state golden manifest structure differs")
        return errors
    for index, row in enumerate(images):
        state = STATE_NAMES[index]
        expected_name = f"ems-800x480-{state.lower()}.png"
        if (not isinstance(row, dict) or set(row) != IMAGE_KEYS or row.get("state") != state or
                row.get("age_ms") != STATE_AGES[index] or row.get("file") != expected_name or
                not isinstance(row.get("bytes"), int) or row["bytes"] < 10_000 or
                not isinstance(row.get("sha256"), str) or not DIGEST.fullmatch(row["sha256"])):
            _error(errors, 20, f"invalid state golden record: {state}")
            continue
        image = manifest_path.parent / expected_name
        try:
            if not image.is_file() or image.stat().st_size != row["bytes"] or _png_size(image) != (800, 480) or _sha256(image) != row["sha256"]:
                _error(errors, 20, f"state golden evidence mismatch: {state}")
        except (OSError, ValueError):
            _error(errors, 20, f"state golden evidence unreadable: {state}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    errors = validate_repository(args.root)
    if errors:
        print("AVIONIX EMS state presentation validation FAILED")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("AVIONIX EMS state presentation PASS: six states and 20 controls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
