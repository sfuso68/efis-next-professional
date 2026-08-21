#!/usr/bin/env python3
"""Validate an installed EFIS terrain package and its shared tile boundaries."""
import argparse
import hashlib
import json
import math
import re
import sys
from pathlib import Path

import numpy as np

ROWS = 3601
EXPECTED_BYTES = ROWS * ROWS * 2
KEY = re.compile(r"^([NS])(\d{2})([EW])(\d{3})$")


def decode_key(value):
    match = KEY.fullmatch(value)
    if not match:
        raise ValueError(f"invalid tile key: {value}")
    lat = int(match.group(2)) * (1 if match.group(1) == "N" else -1)
    lon = int(match.group(4)) * (1 if match.group(3) == "E" else -1)
    return lat, lon


def encode_key(lat, lon):
    return f"{'N' if lat >= 0 else 'S'}{abs(lat):02d}{'E' if lon >= 0 else 'W'}{abs(lon):03d}"


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def load_tile(path):
    return np.memmap(path, dtype=">i2", mode="r", shape=(ROWS, ROWS))


def valid_difference(left, right):
    mask = (left != -32768) & (right != -32768)
    if not np.any(mask):
        return None
    difference = np.abs(left[mask].astype(np.int32) - right[mask].astype(np.int32))
    return {
        "samples": int(difference.size),
        "maximum_m": int(difference.max()),
        "mean_m": round(float(difference.mean()), 4),
        "p95_m": round(float(np.percentile(difference, 95)), 4),
        "over_1m": int(np.count_nonzero(difference > 1)),
    }


def sample(tile, latitude, longitude):
    base_lat = math.floor(latitude)
    base_lon = math.floor(longitude)
    row = min(3600, max(0, round((base_lat + 1.0 - latitude) * 3600.0)))
    column = min(3600, max(0, round((longitude - base_lon) * 3600.0)))
    value = int(tile[row, column])
    return None if value == -32768 else value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("package", type=Path)
    parser.add_argument("--sample", action="append", default=[],
                        help="name,latitude,longitude,expected_m,tolerance_m")
    parser.add_argument("--maximum-boundary-difference", type=float, default=1.0)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    manifest = json.loads((args.package / "manifest.json").read_text(encoding="utf-8"))
    records = manifest.get("tiles", [])
    tiles = {}
    integrity = []
    failures = []
    for record in records:
        key = record["key"]
        if key in tiles:
            failures.append(f"duplicate tile {key}")
            continue
        path = args.package / record["path"]
        size_ok = path.is_file() and path.stat().st_size == EXPECTED_BYTES
        hash_ok = size_ok and digest(path) == record.get("sha256")
        integrity.append({"key": key, "size_ok": size_ok, "hash_ok": hash_ok})
        if not size_ok or not hash_ok:
            failures.append(f"integrity failure {key}")
        else:
            tiles[key] = path

    boundaries = []
    for key, path in sorted(tiles.items()):
        lat, lon = decode_key(key)
        current = load_tile(path)
        for neighbour, edge, a_values, b_values in (
            (encode_key(lat, lon + 1), "E-W", current[:, -1], "west"),
            (encode_key(lat + 1, lon), "N-S", current[0, :], "south"),
        ):
            if neighbour not in tiles:
                continue
            adjacent = load_tile(tiles[neighbour])
            other = adjacent[:, 0] if b_values == "west" else adjacent[-1, :]
            stats = valid_difference(a_values, other)
            boundaries.append({"a": key, "b": neighbour, "edge": edge, **(stats or {})})
            if not stats or stats["maximum_m"] > args.maximum_boundary_difference:
                failures.append(f"boundary discontinuity {key}/{neighbour}")

    samples = []
    for specification in args.sample:
        name, lat_text, lon_text, expected_text, tolerance_text = specification.split(",")
        latitude, longitude = float(lat_text), float(lon_text)
        expected, tolerance = float(expected_text), float(tolerance_text)
        key = encode_key(math.floor(latitude), math.floor(longitude))
        value = sample(load_tile(tiles[key]), latitude, longitude) if key in tiles else None
        accepted = value is not None and abs(value - expected) <= tolerance
        samples.append({"name": name, "key": key, "elevation_m": value,
                        "expected_m": expected, "tolerance_m": tolerance,
                        "accepted": accepted})
        if not accepted:
            failures.append(f"sample outside tolerance {name}")

    result = {
        "schema_version": 1,
        "package_id": manifest.get("package_id"),
        "vertical_datum": manifest.get("vertical_datum"),
        "tiles": len(tiles),
        "integrity": integrity,
        "boundaries": boundaries,
        "samples": samples,
        "passed": not failures,
        "failures": failures,
    }
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result, sort_keys=True))
    if failures:
        raise SystemExit(4)


if __name__ == "__main__":
    try:
        main()
    except (KeyError, ValueError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
