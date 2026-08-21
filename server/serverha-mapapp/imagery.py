"""EFIS imagery mission-package planning for the existing Hangar map server."""
import json, math, os, re, time

CATALOG = "/data/imagery/catalog.json"
IMAGERY_ROOT = "/data/imagery"
PACKAGES = "/plans/imagery"
DEFAULT_DATASET = "north-italy-sentinel2-development"
DEFAULT_TILE_KIB = 90
_SAFE = re.compile(r"[^a-z0-9-]+")


def catalog():
    try:
        with open(CATALOG) as stream:
            return json.load(stream)
    except Exception:
        return {
            "schema": "efis-next.imagery-catalog.v1", "status": "EMPTY",
            "datasets": [], "message": "Imagery archive not ingested"
        }


def manifest_file(dataset_id):
    if dataset_id != DEFAULT_DATASET:
        return None
    path = os.path.realpath(os.path.join(IMAGERY_ROOT, "manifests", dataset_id + ".json"))
    return path if path.startswith(os.path.realpath(IMAGERY_ROOT) + os.sep) and os.path.isfile(path) else None


def tile_file(dataset_id, zoom, x, y):
    if dataset_id != DEFAULT_DATASET or not (0 <= zoom <= 22 and x >= 0 and y >= 0):
        return None
    path = os.path.realpath(os.path.join(IMAGERY_ROOT, "tiles", dataset_id,
                                         str(zoom), str(x), str(y) + ".webp"))
    return path if path.startswith(os.path.realpath(IMAGERY_ROOT) + os.sep) and os.path.isfile(path) else None


def hybrid_style():
    """Compact MapLibre style: Sentinel base with selective OSM/OpenAIP overlays."""
    tile_template = f"/api/imagery/tile/{DEFAULT_DATASET}/{{z}}/{{x}}/{{y}}.webp"
    return {
        "version": 8, "name": "EFIS Hybrid Development",
        "metadata": {"efis:status": "DEVELOPMENT", "efis:imagery": DEFAULT_DATASET},
        "center": [12.22, 45.90], "zoom": 9,
        "sources": {
            "sentinel": {"type": "raster", "tiles": [tile_template], "tileSize": 512,
                         "minzoom": 8, "maxzoom": 14,
                         "attribution": "Contains modified Copernicus Sentinel data 2025"},
            "openmaptiles": {"type": "vector", "url": "/data/v3.json",
                             "attribution": "© OpenStreetMap contributors"},
            "airspaces": {"type": "geojson", "data": "/api/geojson/airspaces",
                          "attribution": "OpenAIP • DEVELOPMENT"}
        },
        "layers": [
            {"id":"background","type":"background","paint":{"background-color":"#10202a"}},
            {"id":"sentinel","type":"raster","source":"sentinel",
             "paint":{"raster-opacity":0.94,"raster-saturation":-0.08,"raster-contrast":0.10}},
            {"id":"water","type":"fill","source":"openmaptiles","source-layer":"water",
             "paint":{"fill-color":"#2c8db8","fill-opacity":0.24}},
            {"id":"roads-major","type":"line","source":"openmaptiles","source-layer":"transportation",
             "filter":["in","class","motorway","trunk","primary","secondary"],
             "paint":{"line-color":"#f4d170","line-opacity":0.68,
                      "line-width":["interpolate",["linear"],["zoom"],8,0.4,14,2.2]}},
            {"id":"buildings","type":"fill","source":"openmaptiles","source-layer":"building","minzoom":13,
             "paint":{"fill-color":"#d8dce0","fill-opacity":0.22,"fill-outline-color":"#7f949e"}},
            {"id":"runways","type":"line","source":"openmaptiles","source-layer":"aeroway",
             "filter":["==","class","runway"],
             "paint":{"line-color":"#ffffff","line-opacity":0.92,
                      "line-width":["interpolate",["linear"],["zoom"],10,1.5,15,10]}},
            {"id":"airspace-fill","type":"fill","source":"airspaces",
             "paint":{"fill-color":"#b155d6","fill-opacity":0.055}},
            {"id":"airspace-boundary","type":"line","source":"airspaces",
             "paint":{"line-color":"#e06cff","line-opacity":0.80,"line-width":1.4,
                      "line-dasharray":[3,2]}}
        ]
    }


def _tile_xy(lat, lon, zoom):
    lat = max(-85.05112878, min(85.05112878, float(lat)))
    count = 1 << zoom
    x = int((float(lon) + 180.0) / 360.0 * count)
    y = int((1.0 - math.asinh(math.tan(math.radians(lat))) / math.pi) * 0.5 * count)
    return max(0, min(count - 1, x)), max(0, min(count - 1, y))


def _bounds_tiles(west, south, east, north, zoom):
    x0, y0 = _tile_xy(north, west, zoom)
    x1, y1 = _tile_xy(south, east, zoom)
    return {(zoom, x, y) for y in range(min(y0, y1), max(y0, y1) + 1)
            for x in range(min(x0, x1), max(x0, x1) + 1)}


def _circle_bbox(lat, lon, radius_nm):
    dy = radius_nm / 60.0
    dx = radius_nm / (60.0 * max(0.1, math.cos(math.radians(lat))))
    return lon - dx, lat - dy, lon + dx, lat + dy


def _route_samples(points, step_nm=1.0):
    samples = []
    for a, b in zip(points, points[1:]):
        lat1, lon1, lat2, lon2 = float(a["lat"]), float(a["lon"]), float(b["lat"]), float(b["lon"])
        north = (lat2 - lat1) * 60.0
        east = (lon2 - lon1) * 60.0 * math.cos(math.radians((lat1 + lat2) / 2))
        count = max(1, math.ceil(math.hypot(north, east) / step_nm))
        samples.extend((lat1 + (lat2-lat1)*i/count, lon1 + (lon2-lon1)*i/count) for i in range(count))
    samples.append((float(points[-1]["lat"]), float(points[-1]["lon"])))
    return samples


def _entries(tiles, dataset=DEFAULT_DATASET, extension="webp"):
    return [{"z": z, "x": x, "y": y, "priority": "MANDATORY",
             "path": f"{dataset}/{z}/{x}/{y}.{extension}",
             "url": f"/api/imagery/tile/{dataset}/{z}/{x}/{y}.{extension}"}
            for z, x, y in sorted(tiles)]


def _finish(mode, selection_id, tiles, zooms, extra=None):
    requested_tiles = sorted(tiles)
    dataset_root = os.path.join(IMAGERY_ROOT, "tiles", DEFAULT_DATASET)
    inventory_active = os.path.isdir(dataset_root)
    available_tiles = [tile for tile in requested_tiles
                       if not inventory_active or tile_file(DEFAULT_DATASET, *tile)]
    entries = _entries(available_tiles)
    missing_count = len(requested_tiles) - len(available_tiles)
    result = {
        "schema": "efis-next.mission-imagery-package.v1", "selection_mode": mode,
        "selection_id": selection_id, "dataset_id": DEFAULT_DATASET,
        "created_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "zooms": sorted(zooms), "tile_count": len(entries),
        "requested_tile_count": len(requested_tiles),
        "coverage_gap_tile_count": missing_count,
        "coverage_complete": missing_count == 0,
        "estimated_bytes": len(entries) * DEFAULT_TILE_KIB * 1024,
        "state": "PLANNED", "offline_ready": False, "tiles": entries,
    }
    result.update(extra or {})
    return result


def area_plan(payload):
    zooms = [int(z) for z in payload.get("zooms", [11, 12, 13, 14])]
    if payload.get("bbox"):
        west, south, east, north = [float(value) for value in payload["bbox"]]
    elif payload.get("center") and payload.get("radius_nm"):
        lat, lon = [float(value) for value in payload["center"]]
        west, south, east, north = _circle_bbox(lat, lon, float(payload["radius_nm"]))
    else:
        return {"error": "bbox or center/radius_nm required"}
    if not (6.5 <= west < east <= 14.0 and 43.4 <= south < north <= 47.1):
        return {"error": "area outside North Italy coverage"}
    tiles = set()
    for zoom in zooms:
        tiles.update(_bounds_tiles(west, south, east, north, zoom))
    return _finish("AREA", payload.get("name", "selected-area"), tiles, zooms,
                   {"bounds": [west, south, east, north]})


def route_plan(payload):
    points = payload.get("points") or []
    if len(points) < 2:
        return {"error": "at least two route points required"}
    zooms = [int(z) for z in payload.get("zooms", [11, 12, 13, 14])]
    corridor = max(1.0, min(30.0, float(payload.get("corridor_nm", 8.0))))
    tiles = set()
    for lat, lon in _route_samples(points):
        for zoom in zooms:
            cx, cy = _tile_xy(lat, lon, zoom)
            metres = 40075016.686 * math.cos(math.radians(lat)) / (1 << zoom)
            radius = max(1, math.ceil(corridor * 1852.0 / metres))
            for dy in range(-radius, radius + 1):
                for dx in range(-radius, radius + 1):
                    if dx*dx + dy*dy <= (radius + 0.35)**2:
                        tiles.add((zoom, cx+dx, cy+dy))
    return _finish("FLIGHT_PLAN", payload.get("name", "flight-plan"), tiles, zooms,
                   {"corridor_nm": corridor, "points": points})


def save(package):
    if package.get("error") or not package.get("tiles"):
        return {"error": "valid planned package required"}
    os.makedirs(PACKAGES, exist_ok=True)
    package_id = _SAFE.sub("-", package.get("selection_id", "package").lower()).strip("-")[:60] or "package"
    package = dict(package)
    package["package_id"] = package_id
    temporary = os.path.join(PACKAGES, "." + package_id + ".tmp")
    destination = os.path.join(PACKAGES, package_id + ".json")
    with open(temporary, "w") as stream:
        json.dump(package, stream, separators=(",", ":"))
    os.replace(temporary, destination)
    return {"package_id": package_id, "state": package["state"], "tile_count": package["tile_count"]}


def packages():
    if not os.path.isdir(PACKAGES): return []
    result = []
    for name in sorted(os.listdir(PACKAGES)):
        if not name.endswith(".json"): continue
        try:
            with open(os.path.join(PACKAGES, name)) as stream: item = json.load(stream)
            result.append({key: item.get(key) for key in ("package_id", "selection_id", "selection_mode", "state", "tile_count", "estimated_bytes", "created_utc")})
        except Exception: pass
    return result
