#!/usr/bin/env python3
"""Compose cached XYZ tiles into a georeferenced route atlas."""
import argparse
import json
import math
import os
import tempfile
from pathlib import Path
from PIL import Image


def world_pixel(lat, lon, zoom, tile_size=512):
    scale = (1 << zoom) * tile_size
    x = (lon + 180.0) / 360.0 * scale
    latitude = math.radians(max(-85.05112878, min(85.05112878, lat)))
    y = (1.0 - math.asinh(math.tan(latitude)) / math.pi) * 0.5 * scale
    return x, y


def route_bounds(plan):
    points = plan.get("points") or []
    if len(points) < 2:
        raise ValueError("route plan requires at least two points")
    corridor = float(plan.get("corridor_nm", 8.0))
    lats = [float(point["lat"]) for point in points]
    lons = [float(point["lon"]) for point in points]
    middle = sum(lats) / len(lats)
    dy = corridor / 60.0
    dx = corridor / (60.0 * max(0.1, math.cos(math.radians(middle))))
    return min(lons)-dx, min(lats)-dy, max(lons)+dx, max(lats)+dy


def tile_lon_lat_bounds(x, y, zoom):
    count = 1 << zoom
    lon0, lon1 = x/count*360.0-180.0, (x+1)/count*360.0-180.0
    lat0 = math.degrees(math.atan(math.sinh(math.pi*(1-2*y/count))))
    lat1 = math.degrees(math.atan(math.sinh(math.pi*(1-2*(y+1)/count))))
    return lon0, lat1, lon1, lat0


def fallback_tile(directory, x, y, zoom, size=512):
    if not directory: return None
    west, south, east, north = tile_lon_lat_bounds(x, y, zoom)
    latitude, longitude = math.floor((south+north)/2), math.floor((west+east)/2)
    key = f"{'N' if latitude >= 0 else 'S'}{abs(latitude):02d}{'E' if longitude >= 0 else 'W'}{abs(longitude):03d}"
    candidates = sorted((path for path in directory.glob(key+"-sentinel2*.png")
                         if "active" not in path.name),
                        key=lambda path: ("4096" not in path.name, -path.stat().st_size))
    if not candidates: return None
    with Image.open(candidates[0]) as source:
        width, height = source.size
        sx0, sx1 = (west-longitude)*width, (east-longitude)*width
        sy0, sy1 = (latitude+1-north)*height, (latitude+1-south)*height
        crop = source.convert("RGB").crop((sx0, sy0, sx1, sy1))
        return crop.resize((size,size), Image.Resampling.LANCZOS).convert("RGBA")


def atomic_json(path, payload):
    descriptor, temporary = tempfile.mkstemp(prefix=path.name+".", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
            json.dump(payload, stream, indent=2); stream.write("\n")
        os.replace(temporary, path)
    finally:
        Path(temporary).unlink(missing_ok=True)


def compose(plan, cache, output, zoom, max_dimension, fallback_dir=None, explicit_bounds=None):
    west, south, east, north = explicit_bounds or route_bounds(plan)
    left, top = world_pixel(north, west, zoom)
    right, bottom = world_pixel(south, east, zoom)
    tile_size = 512
    x0, y0 = math.floor(left/tile_size), math.floor(top/tile_size)
    x1, y1 = math.floor((right-1)/tile_size), math.floor((bottom-1)/tile_size)
    canvas = Image.new("RGBA", ((x1-x0+1)*tile_size, (y1-y0+1)*tile_size), (22,31,34,255))
    dataset = plan["dataset_id"]
    found = fallback = total = 0
    for y in range(y0, y1+1):
        for x in range(x0, x1+1):
            total += 1
            path = cache / dataset / str(zoom) / str(x) / f"{y}.webp"
            image = None
            if path.is_file():
                with Image.open(path) as tile: image = tile.convert("RGBA")
                found += 1
            else:
                image = fallback_tile(fallback_dir, x, y, zoom, tile_size)
                if image is not None: fallback += 1
            if image is not None: canvas.paste(image, ((x-x0)*tile_size, (y-y0)*tile_size))
    crop = canvas.crop((round(left-x0*tile_size), round(top-y0*tile_size),
                        round(right-x0*tile_size), round(bottom-y0*tile_size)))
    ratio = min(1.0, max_dimension/max(crop.size))
    target = (max(1, round(crop.width*ratio)), max(1, round(crop.height*ratio)))
    if target != crop.size: crop = crop.resize(target, Image.Resampling.LANCZOS)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_suffix(output.suffix+".partial")
    crop.convert("RGB").save(temporary, format="PNG", optimize=True)
    os.replace(temporary, output)
    metadata = {"schema":"efis-next.route-atlas.v1", "dataset_id":dataset,
                "zoom":zoom, "bounds":[west,south,east,north], "width":target[0],
                "height":target[1], "tiles_found":found, "fallback_tiles":fallback, "tiles_expected":total,
                "coverage_ratio":found/total if total else 0, "image":str(output.resolve())}
    atomic_json(output.with_suffix(".json"), metadata)
    return metadata


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--plan",type=Path,required=True); parser.add_argument("--cache",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True); parser.add_argument("--zoom",type=int,default=12)
    parser.add_argument("--max-dimension",type=int,default=4096)
    parser.add_argument("--fallback-dir",type=Path)
    parser.add_argument("--bounds",type=float,nargs=4,metavar=("WEST","SOUTH","EAST","NORTH"))
    args=parser.parse_args()
    result=compose(json.loads(args.plan.read_text()),args.cache,args.output,args.zoom,args.max_dimension,args.fallback_dir,args.bounds)
    print(json.dumps(result))

if __name__=="__main__": main()
