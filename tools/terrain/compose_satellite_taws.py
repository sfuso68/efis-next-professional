#!/usr/bin/env python3
"""Compose Sentinel-2 true colour with a translucent scenario TAWS overlay."""
import argparse
import json
import struct
from pathlib import Path

from PIL import Image, ImageEnhance, ImageFilter


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--satellite", type=Path, required=True)
    parser.add_argument("--hgt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--aircraft-altitude-ft", type=float, required=True)
    parser.add_argument("--warning-clearance-m", type=float, default=300)
    parser.add_argument("--danger-clearance-m", type=float, default=30)
    args = parser.parse_args()

    satellite = Image.open(args.satellite).convert("RGB")
    # Sentinel mountain scenes contain deep north-facing shadows. A gentle
    # gamma lift recovers ground detail before the real-time 3D light is added.
    shadow_lut = [round(255 * ((value / 255) ** 0.78)) for value in range(256)]
    satellite = satellite.point(shadow_lut * 3)
    satellite = ImageEnhance.Contrast(satellite).enhance(1.10)
    satellite = ImageEnhance.Color(satellite).enhance(1.08)
    width, height = satellite.size
    raw = args.hgt.read_bytes()
    side = int((len(raw) // 2) ** 0.5)
    if side * side * 2 != len(raw):
        raise SystemExit("invalid square HGT payload")
    aircraft_m = args.aircraft_altitude_ft * 0.3048
    mask = Image.new("RGBA", satellite.size, (0, 0, 0, 0))
    pixels = mask.load()
    for y in range(height):
        sy = round(y * (side - 1) / (height - 1))
        for x in range(width):
            sx = round(x * (side - 1) / (width - 1))
            elevation = struct.unpack_from(">h", raw, 2 * (sy * side + sx))[0]
            clearance = aircraft_m - elevation
            if clearance < args.danger_clearance_m:
                pixels[x, y] = (238, 35, 28, 126)
            elif clearance < args.warning_clearance_m:
                pixels[x, y] = (248, 178, 30, 96)
    # A very small blur removes pixel stair-steps but retains terrain boundaries.
    mask = mask.filter(ImageFilter.GaussianBlur(radius=0.65))
    result = Image.alpha_composite(satellite.convert("RGBA"), mask)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    result.save(args.output, optimize=True)
    print(json.dumps({"output": str(args.output), "size": [width, height],
                      "aircraft_altitude_ft": args.aircraft_altitude_ft}))


if __name__ == "__main__":
    main()
