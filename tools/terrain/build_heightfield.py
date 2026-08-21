#!/usr/bin/env python3
"""Build a GPU height-field image from a Copernicus/SRTM HGT tile."""
import argparse
import struct
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--size", type=int, default=513)
    parser.add_argument("--minimum", type=int, default=0)
    parser.add_argument("--maximum", type=int, default=3200)
    parser.add_argument("--color-output", type=Path)
    parser.add_argument("--aircraft-altitude-ft", type=float, default=7500)
    args = parser.parse_args()
    raw = args.input.read_bytes()
    side = int((len(raw) // 2) ** 0.5)
    if side * side * 2 != len(raw):
        raise SystemExit("invalid square HGT payload")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    span = max(1, args.maximum - args.minimum)
    sampled = []
    with args.output.open("wb") as stream:
        stream.write(f"P5\n{args.size} {args.size}\n65535\n".encode("ascii"))
        for y in range(args.size):
            source_y = round(y * (side - 1) / (args.size - 1))
            row = []
            elevations = []
            for x in range(args.size):
                source_x = round(x * (side - 1) / (args.size - 1))
                elevation = struct.unpack_from(">h", raw, 2 * (source_y * side + source_x))[0]
                elevation = args.minimum if elevation < -500 else elevation
                elevations.append(elevation)
                normalized = max(0, min(65535, round((elevation - args.minimum) * 65535 / span)))
                row.append(normalized)
            stream.write(struct.pack(">" + "H" * len(row), *row))
            sampled.append(elevations)
    if args.color_output:
        args.color_output.parent.mkdir(parents=True, exist_ok=True)
        aircraft_m = args.aircraft_altitude_ft * 0.3048
        with args.color_output.open("wb") as stream:
            stream.write(f"P6\n{args.size} {args.size}\n255\n".encode("ascii"))
            for y, elevations in enumerate(sampled):
                pixels = bytearray()
                for x, elevation in enumerate(elevations):
                    clearance = aircraft_m - elevation
                    band = max(0.0, min(1.0, (elevation - 500) / 2100))
                    base = (round(48 + 68 * band), round(86 - 23 * band), round(45 - 8 * band))
                    if clearance < 30:
                        alert, opacity = (225, 25, 20), 0.72
                        color = tuple(round(base[i] * (1-opacity) + alert[i] * opacity) for i in range(3))
                    elif clearance < 300:
                        alert, opacity = (245, 174, 30), 0.62
                        color = tuple(round(base[i] * (1-opacity) + alert[i] * opacity) for i in range(3))
                    else:
                        color = base
                    pixels.extend(color)
                stream.write(pixels)


if __name__ == "__main__":
    main()
