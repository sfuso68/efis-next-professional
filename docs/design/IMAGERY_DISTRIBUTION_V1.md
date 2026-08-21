# EFIS NEXT Imagery Distribution V1

## Objective

Store North Italy imagery once on an EFIS server and deliver only the tiles required by the selected flight plan or current viewing area. The display remains fully usable offline and never depends on a live connection during flight.

## Components

1. **Ingestion worker** acquires licensed source imagery, normalises colour and cloud policy, creates immutable versions and never exposes provider credentials to aircraft.
2. **Tile builder** generates 512 px WebP/AVIF XYZ pyramids for zoom 8–15 plus DEM/obstacle packages with checksums.
3. **Object store** holds versioned tiles. Initial deployment may use MinIO on the mini-PC; production can use S3-compatible storage with a CDN.
4. **Catalogue API** publishes coverage, edition, effective date, attribution, hashes and package status.
5. **Flight-plan prefetcher** expands the route by an 8 NM corridor and downloads zoom 11–14 before departure. Airport terminal areas add zoom 15.
6. **Runtime cache** on RPi5 reads only local files. A background service downloads missing look-ahead tiles and atomically publishes them after hash verification.

## Safety and availability

- No network access occurs on the render thread.
- A lost hotspot never removes already cached terrain.
- Missing high-resolution imagery falls back to lower zoom, then to the regional base texture.
- DEM, imagery, obstacles and aeronautical data have separate provenance and certification states.
- The server cannot silently replace an active dataset; activation uses a signed/versioned manifest and atomic pointer update.
- Provider tokens remain only on the ingestion server and are never stored on an RPi display.

## Storage profile for North Italy

- Zoom 8–11: complete regional baseline.
- Zoom 12–13: complete operational area.
- Zoom 14: flight corridors and commonly used areas.
- Zoom 15: airport terminal zones and explicitly requested view areas.
- Expected storage depends on cloud filtering and encoding; quotas and actual tile statistics must be measured before hardware sizing.

## Network policy

- On the ground: Wi-Fi/LAN preflight synchronisation is allowed.
- In flight: hotspot download is optional and low priority; flight data and telemetry always take precedence.
- Default cache target: 20 GB imagery plus pinned current flight plan.
- Eviction: least-recently-used tiles outside pinned routes; departure, destination and current corridor cannot be evicted.

## Delivery phases

1. Local mini-PC proof of concept with HTTPS, object storage and static manifests.
2. RPi cache daemon with route planning, resumable downloads, hashes and storage quota.
3. Setup UI showing coverage, download size, progress, version and offline readiness.
4. North Italy ingestion and performance test.
5. Redundant server/CDN and signed production catalogues.

All current imagery is `DEVELOPMENT` and unsuitable as an authorised navigation source.
