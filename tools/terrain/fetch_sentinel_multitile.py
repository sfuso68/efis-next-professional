#!/usr/bin/env python3
"""Fetch the Sentinel-2 texture set required by the current 3D LOD region."""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

TILES = {
    "N45E011": (11, 45, 12, 46),
    "N45E012": (12, 45, 13, 46),
    "N45E013": (13, 45, 14, 46),
    "N46E011": (11, 46, 12, 47),
    "N46E012": (12, 46, 13, 47),
    "N46E013": (13, 46, 14, 47),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("data/terrain/gpu"))
    parser.add_argument("--size", type=int, default=2048)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    if not os.environ.get("CDSE_ACCESS_TOKEN", "").strip():
        raise SystemExit("CDSE_ACCESS_TOKEN is not set in this terminal")
    fetcher = Path(__file__).with_name("fetch_sentinel2_texture.py")
    results = []
    for tile, bbox in TILES.items():
        target = args.output / f"{tile}-sentinel2.png"
        if target.exists() and target.stat().st_size > 100_000 and not args.force:
            results.append({"tile": tile, "status": "reused", "bytes": target.stat().st_size})
            continue
        command = [sys.executable, str(fetcher), "--output", str(target), "--bbox",
                   *map(str, bbox), "--size", str(args.size)]
        subprocess.run(command, check=True)
        results.append({"tile": tile, "status": "downloaded", "bytes": target.stat().st_size})
    print(json.dumps({"textures": results, "complete": len(results) == len(TILES)}))


if __name__ == "__main__":
    main()
