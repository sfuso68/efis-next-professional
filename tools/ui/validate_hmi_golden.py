#!/usr/bin/env python3
"""Fail-closed validation of role-specific AVIONIX HMI golden bundles."""

from __future__ import annotations

import argparse
from datetime import datetime
import json
from pathlib import Path
import re

try:
    from .capture_hmi_golden import SIZES_BY_ROLE, png_pixel_sha256, png_size, sha256
except ImportError:
    from capture_hmi_golden import SIZES_BY_ROLE, png_pixel_sha256, png_size, sha256

ROOT_KEYS = {"schema", "status", "generated_utc", "platform", "fixture", "role",
             "data_state", "data_age_ms", "images"}
IMAGE_KEYS = {"file", "width", "height", "bytes", "sha256", "pixel_sha256"}
FIXTURES = {"ems": "HmiGoldenMain.qml", "skyview": "SkyViewGoldenMain.qml"}
STATES = {"LIVE", "STALE", "LOST", "FAIL", "SIM"}
DIGEST = re.compile(r"[0-9a-f]{64}")


def _error(errors: list[str], control: int, message: str) -> None:
    errors.append(f"[HMI-G20-{control:02d}] {message}")


def validate(directory: Path) -> list[str]:
    errors: list[str] = []
    manifest_path = directory / "manifest.json"
    if not manifest_path.is_file():
        return ["[HMI-G20-01] missing manifest.json"]
    if manifest_path.stat().st_size > 256 * 1024:
        return ["[HMI-G20-01] manifest exceeds 256 KiB"]
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as error:
        return [f"[HMI-G20-01] invalid manifest: {error}"]
    if not isinstance(manifest, dict) or manifest.get("schema") != "avionix.hmi-golden-manifest.v2":
        _error(errors, 1, "root object or schema is invalid")
        if not isinstance(manifest, dict):
            return errors
    if set(manifest) != ROOT_KEYS:
        _error(errors, 2, "manifest properties differ from the closed schema")
    if manifest.get("status") != "SOFTWARE_REFERENCE_NOT_HARDWARE_ACCEPTANCE":
        _error(errors, 3, "golden must not claim hardware acceptance")
    role = manifest.get("role")
    if role not in SIZES_BY_ROLE:
        _error(errors, 4, "role must be ems or skyview")
    if manifest.get("fixture") != FIXTURES.get(role):
        _error(errors, 5, "fixture does not match role")
    if not isinstance(manifest.get("platform"), str) or not manifest.get("platform", "").strip():
        _error(errors, 6, "platform must be a non-empty string")
    try:
        generated = datetime.fromisoformat(str(manifest.get("generated_utc")))
        if generated.utcoffset() is None or generated.utcoffset().total_seconds() != 0:
            raise ValueError("timestamp is not UTC")
    except (TypeError, ValueError):
        _error(errors, 7, "generated_utc must be an ISO-8601 UTC timestamp")
    state = manifest.get("data_state")
    if state not in STATES:
        _error(errors, 8, "data_state is not canonical")
    age = manifest.get("data_age_ms")
    if (not isinstance(age, int) or isinstance(age, bool) or age < 0 or
            (state == "SIM" and age != 0) or (state == "STALE" and not 250 < age <= 1000) or
            (state == "LOST" and age <= 1000)):
        _error(errors, 9, "data age is inconsistent with the declared state")
    if errors:
        return errors

    raw_images = manifest.get("images")
    if not isinstance(raw_images, list):
        _error(errors, 10, "images must be a list")
        raw_images = []
    rows_valid = all(isinstance(item, dict) for item in raw_images)
    records = {(item.get("width"), item.get("height")): item
               for item in raw_images if isinstance(item, dict)}
    expected_sizes = set(SIZES_BY_ROLE.get(role, ()))
    if not rows_valid or len(records) != len(raw_images) or set(records) != expected_sizes:
        _error(errors, 10, "images must contain exactly the role resolution matrix")
        return errors

    names: set[str] = set()
    for size, item in records.items():
        if set(item) != IMAGE_KEYS:
            _error(errors, 11, f"image properties differ from schema for {size}")
            continue
        expected_name = f"{role}-{size[0]}x{size[1]}.png"
        name = item.get("file")
        if name != expected_name:
            _error(errors, 12, f"unexpected filename for {size}")
        if not isinstance(name, str) or Path(name).name != name or "/" in name or "\\" in name:
            _error(errors, 19, f"unsafe image path for {size}")
            continue
        if name in names:
            _error(errors, 13, f"duplicate filename: {name}")
        names.add(name)
        if name != expected_name:
            continue
        image = directory / name
        if not image.is_file():
            _error(errors, 14, f"missing image: {name}")
            continue
        try:
            if png_size(image) != size:
                _error(errors, 15, f"PNG dimensions mismatch for {name}")
        except ValueError as error:
            _error(errors, 15, str(error))
            continue
        actual_bytes = image.stat().st_size
        if actual_bytes < 10_000 or item.get("bytes") != actual_bytes:
            _error(errors, 16, f"invalid byte count for {name}")
        if not isinstance(item.get("sha256"), str) or not DIGEST.fullmatch(item["sha256"]):
            _error(errors, 17, f"invalid SHA-256 syntax for {name}")
        elif sha256(image) != item["sha256"]:
            _error(errors, 17, f"SHA-256 mismatch for {name}")
        if not isinstance(item.get("pixel_sha256"), str) or not DIGEST.fullmatch(item["pixel_sha256"]):
            _error(errors, 18, f"invalid pixel digest syntax for {name}")
        else:
            try:
                if png_pixel_sha256(image) != item["pixel_sha256"]:
                    _error(errors, 18, f"pixel SHA-256 mismatch for {name}")
            except ValueError as error:
                _error(errors, 18, str(error))
    if role in SIZES_BY_ROLE and tuple(records) != SIZES_BY_ROLE[role]:
        _error(errors, 20, "resolution order differs from the role baseline")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    errors = validate(args.directory)
    if errors:
        print("AVIONIX HMI golden validation FAILED")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("AVIONIX HMI golden validation PASS: closed role matrix and 20 controls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
