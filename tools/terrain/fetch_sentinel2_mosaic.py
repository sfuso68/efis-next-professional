#!/usr/bin/env python3
"""Fetch four Sentinel-2 quadrants and assemble one high-detail tile."""
import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bbox", nargs=4, type=float, required=True,
                        metavar=("WEST", "SOUTH", "EAST", "NORTH"))
    parser.add_argument("--quadrant-size", type=int, default=2048)
    args = parser.parse_args()
    if not os.environ.get("CDSE_ACCESS_TOKEN", "").strip():
        raise SystemExit("CDSE_ACCESS_TOKEN is not set")
    west, south, east, north = args.bbox
    mid_lon = (west + east) / 2.0
    mid_lat = (south + north) / 2.0
    fetcher = Path(__file__).with_name("fetch_sentinel2_texture.py")
    # PIL rows run north to south.
    quadrants = [
        ("nw", (west, mid_lat, mid_lon, north), (0, 0)),
        ("ne", (mid_lon, mid_lat, east, north), (args.quadrant_size, 0)),
        ("sw", (west, south, mid_lon, mid_lat), (0, args.quadrant_size)),
        ("se", (mid_lon, south, east, mid_lat), (args.quadrant_size, args.quadrant_size)),
    ]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="efis-s2-") as temporary:
        temporary_path = Path(temporary)
        mosaic = Image.new("RGBA", (args.quadrant_size * 2, args.quadrant_size * 2))
        for name, bbox, position in quadrants:
            target = temporary_path / f"{name}.png"
            subprocess.run([
                sys.executable, str(fetcher), "--output", str(target),
                "--bbox", *map(str, bbox), "--size", str(args.quadrant_size)
            ], check=True)
            with Image.open(target) as source:
                mosaic.paste(source.convert("RGBA"), position)
        mosaic.save(args.output, optimize=True)
    print(f'{{"output":"{args.output}","width":{mosaic.width},"height":{mosaic.height}}}')


if __name__ == "__main__":
    main()
