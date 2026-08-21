#!/usr/bin/env python3
"""Build the read-only EFIS imagery catalogue from staged geographic mosaics."""
import argparse
import hashlib
import json
import re
import struct
from datetime import datetime, timezone
from pathlib import Path

TILE = re.compile(r"^(N|S)(\d{2})(E|W)(\d{3})-sentinel2(?:-(\d+))?\.png$")


def png_size(path):
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or not header.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError(f"not a PNG: {path}")
    return struct.unpack(">II", header[16:24])


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def bounds(match):
    latitude = int(match.group(2)) * (1 if match.group(1) == "N" else -1)
    longitude = int(match.group(4)) * (1 if match.group(3) == "E" else -1)
    return {"west": longitude, "south": latitude, "east": longitude + 1, "north": latitude + 1}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--public-base-url", default="https://mappe.casa.lan/api/imagery/source")
    args = parser.parse_args()
    sources = args.root / "sources"
    records = []
    for path in sorted(sources.glob("*-sentinel2*.png")):
        match = TILE.match(path.name)
        if not match:
            continue
        width, height = png_size(path)
        records.append({
            "tile_key": path.name[:7], "path": f"sources/{path.name}",
            "url": f"{args.public_base_url}/{path.name}", "bounds": bounds(match),
            "width": width, "height": height, "bytes": path.stat().st_size,
            "sha256": sha256(path), "status": "DEVELOPMENT"
        })
    now = datetime.now(timezone.utc).isoformat()
    dataset_id = "north-italy-sentinel2-development"
    pyramid_root = args.root / "tiles" / dataset_id
    zoom_inventory = []
    if pyramid_root.is_dir():
        for directory in sorted(pyramid_root.iterdir(), key=lambda item: int(item.name) if item.name.isdigit() else 999):
            if not directory.is_dir() or not directory.name.isdigit():
                continue
            files = [path for path in directory.rglob("*.webp") if path.is_file()]
            zoom_inventory.append({"zoom": int(directory.name), "tile_count": len(files),
                                   "bytes": sum(path.stat().st_size for path in files)})
    pyramid = {
        "format": "WEBP", "tile_size": 512, "xyz": True,
        "min_zoom": min((item["zoom"] for item in zoom_inventory), default=None),
        "max_zoom": max((item["zoom"] for item in zoom_inventory), default=None),
        "tile_count": sum(item["tile_count"] for item in zoom_inventory),
        "bytes": sum(item["bytes"] for item in zoom_inventory),
        "zooms": zoom_inventory
    } if zoom_inventory else None
    pyramid_state = "READY" if pyramid else "NOT_BUILT"
    manifest = {
        "schema": "efis-next.imagery-dataset.v1", "dataset_id": dataset_id,
        "generated_utc": now, "status": "DEVELOPMENT", "format": "png-source-mosaic",
        "source": {"provider": "Copernicus Data Space Ecosystem", "dataset": "Sentinel-2 L2A",
                   "attribution": "Contains modified Copernicus Sentinel data 2025"},
        "coverage_target": {"west": 6.5, "south": 43.4, "east": 14.0, "north": 47.1},
        "source_mosaics": records,
        "source_mosaic_count": len(records),
        "tile_pyramid_state": pyramid_state,
        "tile_pyramid": pyramid
    }
    manifests = args.root / "manifests"
    manifests.mkdir(parents=True, exist_ok=True)
    (manifests / f"{dataset_id}.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    catalog = {
        "schema": "efis-next.imagery-catalog.v1", "catalog_id": "north-italy-development",
        "generated_utc": now, "status": "DEVELOPMENT",
        "datasets": [{"dataset_id": dataset_id, "title": "North Italy Sentinel-2 development imagery",
                      "manifest_path": f"manifests/{dataset_id}.json", "status": "DEVELOPMENT",
                      "source_mosaic_count": len(records), "tile_pyramid_state": pyramid_state,
                      "tile_count": pyramid.get("tile_count", 0) if pyramid else 0,
                      "tile_bytes": pyramid.get("bytes", 0) if pyramid else 0}]
    }
    (args.root / "catalog.json").write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"root": str(args.root), "mosaics": len(records), "catalog": str(args.root / "catalog.json")}))


if __name__ == "__main__":
    main()
