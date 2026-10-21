# R1-03 — Radar position validity (N3)

Candidate software change, 2026-10-21. Requirements: WX-002, VER-001.
Base: main `4aea1e4edf8678c180e04171813d842ca6ff58a0`.
No deployment, hardware qualification or flight approval.

## Contract

- The PFD binding checks position validity before reading coordinates. Position
  provenance comes from latitude/longitude samples, never from unrelated SIM
  attitude, engine or airspeed channels.
- Real radar requires explicit validity, non-simulated provenance, finite
  coordinates and coverage inside the existing 35–48 N / 4–21 E rectangle.
  This rectangle is a software guard, not evidence of provider coverage.
- There is no default Treviso position. Startup, invalid/out-of-coverage fixes
  and simulated coordinates do not trigger real radar requests or cache loads.
- Loss of position clears image, timestamp, geographic label and tile identity
  immediately. Repeated invalid notifications are idempotent.
- Tile/mode/loss transitions invalidate a context generation. Replies from an
  invalidated context cannot publish or write the cache, including recovery to
  the same tile. Superseded products within the same position context can still
  cache their own product, preserving the existing VMI/SRI isolation contract.
- Explicit weather training is allowed without GNSS and says SIMULATION AREA.
  Switching modes clears the previous frame before asynchronous generation.
  Old simulation worker completions are rejected after a mode transition.
  Leaving training without a valid real fix remains NO DATA.

## Verification and boundaries

The native WeatherService suite uses actual Qt network requests and encoded
256x256 PNG payloads against a loopback-only controlled HTTP server. The test
constructor suppresses external METAR/TAF calls and initial auto-refresh;
production retains the existing text-weather behavior. Docker network is
disabled; source is read-only, build/runtime files are ephemeral. No production
cache or service is used. The native test remains registered in the existing
Qt CTest/CI target; this change does not replace it with source-marker checks.

See `../status/evidence/r1-03-position-20261021/verification.json` for results.
Full native PFD runtime and ARM64/physical GNSS loss/recovery are not qualified
by the isolated WeatherService test. Cache age still uses receipt/file time:
N10 observational freshness is open, as are N9 METAR/TAF freshness and provider
georeferencing. N2/N6 presentation changes remain in separate PR #54; N4/N5 in
PR #53. None is assumed integrated by this candidate.

## Next gate

Review CI and integrate reviewed software independently. Then complete
per-channel engine/fuel validity (R1-04 / N7–N8) and weather freshness
(R1-05 / N9–N10). Real node activation remains blocked by power qualification;
the later bench campaign must exercise GNSS absence, loss/recovery and mode
transitions on the actual display and data source.
