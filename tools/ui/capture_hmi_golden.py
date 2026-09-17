#!/usr/bin/env python3
"""Capture deterministic AVIONIX HMI reference images from the Qt renderer."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import zlib
from datetime import datetime, timezone

SIZES_BY_ROLE = {
    "ems": ((800, 480), (1024, 600), (1280, 800)),
    "skyview": ((1024, 600), (1280, 800), (1920, 1080)),
}
SIZES = SIZES_BY_ROLE["ems"]


def png_size(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"not a PNG image: {path}")
    return struct.unpack(">II", header[16:24])


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def png_pixels(path: Path) -> bytes:
    data = path.read_bytes()
    offset = 8
    idat = bytearray()
    width = height = bit_depth = colour_type = 0
    while offset + 12 <= len(data):
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        if kind == b"IHDR":
            width, height, bit_depth, colour_type = struct.unpack(">IIBB", payload[:10])
        elif kind == b"IDAT":
            idat.extend(payload)
        elif kind == b"IEND":
            break
        offset += 12 + length
    channels = {2: 3, 6: 4}.get(colour_type)
    if bit_depth != 8 or channels is None or not idat:
        raise ValueError(f"unsupported PNG format for pixel digest: {path}")
    decoded = zlib.decompress(bytes(idat))
    stride = width * channels
    expected = height * (stride + 1)
    if len(decoded) != expected:
        raise ValueError(f"unexpected PNG scanline length: {path}")
    previous = bytearray(stride)
    pixels = bytearray()
    cursor = 0
    for _ in range(height):
        filter_type = decoded[cursor]
        source = decoded[cursor + 1:cursor + 1 + stride]
        cursor += stride + 1
        row = bytearray(stride)
        for index, value in enumerate(source):
            left = row[index - channels] if index >= channels else 0
            above = previous[index]
            upper_left = previous[index - channels] if index >= channels else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                estimate = left + above - upper_left
                distances = (abs(estimate-left), abs(estimate-above), abs(estimate-upper_left))
                predictor = (left, above, upper_left)[distances.index(min(distances))]
            else:
                raise ValueError(f"unknown PNG filter {filter_type}: {path}")
            row[index] = (value + predictor) & 0xFF
        pixels.extend(row)
        previous = row
    return bytes(pixels)


def png_pixel_sha256(path: Path) -> str:
    return hashlib.sha256(png_pixels(path)).hexdigest()


def capture(executable: Path, output_dir: Path, platform: str, delay_ms: int,
            data_state: str = "SIM", data_age_ms: int = 0,
            role: str = "ems") -> dict:
    if role not in SIZES_BY_ROLE:
        raise ValueError(f"unsupported HMI golden role: {role}")
    output_dir.mkdir(parents=True, exist_ok=True)
    images = []
    for width, height in SIZES_BY_ROLE[role]:
        output = output_dir / f"{role}-{width}x{height}.png"
        environment = os.environ.copy()
        environment.update({
            "QT_QPA_PLATFORM": platform,
            "QT_SCALE_FACTOR": "1",
            "EFIS_CAPTURE": str(output.resolve()),
            "EFIS_CAPTURE_SIZE": f"{width}x{height}",
            "EFIS_CAPTURE_DELAY_MS": str(delay_ms),
            "EFIS_CAPTURE_EXIT": "1",
            "EFIS_HMI_GOLDEN_STATE": data_state,
            "EFIS_HMI_GOLDEN_AGE_MS": str(data_age_ms),
        })
        completed = subprocess.run(
            [str(executable), "--skyview-golden" if role == "skyview" else "--hmi-golden"],
            env=environment,
            check=False, timeout=max(15, delay_ms // 1000 + 10),
        )
        if completed.returncode != 0:
            raise RuntimeError(f"capture {width}x{height} failed with {completed.returncode}")
        actual = png_size(output)
        if actual != (width, height):
            raise RuntimeError(f"capture size {actual} does not match {(width, height)}")
        images.append({
            "file": output.name,
            "width": width,
            "height": height,
            "bytes": output.stat().st_size,
            "sha256": sha256(output),
            "pixel_sha256": png_pixel_sha256(output),
        })
    manifest = {
        "schema": "avionix.hmi-golden-manifest.v2",
        "status": "SOFTWARE_REFERENCE_NOT_HARDWARE_ACCEPTANCE",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "platform": platform,
        "fixture": "SkyViewGoldenMain.qml" if role == "skyview" else "HmiGoldenMain.qml",
        "role": role,
        "data_state": data_state,
        "data_age_ms": data_age_ms,
        "images": images,
    }
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--platform", default="offscreen")
    parser.add_argument("--delay-ms", type=int, default=800)
    parser.add_argument("--state", choices=("LIVE", "STALE", "LOST", "FAIL", "SIM"), default="SIM")
    parser.add_argument("--age-ms", type=int, default=0)
    parser.add_argument("--role", choices=tuple(SIZES_BY_ROLE), default="ems")
    args = parser.parse_args()
    if not args.executable.is_file():
        parser.error(f"executable not found: {args.executable}")
    if args.age_ms < 0:
        parser.error("--age-ms must be non-negative")
    print(json.dumps(capture(args.executable, args.output_dir, args.platform, args.delay_ms,
                             args.state, args.age_ms, args.role)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
