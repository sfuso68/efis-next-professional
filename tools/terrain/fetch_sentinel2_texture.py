#!/usr/bin/env python3
"""Fetch a georeferenced Sentinel-2 L2A true-colour texture from CDSE."""
import argparse
import json
import os
import urllib.error
import urllib.request
from pathlib import Path

ENDPOINT = "https://sh.dataspace.copernicus.eu/api/v1/process"
EVALSCRIPT = """//VERSION=3
function setup() {
  return {input:[{bands:[\"B02\",\"B03\",\"B04\",\"dataMask\"]}],
          output:{bands:4, sampleType:\"AUTO\"}};
}
function evaluatePixel(s) {
  const gain=2.5;
  return [gain*s.B04, gain*s.B03, gain*s.B02, s.dataMask];
}
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bbox", nargs=4, type=float, default=[12.0, 46.0, 13.0, 47.0])
    parser.add_argument("--from-date", default="2025-06-01T00:00:00Z")
    parser.add_argument("--to-date", default="2025-09-30T23:59:59Z")
    parser.add_argument("--size", type=int, default=2048)
    args = parser.parse_args()
    token = os.environ.get("CDSE_ACCESS_TOKEN", "").strip()
    if not token:
        raise SystemExit("CDSE_ACCESS_TOKEN is not set")
    payload = {
        "input": {
            "bounds": {"bbox": args.bbox, "properties": {"crs": "http://www.opengis.net/def/crs/OGC/1.3/CRS84"}},
            "data": [{"type": "sentinel-2-l2a",
                      "dataFilter": {"timeRange": {"from": args.from_date, "to": args.to_date},
                                     "maxCloudCoverage": 15,
                                     "mosaickingOrder": "leastCC"}}]
        },
        "output": {"width": args.size, "height": args.size,
                   "responses": [{"identifier": "default", "format": {"type": "image/png"}}]},
        "evalscript": EVALSCRIPT
    }
    request = urllib.request.Request(ENDPOINT, data=json.dumps(payload).encode(), method="POST",
                                     headers={"Authorization": f"Bearer {token}",
                                              "Content-Type": "application/json",
                                              "Accept": "image/png"})
    try:
        with urllib.request.urlopen(request, timeout=180) as response:
            content = response.read()
    except urllib.error.HTTPError as error:
        detail = error.read().decode("utf-8", "replace")[:500]
        raise SystemExit(f"CDSE Process API failed: HTTP {error.code}: {detail}") from error
    if not content.startswith(b"\x89PNG"):
        raise SystemExit("CDSE response is not a PNG texture")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(content)
    print(json.dumps({"output": str(args.output), "bytes": len(content), "bbox": args.bbox}))


if __name__ == "__main__":
    main()
