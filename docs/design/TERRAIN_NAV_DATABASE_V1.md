# Terrain and Navigation Database V1

## Architecture

The flight renderer never parses source datasets. An offline build pipeline validates and converts them into immutable, versioned packages. The runtime opens a compact SQLite catalogue and memory-mapped terrain/obstacle tiles; a small JSON index supports the setup UI.

Layers are independent:

1. terrain: tiled DEM, multiresolution LOD, checksum and vertical datum;
2. obstacles: spatially indexed points/polygons with MSL and AGL height;
3. airports/runways: AIXM-compatible identity and threshold geometry;
4. runtime cache: nearest airports, active airport and tiles around the aircraft.

Every package records source, edition, effective dates, licence, checksum and status. Development/open data is never presented as authorised aeronautical data.

## Runtime targets

- no file or SQL work on the render thread;
- background tile loading and double-buffered scene snapshots;
- 60 fps display target, 30 fps minimum under tile transitions;
- nearest-airport query below 20 ms for the loaded region;
- deterministic fallback when a tile or obstacle layer is missing;
- terrain/obstacle/runway geometry sharing one WGS84/ENU camera transform.

## Implementation checkpoint — 2026-08-18

- HGT loading moved to Qt Concurrent; procedural terrain remains active until a validated tile is published.
- Adaptive mesh LOD uses 56×40 near/low and 48×34 far/high profiles.
- Open obstacle samples are indexed into 0.1° geographic cells; the renderer queries only the 5×5 neighbourhood.
- TAWS simulation colours real DEM vertices red below 30 m clearance and amber below 300 m clearance.
- RPi5 measured render health after optimisation: approximately 34–35 fps at 1024×600.
- `REAL DEM`, `TAWS SIM`, `NON-CERT` and data provenance remain visible in the UI.

### Multi-tile checkpoint

- Tile keys are resolved automatically from WGS84 position (`N45E012` format).
- Active and look-ahead tiles load concurrently without blocking the render thread.
- A four-entry LRU cache bounds terrain memory usage; active data is never evicted.
- Prefetch looks 25 km ahead and is recalculated only when the aircraft enters a new 15° heading sector.
- Crossing into an unavailable or pending tile uses the procedural fallback instead of blank or stale geometry.
- The active tile and cache count are visible in the PFD provenance label.
- Elevation sampling has a direct-buffer fast path for the active tile; hash lookup is restricted to tile borders.
- Stable RPi5 render health after the multi-tile optimisation: 34.1–34.5 fps, approximately 29 ms/frame.

### Versioned terrain-package checkpoint

- `tools/terrain/build_terrain_package.py` validates 3601×3601 HGT tiles and can convert a single-band GeoTIFF into the runtime big-endian Int16 format.
- Every immutable package contains a manifest with package identity, source, edition, attribution, status, vertical datum, geographic bounds, byte size and SHA-256 for every tile.
- `tools/terrain/install_terrain_package.py` rejects missing, truncated or checksum-mismatched tiles before installation.
- Installation writes into `data/terrain/packages/<package_id>` and changes `data/terrain/active-package.json` atomically only after full validation.
- Runtime resolves the active manifest without blocking the render path; `EFIS_TERRAIN_ROOT` remains available as an explicit development override.
- The PFD provenance label exposes package ID, active tile, cache occupancy and `NON-CERT` status.
- Active development package: `veneto-dev-2026-08-18`, Mapzen archive source, vertical datum `UNKNOWN`, SHA-256 verified on the RPi5. It is simulation data and must be replaced before any operational use.
- RPi5 validation: build and core tests passed; active package loaded successfully; initial measured render health was 45 fps at 1024×600.

### Acceptance gate for a Copernicus package

1. Download through an authenticated and licence-compliant source; preserve the original product identifier and acquisition metadata.
2. Record `EGM2008` explicitly and validate elevations against surveyed or authoritative checkpoints in the operating region.
3. Produce all tiles covering the configured operational envelope plus a defined safety margin; reject holes and duplicate tile keys.
4. Verify hashes again on the target computer before atomic activation.
5. Run boundary-crossing, missing-tile, cold-cache and sustained-frame-rate tests on the RPi5.
6. Keep the package `DEVELOPMENT`/`NON-CERT` until terrain, obstacles and aeronautical data have each passed their independent assurance process.

Official acquisition is performed through Copernicus Data Space Ecosystem using either authenticated OData for filtered products or S3-compatible storage for bulk delivery. The intended collection is `COP-DEM_GLO-30-DGED`; the runtime consumes derived tiles only and never stores access credentials. Copernicus source identifiers, release, required attribution and licence acceptance must be preserved in the package manifest. Authentication secrets belong in the operator environment or an OS credential store, never in this repository or on the display image.

### Copernicus acquisition checkpoint

- `tools/terrain/fetch_copernicus_dem.py` queries the official OData `COP-DEM` collection by an EPSG:4326 operating-area polygon.
- The default Veneto envelope is `10.6,44.8,13.2,46.7`; it is configurable and intentionally larger than the Treviso local test area.
- Results are restricted to the `COP-DEM_GLO-30-DGED` GeoTIFF delivery, excluding the parallel DTED products.
- Discovery is unauthenticated and writes `discovery.json`; after strict positive-area intersection and latest-delivery deduplication, the live catalogue returned 12 unique DGED tiles on 2026-08-18.
- Download is an explicit second operation, requires `CDSE_ACCESS_TOKEN`, supports HTTP range resume and writes only to staging.
- Credentials are read from the process environment and are never prompted, printed, persisted or embedded in source/configuration files.
- Downloaded products are not unpacked, converted, installed or activated automatically. Each transition remains a separate validation gate.
- Copernicus delivery filenames are now mapped to normalized runtime keys such as `N45E012` by the package builder.
- `tools/terrain/prepare_copernicus_stage.py` rejects absolute paths, traversal entries, symbolic links, oversized members, malformed ZIPs and products without exactly one DEM GeoTIFF.
- The preparation report lists source product IDs and hashes every extracted GeoTIFF; readiness remains false until all 12 expected cells are present.

### Copernicus release correction — 2026-08-18

- The first OData selection resolved legacy catalogue records whose `$value` archives contained only quicklook JPEG and INSPIRE metadata. Their small size (approximately 80–222 kB) exposed the issue before conversion; no raster was installed or activated.
- Catalogue selection now requires both dataset `COP-DEM_GLO-30-DGED/2024_1` and product type `SAR_DGE_30_A4AD`, following the current CCM catalogue attributes.
- The corrected query returns 12 unique Veneto cells with 2024 publication metadata and S3 paths under `/eodata/CCM/COP-DEM_GLO-30-DGED/SAR_DGE_30_A4AD/`.
- Legacy metadata-only downloads remain quarantined evidence and must not be supplied to the package builder.
- Download completion now includes an internal ZIP audit. A product is accepted only when it contains exactly one `_DEM.tif`/`_DEM.tiff`; metadata-only, ambiguous and invalid archives produce distinct rejected states and a non-zero process exit.
- `--audit-existing` verifies a staging directory without downloading or requiring a token and writes `payload-audit.json`. The empty 2024_1 staging area correctly reports all 12 cells as `MISSING` and fails closed.
- Existing valid archives are reused after payload inspection, avoiding unnecessary repeated transfers when a short-lived token is renewed.

### Resolved follow-up

- The maintenance overlay now guards the staged aircraft and speed map during startup; the RPi5 restart log no longer reports undefined V-speed bindings.

### Copernicus Veneto runtime validation — 2026-08-19

- All 12 `COP-DEM_GLO-30-DGED/2024_1` products passed the strict archive audit with exactly one DEM GeoTIFF per product.
- Safe preparation completed with `ready=true`, 12 prepared cells and no missing cells.
- Package `cop-dem-glo30-veneto-2024_1` contains 12 checksum-verified 3601×3601 HGT tiles, declares `EGM2008` and remains `DEVELOPMENT` / `NON-CERT`.
- The package was installed atomically on the RPi5. The previous Mapzen development package remains installed for explicit rollback.
- The LIPH sample is 16 m MSL versus an 18 m reference, within the 10 m development acceptance band.
- `validate_terrain_package.py` verified every tile hash and all 17 shared boundaries; the maximum boundary discontinuity was 0 m.
- A renderer fault test with an unavailable active/look-ahead tile produced explicit `terrain_tile_unavailable` events and retained the procedural fallback at approximately 32 fps.
- Package selection is now authoritative: when a versioned package or explicit terrain root is configured, the renderer cannot silently substitute legacy Mapzen assets.
- On the real Wayland display, cold renderer cache loaded `N45E012`, prefetched `N45E011` and then sustained approximately 32–33 fps; the first measured interval reached 44 fps.
- ARM build, C++ core tests and six terrain-tool tests all passed after the fallback correction.

Remaining assurance work: repeat surveyed elevation checks across the operating area, exercise an actual simulated aircraft crossing between cells, measure a longer thermal/frame-time run, and qualify the independent obstacle layer. These items prevent promotion beyond `DEVELOPMENT`.

## Data policy

- Terrain baseline: Copernicus DEM GLO-30, preserving attribution and EGM2008 metadata.
- Operational aeronautical data: authorised ENAV/EAD delivery, preferably AIXM 5.1.
- Obstacles: authorised eTOD/AIP source for operational use.
- Open or hand-entered data: development/simulation only.

Treviso in the seed database is intentionally marked `DEVELOPMENT`; its geometry must not be used for navigation or obstacle clearance.
