# EFIS NEXT — GPU Map V1

## Purpose

GPU Map V1 is the first native map-rendering slice for the new EFIS NEXT
codebase. It establishes the rendering path and visual hierarchy for Mission
View without reusing the legacy EFIS implementation.

## Rendering architecture

- `GpuMapItem` is a native Qt Quick Scene Graph item.
- Static and dynamic vector layers are submitted as scene-graph geometry and
  rendered by the Qt RHI backend on the Raspberry Pi GPU.
- `GpuMapSurface.qml` composes the native surface with labels, layer state,
  range information and touch-oriented controls.
- `Theme.qml` is the initial shared token set for colour, spacing, typography,
  borders and semantic status states.

This keeps the presentation layer adaptable while allowing performance-critical
graphics to move into native render nodes when required.

## V1 layers

- dark navigation background and reference grid;
- synthetic terrain polygon;
- range rings;
- synthetic cyan route and waypoint symbols;
- ownship symbol;
- explicit state for terrain, airspace, traffic and weather layers.

All generated information is labelled `SIM` or `SYNTHETIC`. A layer without an
authoritative source is shown as `OFF`, `NOT AVAILABLE` or `UNVERIFIED`; the UI
must never present invented operational data as valid.

## Data boundary

The renderer does not own navigation logic. Future adapters will provide
immutable map snapshots containing:

- aircraft position, track, heading and uncertainty;
- flight plan and active leg;
- terrain and obstacle tiles with provenance and age;
- controlled airspace;
- traffic targets;
- weather products with timestamp and validity.

The Mission View remains usable when individual layers are unavailable and must
show their degraded state explicitly.

## Quality gate

The 1024 x 600 Raspberry Pi target must maintain:

- stable full-screen Wayland operation;
- no thermal throttling during the test;
- readable primary information at normal seated distance;
- no QML runtime errors;
- deterministic degraded-mode annunciation;
- recovery to the normal PFD service after a preview or failure.

## Next increment

1. Replace synthetic geometry with a versioned `MapSnapshot` interface.
2. Add clipping, transforms, map orientation and declutter levels.
3. Add an offline vector-tile cache and terrain database adapter.
4. Add real flight-plan legs from the navigation domain.
5. Add GPU timing, frame-time telemetry and visual regression captures.

