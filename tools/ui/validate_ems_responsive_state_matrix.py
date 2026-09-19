#!/usr/bin/env python3
"""Fail-closed validation of the EMS state/resolution cross-product."""

from __future__ import annotations

import argparse
from datetime import datetime
import json
from pathlib import Path
import re

try:
    from .capture_ems_responsive_state_matrix import SIZES, STATES, filename
    from .capture_hmi_golden import png_pixel_sha256, png_size, sha256
except ImportError:
    from capture_ems_responsive_state_matrix import SIZES, STATES, filename
    from capture_hmi_golden import png_pixel_sha256, png_size, sha256


ROOT_KEYS = {"schema", "status", "platform", "generated_utc", "role", "fixture", "images"}
IMAGE_KEYS = {"state", "age_ms", "width", "height", "file", "bytes", "sha256", "pixel_sha256"}
CONFIG_KEYS = {"schema", "status", "role", "fixture", "resolutions", "states", "invariants", "golden"}
STATE_KEYS = {"name", "age_ms", "severity", "show_age"}
DIGEST = re.compile(r"[0-9a-f]{64}")


def _error(errors: list[str], control: int, message: str) -> None:
    errors.append(f"[HMI-M20-{control:02d}] {message}")


def _json(path: Path, maximum: int, control: int, errors: list[str]) -> dict | None:
    if not path.is_file() or path.stat().st_size > maximum:
        _error(errors, control, f"missing or oversized JSON: {path}")
        return None
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        _error(errors, control, f"invalid JSON: {error}")
        return None
    if not isinstance(value, dict):
        _error(errors, control, "JSON root must be an object")
        return None
    return value


def validate(root: Path) -> list[str]:
    errors: list[str] = []
    config_path = root / "config/ui/avionix-ems-responsive-state-matrix-v1.json"
    config = _json(config_path, 128 * 1024, 1, errors)
    if config is None:
        return errors
    if config.get("schema") != "avionix.ems-responsive-state-matrix.v1":
        _error(errors, 1, "configuration schema is invalid")
    if set(config) != CONFIG_KEYS:
        _error(errors, 2, "configuration properties differ from the closed schema")
    if config.get("status") != "SOFTWARE_REFERENCE_NOT_HARDWARE_ACCEPTANCE":
        _error(errors, 3, "configuration must not claim hardware acceptance")
    if config.get("role") != "ems" or config.get("fixture") != "HmiGoldenMain.qml":
        _error(errors, 4, "role or fixture is invalid")
    expected_sizes = [list(size) for size in SIZES]
    if config.get("resolutions") != expected_sizes:
        _error(errors, 5, "resolution matrix or order is invalid")
    states = config.get("states")
    if (not isinstance(states, list) or
            [item.get("name") for item in states if isinstance(item, dict)] != [s[0] for s in STATES]):
        _error(errors, 6, "state matrix or order is invalid")
        states = []
    if states and any(set(item) != STATE_KEYS for item in states if isinstance(item, dict)):
        _error(errors, 7, "state properties differ from the closed schema")
    expected_ages = dict(STATES)
    if states and any(item.get("age_ms") != expected_ages.get(item.get("name")) for item in states):
        _error(errors, 8, "state age is not deterministic")
    severities = {"LIVE": "NORMAL", "STALE": "CAUTION", "DEGRADED": "CAUTION",
                  "LOST": "WARNING", "FAIL": "WARNING", "SIM": "SIMULATION"}
    age_visibility = {"LIVE": False, "STALE": True, "DEGRADED": True,
                      "LOST": True, "FAIL": False, "SIM": False}
    if states and any(item.get("severity") != severities[item["name"]] or
                      item.get("show_age") is not age_visibility[item["name"]]
                      for item in states):
        _error(errors, 9, "state severity or age visibility is invalid")

    invariants = config.get("invariants")
    if not isinstance(invariants, dict) or invariants != {
        "captures": 18,
        "state_semantics_independent_of_resolution": True,
        "normal_green_forbidden_for": ["STALE", "DEGRADED", "LOST", "FAIL"],
        "derived_functions_blocked_for": ["LOST", "FAIL"],
        "compact_resolution": [800, 480],
        "expanded_resolutions": [[1024, 600], [1280, 800]],
        "colour_is_not_only_cue": True,
        "no_control_authority": True,
    }:
        _error(errors, 10, "responsive invariants are incomplete or open-ended")

    golden = config.get("golden")
    if not isinstance(golden, dict) or golden != {
        "schema": "avionix.ems-responsive-state-golden.v1",
        "directory": "tests/golden/ems-responsive-states",
        "filename_template": "ems-{width}x{height}-{state}.png",
        "platform": "windows",
        "interactive": False,
    }:
        _error(errors, 11, "golden declaration is invalid")
        return errors
    if errors:
        return errors
    directory = root / golden["directory"]
    manifest = _json(directory / "manifest.json", 256 * 1024, 12, errors)
    if manifest is None:
        return errors
    if manifest.get("schema") != golden["schema"] or set(manifest) != ROOT_KEYS:
        _error(errors, 12, "golden manifest schema is invalid")
    if manifest.get("status") != config.get("status"):
        _error(errors, 13, "golden manifest must remain software-only")
    if (manifest.get("platform") != golden["platform"] or manifest.get("role") != "ems" or
            manifest.get("fixture") != "HmiGoldenMain.qml"):
        _error(errors, 14, "golden provenance is invalid")
    try:
        generated = datetime.fromisoformat(str(manifest.get("generated_utc")))
        if generated.utcoffset() is None or generated.utcoffset().total_seconds() != 0:
            raise ValueError
    except (TypeError, ValueError):
        _error(errors, 15, "golden timestamp must be ISO-8601 UTC")
    if errors:
        return errors

    raw_images = manifest.get("images")
    if not isinstance(raw_images, list) or len(raw_images) != 18:
        _error(errors, 16, "golden must contain exactly 18 captures")
        return errors
    expected = [(w, h, state, age) for w, h in SIZES for state, age in STATES]
    actual = [(item.get("width"), item.get("height"), item.get("state"), item.get("age_ms"))
              for item in raw_images if isinstance(item, dict)]
    if len(actual) != len(raw_images) or actual != expected or len(set(actual)) != 18:
        _error(errors, 17, "state/resolution cross-product or order is invalid")
        return errors

    for item in raw_images:
        size = (item["width"], item["height"])
        expected_name = filename(*size, item["state"])
        if set(item) != IMAGE_KEYS or item.get("file") != expected_name:
            _error(errors, 18, f"image record or filename is invalid: {expected_name}")
            return errors
        name = item["file"]
        if Path(name).name != name or "/" in name or "\\" in name:
            _error(errors, 18, f"unsafe image path: {name}")
            return errors
        image = directory / name
        if not image.is_file():
            _error(errors, 19, f"missing image: {name}")
            return errors
        try:
            dimensions_valid = png_size(image) == size
            pixels_valid = png_pixel_sha256(image) == item.get("pixel_sha256")
        except ValueError:
            dimensions_valid = pixels_valid = False
        if (not dimensions_valid or image.stat().st_size < 10_000 or
                item.get("bytes") != image.stat().st_size):
            _error(errors, 19, f"invalid PNG dimensions or byte count: {name}")
            return errors
        if (not isinstance(item.get("sha256"), str) or not DIGEST.fullmatch(item["sha256"]) or
                sha256(image) != item["sha256"] or
                not isinstance(item.get("pixel_sha256"), str) or
                not DIGEST.fullmatch(item["pixel_sha256"]) or not pixels_valid):
            _error(errors, 20, f"digest mismatch: {name}")
            return errors
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    errors = validate(args.root)
    if errors:
        print("AVIONIX EMS responsive state matrix FAILED")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("AVIONIX EMS responsive state matrix PASS: 18 captures and 20 controls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
