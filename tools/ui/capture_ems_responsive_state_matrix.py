#!/usr/bin/env python3
"""Capture or sign the deterministic EMS state/resolution cross-product."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess

try:
    from .capture_hmi_golden import png_pixel_sha256, png_size, sha256
except ImportError:
    from capture_hmi_golden import png_pixel_sha256, png_size, sha256


SIZES = ((800, 480), (1024, 600), (1280, 800))
STATES = (("LIVE", 0), ("STALE", 600), ("DEGRADED", 600),
          ("LOST", 1500), ("FAIL", 0), ("SIM", 0))


def filename(width: int, height: int, state: str) -> str:
    return f"ems-{width}x{height}-{state.lower()}.png"


def write_manifest(output_dir: Path, platform: str) -> dict:
    images = []
    for width, height in SIZES:
        for state, age_ms in STATES:
            image = output_dir / filename(width, height, state)
            if not image.is_file() or png_size(image) != (width, height):
                raise RuntimeError(f"missing or invalid state capture: {image}")
            images.append({
                "state": state,
                "age_ms": age_ms,
                "width": width,
                "height": height,
                "file": image.name,
                "bytes": image.stat().st_size,
                "sha256": sha256(image),
                "pixel_sha256": png_pixel_sha256(image),
            })
    manifest = {
        "schema": "avionix.ems-responsive-state-golden.v1",
        "status": "SOFTWARE_REFERENCE_NOT_HARDWARE_ACCEPTANCE",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "platform": platform,
        "role": "ems",
        "fixture": "HmiGoldenMain.qml",
        "images": images,
    }
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    return manifest


def capture(executable: Path, output_dir: Path, platform: str, delay_ms: int) -> dict:
    output_dir.mkdir(parents=True, exist_ok=True)
    for width, height in SIZES:
        for state, age_ms in STATES:
            output = output_dir / filename(width, height, state)
            environment = os.environ.copy()
            environment.update({
                "QT_QPA_PLATFORM": platform,
                "QT_SCALE_FACTOR": "1",
                "EFIS_CAPTURE": str(output.resolve()),
                "EFIS_CAPTURE_SIZE": f"{width}x{height}",
                "EFIS_CAPTURE_DELAY_MS": str(delay_ms),
                "EFIS_CAPTURE_EXIT": "1",
                "EFIS_HMI_GOLDEN_STATE": state,
                "EFIS_HMI_GOLDEN_AGE_MS": str(age_ms),
            })
            completed = subprocess.run(
                [str(executable), "--hmi-golden"], env=environment,
                check=False, timeout=max(30, delay_ms // 1000 + 15),
            )
            if completed.returncode != 0:
                raise RuntimeError(
                    f"capture {width}x{height}/{state} failed with {completed.returncode}"
                )
    return write_manifest(output_dir, platform)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--platform", default="offscreen")
    parser.add_argument("--delay-ms", type=int, default=800)
    parser.add_argument("--manifest-only", action="store_true")
    args = parser.parse_args()
    if not args.manifest_only and (args.executable is None or not args.executable.is_file()):
        parser.error(f"executable not found: {args.executable}")
    result = (write_manifest(args.output_dir, args.platform) if args.manifest_only else
              capture(args.executable, args.output_dir, args.platform, args.delay_ms))
    print(json.dumps(result))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
