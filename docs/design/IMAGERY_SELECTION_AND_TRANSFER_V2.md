# EFIS NEXT Imagery Selection and Transfer V2

## Operational objective

The EFIS receives a self-contained, versioned mission package before flight. Two selection methods are enabled first, but both feed one transfer, validation and cache pipeline.

## Method A — Area selection

The pilot selects a named area, rectangle or centre/radius. Setup shows coverage, zoom levels, estimated size, available storage and expected download time before confirmation. Area packages are useful for local flying, training, patrol and flights without a formal route.

Initial presets:

- airport terminal area: 15–25 NM, zoom through 15;
- local operating area: 40–80 NM, zoom through 14;
- North Italy baseline: complete zoom 8–12, higher detail on demand.

## Method B — Flight-plan selection

The route is expanded by a configurable corridor, initially 8 NM. Departure, destination and alternates receive higher priority and terminal detail. Route amendments produce a delta plan; existing tiles are reused rather than downloaded again.

## Single mission-package lifecycle

1. `PLANNED`: selection is converted to tile identifiers and size estimate.
2. `APPROVED`: pilot confirms storage/network use.
3. `DOWNLOADING`: resumable background transfer with telemetry priority.
4. `VERIFYING`: type, length and checksum are checked.
5. `OFFLINE_READY`: all mandatory tiles are locally available.
6. `PARTIAL`: lower-resolution fallback is available but requested detail is incomplete.
7. `STALE`: a newer catalogue exists; current package remains usable with explicit status.

## Professional extensions

### Automatic terminal enrichment

Always include departure, destination and selected alternates at the maximum available resolution, plus runway geometry and obstacles. This avoids manually drawing several areas around a route.

### Dynamic look-ahead

While connected, request tiles ahead of aircraft heading and groundspeed. These are opportunistic and can never delay flight telemetry or render work.

### Diversion envelope

Prepare a lower-resolution safety envelope around the route and higher detail around reachable alternate airports. This is more operationally useful than a narrow corridor alone.

### Progressive quality

Download in this order: regional overview, route baseline, terminal areas, route high detail, optional surrounding detail. The EFIS becomes usable early and exposes a precise readiness state.

### Fleet content profiles

Server policies can differ by aircraft, display storage and mission type. The same source catalogue can serve Standard and Professional profiles without duplicating imagery.

## Recommended initial policy

- North Italy server archive: zoom 8–14, terminal zones zoom 15.
- RPi baseline: North Italy zoom 8–11 permanently cached.
- Per-flight package: zoom 12–14 along an 8 NM corridor.
- Terminal areas: zoom 15 within 20 NM of departure, destination and alternates.
- Local-area mode: selected radius plus one lower-resolution safety ring.
- Cache: 20 GiB default, 5 GiB minimum free space, mission package pinned until flight closure.

## Required Setup pages

1. `MAP DATA`: dataset edition, coverage, storage and server reachability.
2. `SELECT AREA`: map rectangle, centre/radius and named presets.
3. `FLIGHT PLAN`: route, corridor, airports and alternates.
4. `TRANSFER`: size, progress, failures and pause/resume.
5. `READINESS`: mandatory layers and `OFFLINE READY` decision.

The imagery remains supplementary development data. Terrain, obstacle and aeronautical packages retain independent provenance and assurance states.
