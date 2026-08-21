#!/usr/bin/env python3
"""Build an imagery plan for a rectangular or circular operating area."""
import argparse
import json
import math
from pathlib import Path

from plan_route_cache import tile_xy


def tile_bounds(west, south, east, north, zoom):
    x0, y0 = tile_xy(north, west, zoom)
    x1, y1 = tile_xy(south, east, zoom)
    for y in range(min(y0, y1), max(y0, y1) + 1):
        for x in range(min(x0, x1), max(x0, x1) + 1):
            yield zoom, x, y


def circle_bbox(latitude, longitude, radius_nm):
    lat_delta = radius_nm / 60.0
    lon_delta = radius_nm / (60.0 * max(0.1, math.cos(math.radians(latitude))))
    return longitude - lon_delta, latitude - lat_delta, longitude + lon_delta, latitude + lat_delta


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--area-id", required=True)
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--bbox", nargs=4, type=float, metavar=("WEST", "SOUTH", "EAST", "NORTH"))
    selection.add_argument("--circle", nargs=3, type=float, metavar=("LAT", "LON", "RADIUS_NM"))
    parser.add_argument("--zooms", nargs="+", type=int, default=[11, 12, 13, 14])
    parser.add_argument("--average-tile-kib", type=int, default=90)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    bbox = tuple(args.bbox) if args.bbox else circle_bbox(*args.circle)
    west, south, east, north = bbox
    coverage = manifest["coverage"]
    if west < coverage["west"] or east > coverage["east"] or south < coverage["south"] or north > coverage["north"]:
        raise SystemExit("selected area exceeds dataset coverage")
    tiles = set()
    for zoom in args.zooms:
        if zoom < manifest["min_zoom"] or zoom > manifest["max_zoom"]:
            raise SystemExit(f"zoom {zoom} outside server manifest")
        tiles.update(tile_bounds(west, south, east, north, zoom))
    template = manifest["tile_url_template"]
    entries = [{"z": z, "x": x, "y": y,
                "priority": "AREA",
                "url": template.replace("{z}", str(z)).replace("{x}", str(x)).replace("{y}", str(y)),
                "cache_path": f'{manifest["dataset_id"]}/{z}/{x}/{y}.{manifest["format"]}'}
               for z, x, y in sorted(tiles)]
    document = {
        "schema": "efis-next.imagery-download-plan.v1",
        "selection_mode": "AREA",
        "dataset_id": manifest["dataset_id"],
        "area_id": args.area_id,
        "bounds": {"west": west, "south": south, "east": east, "north": north},
        "zooms": sorted(args.zooms),
        "tile_count": len(entries),
        "estimated_bytes": len(entries) * args.average_tile_kib * 1024,
        "tiles": entries,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), "tiles": len(entries),
                      "estimated_mib": round(document["estimated_bytes"] / 1048576, 1)}))


if __name__ == "__main__":
    main()
