#!/usr/bin/env python3
"""Validate HMI golden provenance, dimensions and content identity."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

try:
    from .capture_hmi_golden import SIZES, png_pixel_sha256, png_size, sha256
except ImportError:
    from capture_hmi_golden import SIZES, png_pixel_sha256, png_size, sha256


def validate(directory: Path) -> list[str]:
    errors: list[str] = []
    manifest_path = directory / "manifest.json"
    if not manifest_path.is_file():
        return ["missing manifest.json"]
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as error:
        return [f"invalid manifest: {error}"]
    if manifest.get("schema") != "avionix.hmi-golden-manifest.v1":
        errors.append("unexpected manifest schema")
    if manifest.get("status") != "SOFTWARE_REFERENCE_NOT_HARDWARE_ACCEPTANCE":
        errors.append("golden must not claim hardware acceptance")
    records = {(item.get("width"), item.get("height")): item for item in manifest.get("images", [])}
    if set(records) != set(SIZES):
        errors.append("manifest must contain exactly the three target resolutions")
    for size in SIZES:
        item = records.get(size)
        if not item:
            continue
        image = directory / str(item.get("file", ""))
        if not image.is_file():
            errors.append(f"missing image for {size[0]}x{size[1]}")
            continue
        try:
            if png_size(image) != size:
                errors.append(f"PNG dimensions mismatch for {image.name}")
        except ValueError as error:
            errors.append(str(error))
            continue
        if image.stat().st_size < 10_000:
            errors.append(f"suspiciously small image: {image.name}")
        if sha256(image) != item.get("sha256"):
            errors.append(f"SHA-256 mismatch for {image.name}")
        try:
            if png_pixel_sha256(image) != item.get("pixel_sha256"):
                errors.append(f"pixel SHA-256 mismatch for {image.name}")
        except ValueError as error:
            errors.append(str(error))
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
    print("AVIONIX HMI golden validation PASS: 800x480, 1024x600, 1280x800")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
