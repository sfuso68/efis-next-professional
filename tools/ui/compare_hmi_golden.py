#!/usr/bin/env python3
"""Tolerance-based visual comparison for independently rendered HMI PNGs."""

from __future__ import annotations

import argparse
from pathlib import Path

try:
    from .capture_hmi_golden import SIZES, png_pixels, png_size
except ImportError:
    from capture_hmi_golden import SIZES, png_pixels, png_size


def difference(left: bytes, right: bytes) -> tuple[int, float, float]:
    if len(left) != len(right):
        raise ValueError("pixel buffers have different lengths")
    if not left:
        return 0, 0.0, 0.0
    deltas = [abs(a-b) for a, b in zip(left, right)]
    return max(deltas), sum(deltas)/len(deltas), sum(delta > 0 for delta in deltas)/len(deltas)


def compare(reference: Path, candidate: Path,
            maximum_delta: int = 8, maximum_mean: float = 0.001,
            maximum_changed_fraction: float = 0.001) -> list[str]:
    errors: list[str] = []
    for width, height in SIZES:
        name = f"ems-{width}x{height}.png"
        expected, actual = reference/name, candidate/name
        if not expected.is_file() or not actual.is_file():
            errors.append(f"missing comparison image: {name}")
            continue
        if png_size(expected) != png_size(actual):
            errors.append(f"dimension mismatch: {name}")
            continue
        peak, mean, changed = difference(png_pixels(expected), png_pixels(actual))
        if peak > maximum_delta or mean > maximum_mean or changed > maximum_changed_fraction:
            errors.append(
                f"visual drift {name}: max={peak}, mean={mean:.6f}, changed={changed:.6f}"
            )
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    args = parser.parse_args()
    errors = compare(args.reference, args.candidate)
    if errors:
        print("AVIONIX HMI visual comparison FAILED")
        for error in errors:
            print(f"  - {error}")
        return 1
    print("AVIONIX HMI visual comparison PASS within anti-alias tolerance")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
