# EFIS NEXT — PFD Professional V1

## Status

This document defines the first visual quality gate for the new EFIS NEXT
platform. The approved reference image is:

`output/concepts/pfd-professional-v1.png`

It is a design target, not an operational display. Values and terrain shown in
the concept are illustrative.

## Visual architecture

The screen is divided into five persistent information zones:

1. top flight-mode and source annunciation rail;
2. central synthetic-vision flight canvas;
3. left air-data tape;
4. right altitude and vertical-speed tapes;
5. lower navigation, engine and aircraft-system deck.

Traffic and moving-map information may occupy a context panel without obscuring
the invariant PFD instruments.

## Quality requirements

- Primary values remain readable in direct sunlight and degraded modes.
- Colour never acts as the only carrier of safety-critical meaning.
- Active, armed, invalid, stale, simulated and unavailable states are visually
  distinct.
- Decorative effects must not reduce contrast or frame rate.
- Synthetic vision, terrain, traffic and weather always include provenance,
  validity and age in the operational implementation.
- Layouts scale from 1024 x 600 to Full HD without merely enlarging every item.

## Rendering allocation

- C++ Scene Graph / RHI: terrain, horizon, runway, flight path, tapes, HSI,
  traffic geometry and dense plotted data.
- QML: responsive layout, state composition, touch interaction and text panels.
- Shader assets: terrain shading, masks, anti-aliasing and restrained transitions.
- SVG atlas: aircraft, airport, waypoint, warning and system symbols.

## Implementation sequence

1. Introduce typography, spacing, stroke and colour tokens.
2. Build the invariant PFD geometry as reusable native components.
3. Implement tapes, bugs, trend vectors and validity states.
4. Implement the synthetic-vision renderer and runway corridor.
5. Implement the HSI/navigation deck.
6. Integrate EMS and system strips through profile configuration.
7. Add Standard, Professional and reversionary layouts.
8. Validate frame timing, sunlight readability and failure annunciation on the
   Raspberry Pi target.

