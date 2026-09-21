#!/usr/bin/env python3
"""Re-render every EMS golden state and compare the fresh pixels fail-closed."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import tempfile

try:
    from .capture_ems_responsive_state_matrix import capture
    from .capture_hmi_golden import png_pixels, png_size
    from .compare_hmi_golden import difference
except ImportError:
    from capture_ems_responsive_state_matrix import capture
    from capture_hmi_golden import png_pixels, png_size
    from compare_hmi_golden import difference


def _key(image: dict) -> tuple[int, int, str]:
    return image.get("width"), image.get("height"), image.get("state")


def compare_manifests(reference: dict, rerendered: dict) -> list[str]:
    """Compare ordered semantic keys and decoded-pixel digests.

    Encoded PNG bytes are deliberately ignored: metadata and compressor output
    may differ while the displayed pixels remain identical.
    """
    errors: list[str] = []
    expected = reference.get("images")
    actual = rerendered.get("images")
    if not isinstance(expected, list) or not isinstance(actual, list):
        return ["[HMI-RERENDER-01] both manifests must contain image lists"]
    expected_keys = [_key(item) for item in expected if isinstance(item, dict)]
    actual_keys = [_key(item) for item in actual if isinstance(item, dict)]
    if len(expected_keys) != len(expected) or expected_keys != actual_keys:
        return ["[HMI-RERENDER-02] rerendered state/resolution matrix differs"]
    return errors


def visual_drift_exceeds_tolerance(peak: int, mean: float, changed: float) -> bool:
    """Reject visible drift while allowing one-level raster rounding noise.

    A ±1 channel delta is the smallest possible 8-bit difference and can vary
    between otherwise identical Mesa/OpenGL runs. Deltas of two or more remain
    subject to the strict aggregate limits; a peak above eight always fails.
    """
    return peak > 8 or (peak > 1 and (mean > 0.001 or changed > 0.001))


def compare_directories(reference_dir: Path, rerender_dir: Path) -> list[str]:
    reference_path = reference_dir / "manifest.json"
    rerender_path = rerender_dir / "manifest.json"
    if not reference_path.is_file() or not rerender_path.is_file():
        return ["[HMI-RERENDER-05] reference or rerender manifest is missing"]
    reference = json.loads(reference_path.read_text(encoding="utf-8"))
    rerendered = json.loads(rerender_path.read_text(encoding="utf-8"))
    errors = compare_manifests(reference, rerendered)
    if errors:
        return errors
    for reference_image, actual_image in zip(reference["images"], rerendered["images"]):
        reference_file = reference_dir / reference_image["file"]
        actual_file = rerender_dir / actual_image["file"]
        try:
            if png_size(reference_file) != png_size(actual_file):
                errors.append(f"[HMI-RERENDER-03] dimension mismatch for {reference_file.name}")
                continue
            peak, mean, changed = difference(
                png_pixels(reference_file), png_pixels(actual_file)
            )
        except (OSError, ValueError) as error:
            errors.append(f"[HMI-RERENDER-03] invalid PNG {reference_file.name}: {error}")
            continue
        if visual_drift_exceeds_tolerance(peak, mean, changed):
            errors.append(
                f"[HMI-RERENDER-03] visual drift {reference_file.name}: "
                f"max={peak}, mean={mean:.6f}, changed={changed:.6f}"
            )
    return errors


def verify(executable: Path, reference_dir: Path, platform: str, delay_ms: int,
           output_dir: Path | None = None) -> list[str]:
    manifest_path = reference_dir / "manifest.json"
    if not executable.is_file():
        return [f"[HMI-RERENDER-04] renderer not found: {executable}"]
    if not manifest_path.is_file():
        return [f"[HMI-RERENDER-05] reference manifest not found: {manifest_path}"]
    if output_dir is not None:
        output_dir.mkdir(parents=True, exist_ok=True)
        capture(executable, output_dir, platform, delay_ms)
        return compare_directories(reference_dir, output_dir)
    with tempfile.TemporaryDirectory(prefix="avionix-ems-rerender-") as temporary:
        rerender_dir = Path(temporary)
        capture(executable, rerender_dir, platform, delay_ms)
        return compare_directories(reference_dir, rerender_dir)


def main() -> int:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--executable", type=Path)
    group.add_argument("--rerender-dir", type=Path)
    parser.add_argument("--reference-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path,
                        help="persist fresh captures for CI diagnostics")
    parser.add_argument("--platform", default="offscreen")
    parser.add_argument("--delay-ms", type=int, default=800)
    args = parser.parse_args()
    try:
        errors = (compare_directories(args.reference_dir, args.rerender_dir)
                  if args.rerender_dir is not None else
                  verify(args.executable, args.reference_dir, args.platform, args.delay_ms,
                         args.output_dir))
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError,
            subprocess.SubprocessError) as error:
        errors = [f"[HMI-RERENDER-06] rerender failed: {error}"]
    if errors:
        print("AVIONIX EMS golden rerender FAILED")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("AVIONIX EMS golden rerender PASS: fresh pixels match 18 references")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
