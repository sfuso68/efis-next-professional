# Navigation Control Center V1

## Purpose

The NAV page is the common presentation layer for GPS, VOR and NDB sources. It does not synthesize radio-navigation validity: unavailable receivers, frequencies or database records remain explicitly unavailable.

## Interaction

- Tap the lower PFD compass/HSI zone to open NAV directly.
- The vertical MENU provides PFD, NAV, CANVAS, MISSION, EMS and FPV pages without covering the flight instruments horizontally.
- GPS, VOR and NDB are selected from the left source rail.
- `Ctrl+5` opens NAV during desktop development and display validation.

## Layout

1. Left source rail: selected navigation sensor, receiver state and tuning status.
2. Central HSI: rotating 360° compass rose, heading readout, selected course, CDI, bearing, groundspeed and cross-track status.
3. Right data panel: active facility/destination, position, desired track, integrity and database status.

## Data policy

- GPS currently uses the selected airport and controller navigation state.
- VOR/NDB remain `NO TUNED FACILITY` until a validated facility/frequency database and receiver adapter exist.
- Current values are labelled simulation data and are not authorised for operational navigation.
- Future radionavigation records require identifier, name, type, frequency, position, service volume, magnetic variation, effective dates, source, status and checksum.

## Next increments

1. Add a versioned VOR/NDB/frequency package beside the airport database.
2. Add dual NAV receiver state (`NAV1`, `NAV2`) and bearing pointers.
3. Implement touch tuning, nearest facilities and frequency transfer/standby.
4. Connect Pixhawk/MAVLink GPS integrity and M10 receiver diagnostics.
5. Add flight-plan legs, OBS mode, direct-to and missed-approach-safe interaction states.

## Phase 2 checkpoint — receiver and database contract

- VOR and NDB modes expose independent `NAV1` and `NAV2` receiver selection.
- ACTIVE and STBY frequency fields and the transfer action are operational, but remain blank until validated data is installed.
- The UI displays `NO FACILITY`, `NOT TUNED` and `NO VALIDATED FREQUENCY PACKAGE` instead of synthesizing radio-navigation information.
- `data/navigation/schema/radionavigation.schema.json` defines the versioned package contract for VOR, VOR/DME, VORTAC, NDB, DME, ILS and localizer records.
- `data/navigation/radionavigation-index.json` is an intentionally empty development package; it proves the safe no-data state without shipping invented frequencies.
- Raspberry Pi build and core tests passed; GPS and VOR/NAV1 layouts were visually verified at 1024×600.

## Phase 3 checkpoint — controlled ingestion

- `tools/navigation/import_radionavigation.py` converts a reviewed CSV export into the versioned JSON contract.
- Required inputs are identifier, facility type, name, frequency and WGS84 position; optional fields cover elevation, magnetic variation, service volume and source record ID.
- The importer normalizes VHF MHz to integer kHz and validates the NDB, VOR/DME/VORTAC and ILS/LOC frequency bands independently.
- Invalid coordinates, unsupported facility types and duplicate identifier/type/frequency records are rejected.
- Every output records the complete source-file SHA-256, edition, attribution, reference and effective dates.
- Imported data remains `DEVELOPMENT`; promotion to operational status is deliberately outside the importer and requires a separate assurance gate.
- The C++ controller loads the index at startup and exposes database version, status and facility count to QML.
- The empty-package runtime test reports `0.1.0-empty-development • NO_DATA • 0 facilities`, demonstrating deterministic fail-safe behaviour.
- Three importer tests and all EFIS core tests pass on the RPi5; initial render health remains approximately 45 fps.
