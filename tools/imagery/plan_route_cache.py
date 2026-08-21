#!/usr/bin/env python3
"""Build a deterministic imagery-tile download plan for a flight route."""
import argparse
import json
import math
from pathlib import Path


def tile_xy(latitude, longitude, zoom):
    latitude = max(-85.05112878, min(85.05112878, latitude))
    count = 1 << zoom
    x = int((longitude + 180.0) / 360.0 * count)
    lat_rad = math.radians(latitude)
    y = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) * 0.5 * count)
    return max(0, min(count - 1, x)), max(0, min(count - 1, y))


def interpolate(route, step_nm=1.0):
    points = []
    for start, end in zip(route, route[1:]):
        mean_lat = math.radians((start[0] + end[0]) * 0.5)
        north_nm = (end[0] - start[0]) * 60.0
        east_nm = (end[1] - start[1]) * 60.0 * math.cos(mean_lat)
        count = max(1, math.ceil(math.hypot(north_nm, east_nm) / step_nm))
        for index in range(count):
            ratio = index / count
            points.append((start[0] + (end[0] - start[0]) * ratio,
                           start[1] + (end[1] - start[1]) * ratio))
    points.append(tuple(route[-1]))
    return points


def route_tiles(route, zoom, corridor_nm):
    result = set()
    for latitude, longitude in interpolate(route):
        center_x, center_y = tile_xy(latitude, longitude, zoom)
        metres_per_tile = 40075016.686 * math.cos(math.radians(latitude)) / (1 << zoom)
        radius = max(1, math.ceil(corridor_nm * 1852.0 / metres_per_tile))
        for dy in range(-radius, radius + 1):
            for dx in range(-radius, radius + 1):
                if dx * dx + dy * dy <= (radius + 0.35) ** 2:
                    result.add((zoom, center_x + dx, center_y + dy))
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--route", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--zooms", nargs="+", type=int, default=[11, 12, 13, 14])
    parser.add_argument("--corridor-nm", type=float, default=8.0)
    args = parser.parse_args()
    route_document = json.loads(args.route.read_text(encoding="utf-8"))
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    route = route_document["route"]
    if len(route) < 2:
        raise SystemExit("route requires at least two latitude/longitude points")
    tiles = set()
    for zoom in args.zooms:
        if zoom < manifest["min_zoom"] or zoom > manifest["max_zoom"]:
            raise SystemExit(f"zoom {zoom} outside server manifest")
        tiles.update(route_tiles(route, zoom, args.corridor_nm))
    template = manifest["tile_url_template"]
    entries = [{"z": z, "x": x, "y": y,
                "url": template.replace("{z}", str(z)).replace("{x}", str(x)).replace("{y}", str(y)),
                "cache_path": f'{manifest["dataset_id"]}/{z}/{x}/{y}.{manifest["format"]}'}
               for z, x, y in sorted(tiles)]
    output = {
        "schema": "efis-next.imagery-download-plan.v1",
        "selection_mode": "FLIGHT_PLAN",
        "dataset_id": manifest["dataset_id"],
        "route_id": route_document.get("route_id", args.route.stem),
        "corridor_nm": args.corridor_nm,
        "zooms": sorted(args.zooms),
        "tile_count": len(entries),
        "estimated_bytes": len(entries) * 90 * 1024,
        "tiles": entries,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), "tiles": len(entries), "zooms": sorted(args.zooms)}))


if __name__ == "__main__":
    main()
