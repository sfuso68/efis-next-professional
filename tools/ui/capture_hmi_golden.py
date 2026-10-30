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
    with path.open("rb") as stream:
        header = stream.read(24)
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
    # Review files are inputs, not trusted merely because a manifest repeats
    # their hashes. Bound encoded and decoded memory before inflating IDAT.
    if path.stat().st_size > 8 * 1024 * 1024:
        raise ValueError(f"PNG exceeds encoded size limit: {path}")
    with path.open("rb") as stream:
        data = stream.read(8 * 1024 * 1024 + 1)
    if len(data) > 8 * 1024 * 1024:
        raise ValueError(f"PNG exceeds encoded size limit: {path}")
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a PNG image: {path}")
    offset = 8
    idat = bytearray()
    width = height = bit_depth = colour_type = 0
    header_seen = end_seen = False
    while offset + 12 <= len(data):
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        if length > len(data) - offset - 12:
            raise ValueError(f"truncated PNG chunk: {path}")
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        checksum = struct.unpack(">I", data[offset + 8 + length:offset + 12 + length])[0]
        if zlib.crc32(kind + payload) != checksum:
            raise ValueError(f"PNG chunk CRC mismatch: {path}")
        if kind in (b"acTL", b"fcTL", b"fdAT"):
            raise ValueError(f"animated PNG is not a static golden: {path}")
        if kind == b"IHDR":
            if header_seen or offset != 8 or length != 13:
                raise ValueError(f"invalid PNG IHDR: {path}")
            width, height, bit_depth, colour_type, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            if not width or not height or width * height > 4194304:
                raise ValueError(f"PNG exceeds pixel size limit: {path}")
            if compression != 0 or filtering != 0 or interlace != 0:
                raise ValueError(f"unsupported PNG coding: {path}")
            header_seen = True
        elif kind == b"IDAT":
            if not header_seen:
                raise ValueError(f"PNG IDAT precedes IHDR: {path}")
            idat.extend(payload)
        elif kind == b"IEND":
            if length or offset + 12 != len(data):
                raise ValueError(f"invalid PNG IEND/trailing bytes: {path}")
            end_seen = True
            break
        elif kind and not (kind[0] & 32):
            raise ValueError(f"unsupported critical PNG chunk: {path}")
        offset += 12 + length
    channels = {2: 3, 6: 4}.get(colour_type)
    if not header_seen or not end_seen or bit_depth != 8 or channels is None or not idat:
        raise ValueError(f"unsupported PNG format for pixel digest: {path}")
    stride = width * channels
    expected = height * (stride + 1)
    try:
        inflater = zlib.decompressobj()
        decoded = inflater.decompress(bytes(idat), expected + 1)
    except zlib.error as error:
        raise ValueError(f"invalid PNG deflate stream: {path}") from error
    if (len(decoded) != expected or not inflater.eof or inflater.unconsumed_tail
            or inflater.unused_data):
        raise ValueError(f"unexpected PNG scanline length: {path}")
    previous = bytearray(stride)
    pixels = bytearray()
    cursor = 0
    for _ in range(height):
        filter_type = decoded[cursor]
        source = decoded[cursor + 1:cursor + 1 + stride]
        cursor += stride + 1
        if filter_type == 0:
            # PNG None filter is already decoded; avoid a per-byte Python loop.
            pixels.extend(source)
            previous = bytearray(source)
            continue
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


def write_manifest(output_dir: Path, platform: str, data_state: str,
                   data_age_ms: int, role: str) -> dict:
    images = []
    for width, height in SIZES_BY_ROLE[role]:
        output = output_dir / f"{role}-{width}x{height}.png"
        if not output.is_file():
            raise FileNotFoundError(f"missing captured image: {output}")
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


def capture(executable: Path, output_dir: Path, platform: str, delay_ms: int,
            data_state: str = "SIM", data_age_ms: int = 0,
            role: str = "ems") -> dict:
    if role not in SIZES_BY_ROLE:
        raise ValueError(f"unsupported HMI golden role: {role}")
    output_dir.mkdir(parents=True, exist_ok=True)
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
            check=False, timeout=max(30, delay_ms // 1000 + 15),
        )
        if completed.returncode != 0:
            raise RuntimeError(f"capture {width}x{height} failed with {completed.returncode}")
        actual = png_size(output)
        if actual != (width, height):
            raise RuntimeError(f"capture size {actual} does not match {(width, height)}")
    return write_manifest(output_dir, platform, data_state, data_age_ms, role)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--platform", default="offscreen")
    parser.add_argument("--delay-ms", type=int, default=800)
    parser.add_argument("--state", choices=("LIVE", "STALE", "DEGRADED", "LOST", "FAIL", "SIM"), default="SIM")
    parser.add_argument("--age-ms", type=int, default=0)
    parser.add_argument("--role", choices=tuple(SIZES_BY_ROLE), default="ems")
    parser.add_argument("--manifest-only", action="store_true",
                        help="hash already captured images without launching Qt")
    args = parser.parse_args()
    if not args.manifest_only and (args.executable is None or not args.executable.is_file()):
        parser.error(f"executable not found: {args.executable}")
    if args.age_ms < 0:
        parser.error("--age-ms must be non-negative")
    result = (write_manifest(args.output_dir, args.platform, args.state, args.age_ms, args.role)
              if args.manifest_only else
              capture(args.executable, args.output_dir, args.platform, args.delay_ms,
                      args.state, args.age_ms, args.role))
    print(json.dumps(result))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
