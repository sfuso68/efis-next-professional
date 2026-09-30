# Weather radar and hotspot connectivity V1

## Operational boundary

Weather is supplementary situational-awareness data. Loss of internet, Radar-DPC, cache or
NetworkManager must never affect primary flight, engine, recording or navigation functions.
Every weather presentation shows source, age and an explicit `NO DATA` state; stale imagery
must not be presented as current.

## Public sources

- Primary precipitation product: Italian Civil Protection Radar-DPC `radar:vmi`, WMS/WMTS,
  nominal five-minute update, CC-BY-SA attribution.
- Aviation text products: AviationWeather.gov METAR/TAF API, queried at conservative intervals.
- Future layers: Radar-DPC SRI, lightning and CAPPI. They remain separately selectable because
  reflectivity, surface rainfall and altitude slices have different meanings.

## Runtime design

`WeatherRadarOverlay.qml` is the first reusable presentation layer. NAV and Canvas receive a
geographic overlay; PFD receives a deliberately bounded inset so weather cannot obscure the
attitude, airspeed or altitude instruments. Qt performs asynchronous image loading and caching.
The current Veneto preview uses a Radar-DPC Web Mercator tile aligned to GeoWebCache; arbitrary
WMS bounding boxes are rejected by that service. Moving-map integration will request the exact
set of aligned tiles intersecting the current viewport.

The production service will own downloads outside the render loop, retain the last valid frame
atomically, enforce age thresholds (CURRENT <= 10 min, STALE 11-20 min, EXPIRED > 20 min), and
publish a small immutable snapshot to every display. The edge text cache now applies the same
thresholds to METAR/TAF, derives age from the cache record's `fetched_utc` rather than mutable
file mtime, rejects future timestamps as `CLOCK_SKEW`, rejects malformed schema and confines
station identifiers to four ASCII alphanumeric characters. Stale data may remain visible only
with its state and age; weather is always marked ineligible for safety authority.

Radar cache identity includes product, zoom and tile coordinates. A position or product change
may load only the exact matching cached frame; otherwise the image is cleared to `NO DATA`.
Network replies retain their original request identity and may update the visible frame only
while that product/tile is still current. Superseded replies may populate their own cache entry
but cannot be relabelled or displayed as the newly selected product or geographic tile. Product
names are restricted to the explicit VMI/SRI/CAPPI 3000 allowlist and negative tile coordinates
or path-like product strings fail closed before cache path construction. CAPPI 3000 remains a
training-only selection in the current implementation; leaving simulation forces a supported
live product instead of issuing a mislabeled live request.

## Hotspot policy

The Raspberry Pi connects to a phone hotspot through a saved NetworkManager connection profile.
The EFIS may scan networks and activate/deactivate saved profiles. Passwords are never placed in
command-line arguments, logs, configuration tracked by Git, QML properties or telemetry. New
credentials are entered through NetworkManager's secret agent/setup workflow while on ground.

Network availability is an optional service. Acquisition uses bounded timeouts, backoff and a
maximum refresh rate; no automatic network change is allowed in flight.

## Next increments

1. C++ acquisition/cache service with TLS, provenance and deterministic stale-state tests.
2. Setup > Connectivity page for saved profiles, signal level, IP and guarded on-ground connect.
3. WMS tiles matched to the actual moving-map projection and current map extent.
4. METAR/TAF/SIGMET cards and nearest-airport selection.
5. Radar animation, lightning and CAPPI layers with memory/CPU budgets validated on RPi5.
