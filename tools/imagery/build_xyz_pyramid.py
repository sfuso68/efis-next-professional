#!/usr/bin/env python3
"""Georeference Sentinel source mosaics and build an XYZ WebP pyramid with GDAL."""
import argparse
import json
import re
import shutil
import subprocess
from pathlib import Path

SOURCE = re.compile(r"^(N|S)(\d{2})(E|W)(\d{3})-sentinel2(?:-\d+)?\.png$")


def coordinate(hemisphere, value):
    number = int(value)
    return number if hemisphere in ("N", "E") else -number


def run(command):
    subprocess.run([str(item) for item in command], check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--zooms", default="8-14")
    parser.add_argument("--processes", type=int, default=4)
    parser.add_argument("--quality", type=int, default=86)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    required = ("gdal_translate", "gdalbuildvrt", "gdal2tiles.py")
    missing = [tool for tool in required if not shutil.which(tool)]
    if missing:
        raise SystemExit("missing GDAL tools: " + ", ".join(missing))
    sources = args.root / "sources"
    work = args.root / "work"
    georeferenced = work / "georeferenced"
    output = args.root / "tiles" / "north-italy-sentinel2-development"
    if output.exists() and any(output.iterdir()) and not args.force:
        raise SystemExit(f"output is not empty: {output}; use --force only for an explicit rebuild")
    georeferenced.mkdir(parents=True, exist_ok=True)
    inputs = []
    for source in sorted(sources.glob("*-sentinel2*.png")):
        match = SOURCE.match(source.name)
        if not match:
            continue
        south = coordinate(match.group(1), match.group(2))
        west = coordinate(match.group(3), match.group(4))
        target = georeferenced / (source.stem + ".tif")
        run(["gdal_translate", "-q", "-of", "GTiff", "-a_srs", "EPSG:4326",
             "-a_ullr", west, south + 1, west + 1, south, source, target,
             "-co", "TILED=YES", "-co", "COMPRESS=DEFLATE", "-co", "BIGTIFF=IF_SAFER"])
        inputs.append(target)
    if not inputs:
        raise SystemExit("no Sentinel source mosaics found")
    vrt = work / "north-italy-sentinel2.vrt"
    run(["gdalbuildvrt", "-q", "-overwrite", vrt, *inputs])
    output.mkdir(parents=True, exist_ok=True)
    run(["gdal2tiles.py", "--xyz", "--profile=mercator", "--resampling=bilinear",
         f"--zoom={args.zooms}", "--tilesize=512", "--tiledriver=WEBP",
         f"--webp-quality={max(1, min(100, args.quality))}",
         f"--processes={max(1, args.processes)}", "--webviewer=none", vrt, output])
    tile_count = sum(1 for path in output.rglob("*.webp") if path.is_file())
    byte_count = sum(path.stat().st_size for path in output.rglob("*.webp") if path.is_file())
    report = {"schema": "efis-next.imagery-pyramid-report.v1", "zooms": args.zooms,
              "source_mosaics": len(inputs), "tile_count": tile_count,
              "bytes": byte_count, "format": "WEBP", "tile_size": 512,
              "quality": args.quality, "xyz": True}
    (work / "last-pyramid-build.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
