# EFIS Operational Catalog V2

## Purpose and safety boundary

Operational Catalog V2 is the controlled source for aircraft configuration,
display limits, alert thresholds, procedures and installed-system metadata. It
extends Catalog V1; it does not replace the existing aircraft and engine search
catalogue.

The database is not an approval authority. A value copied from a web page,
legacy source, owner statement or similar-aircraft profile remains indicative.
Only the documents applicable to the individual aircraft, engine, propeller and
installed equipment can support a `VERIFIED` or `APPROVED` release.

## No-silent-omission rule

The controlled dictionary contains 447 fields in 28 domains. When a profile is
materialized, the database creates exactly one row for every dictionary field.
Every row has one disposition:

- `VALUE`: an explicit typed value;
- `INHERIT`: use the parent model or variant value;
- `NOT_APPLICABLE`: the field does not apply, with a mandatory rationale;
- `NOT_INSTALLED`: the related equipment is absent, with a mandatory rationale;
- `UNKNOWN`: unresolved and visible as such; it can block operational release.

The three extension fields for aircraft, engine and equipment specifics capture
items not yet standardized. `data_governance.unmapped_source_fields` records
anything found in an applicable source but not mapped to the dictionary. It
must be empty before a profile can be released.

## Profile hierarchy

Profiles are overlays, not copies:

```text
MODEL
  -> VARIANT
       -> INSTALLED_AIRCRAFT

ENGINE_MODEL -----------^
PROPELLER_MODEL --------^
```

An installed-aircraft profile identifies the registration, construction number,
serial numbers, installed sensors, calibration records, actual weight and
balance, electrical topology and equipment configuration. A model profile must
never be treated as proof that the individual aircraft has the same limits or
equipment.

## Controlled domains

The dictionary covers identity, regulatory basis, geometry, mass and balance,
structural limits, all named and tabulated speeds, performance, engine, fluids,
propeller, fuel system, electrical system, flight controls, flaps, landing gear,
pitot/static and air data, sensors, avionics, autopilot/servos, optional systems,
normal and emergency procedures, EMS alert logic, Emergency Assist, maintenance,
display units, data governance and custom/unmapped parameters.

The JSON dictionary is deliberately extensible. Adding a new field requires a
dictionary revision and regeneration of all materialized profiles; an unknown
field in a profile is rejected instead of ignored.

## Lifecycle and validation

1. `SIMULATION`: suitable only for simulator and UI development.
2. `DRAFT`: source collection and aircraft survey in progress.
3. `VERIFIED`: required fields resolved; safety-critical values cite source,
   document and page; unmapped source fields are empty.
4. `APPROVED`: formally accepted for the defined project use with a release
   record, hashes and rollback reference.
5. `RETIRED`: retained for audit but not selectable.

`VERIFIED` and `APPROVED` reject unresolved required fields. They also reject a
safety-critical `VALUE` without source key, document reference and page. This is
a software completeness gate, not a certification claim.

## Repository assets

- `data/catalog/operational-parameter-dictionary-v2.json`: authoritative field
  dictionary.
- `docs/design/EFIS_OPERATIONAL_PARAMETER_REGISTER_V2.md`: generated,
  human-readable list of every controlled field.
- `data/catalog/schema/002_operational_parameters.sql`: normalized SQLite V2
  layer.
- `data/catalog/seed/operational-seed-v2.json`: initial model and installed-
  aircraft overlays, intentionally `SIMULATION`.
- `tools/catalog/catalog_tool.py`: dictionary/profile validators and database
  builder.
- `tests/catalog_tool_tests.py`: completeness, rejection and database tests.

## Setup editor and draft boundary

PFD and standalone EMS expose the same `OperationalProfileSetup.qml` page and
the same C++ staging model. The editor can browse all domains and fields, show
resolved inheritance and safety blockers, and stage typed values with document,
revision, page and rationale metadata.

Edits are written atomically to `efis-next.operational-profile-draft.v2`. This
artifact always contains `activation_allowed=false`; it is deliberately not an
active-profile format and the flight runtime has no loader for it. The editor
has no activation control. Promotion requires a separate future review,
validation, immutable release, signature and rollback workflow.

## Build and validation

```bash
python3 tools/catalog/catalog_tool.py validate-parameter-dictionary \
  data/catalog/operational-parameter-dictionary-v2.json

python3 tools/catalog/catalog_tool.py validate-operational-seed \
  --dictionary data/catalog/operational-parameter-dictionary-v2.json \
  data/catalog/seed/operational-seed-v2.json

python3 tools/catalog/catalog_tool.py build \
  --schema data/catalog/schema/001_initial.sql \
  --operational-schema data/catalog/schema/002_operational_parameters.sql \
  --seed data/catalog/seed/catalog-seed.json \
  --parameter-dictionary data/catalog/operational-parameter-dictionary-v2.json \
  --operational-seed data/catalog/seed/operational-seed-v2.json \
  --legacy data/catalog/seed/legacy-catalog-import.json \
  --output data/catalog/efis-catalog.db
```

The standalone profile validator accepts one extracted profile JSON and optional
`--parent` profile JSON. The seed validator checks the complete collection,
parent references, cycles and every materialized field.

## Data acquisition required for the installed aircraft

Before moving beyond simulation, capture and reconcile:

- aircraft data plate, registration and construction standard;
- applicable AFM/POH, supplements and effective-page list;
- current weight-and-balance report and equipment list;
- engine and propeller data plates plus applicable manuals and service records;
- measured flap/gear positions, control travel and sensor calibration;
- fuel system usable quantities, tank calibration and selector topology;
- electrical schematic, protection, buses, sources, loads and endurance;
- avionics, Pixhawk, air-data, CAN/Ethernet and servo installation records;
- every limitation, speed table, performance table, procedure and maintenance
  interval with document revision and page.

Similarity to a Pioneer 300 is tracked only as an owner declaration until
document applicability is established field by field.
