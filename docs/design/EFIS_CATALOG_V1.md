# EFIS Catalog V1

> Catalog V1 remains the searchable aircraft/engine directory. The controlled,
> field-complete operational layer is specified in
> `docs/design/EFIS_OPERATIONAL_CATALOG_V2.md` and schema migration
> `data/catalog/schema/002_operational_parameters.sql`.

EFIS Catalog is an updateable reference database. It never changes the active
aircraft profile automatically.

## Trust boundary

1. Imported Internet or legacy data enters as `INDICATIVE`.
2. A source can promote a field to `DOCUMENTED`.
3. Manual comparison with the installed aircraft POH/AFM promotes it to
   `VERIFIED`.
4. Only an explicitly activated profile may become `APPROVED`.

The PFD, Canvas, EMS, alert engine, simulator and recorder consume a frozen
active profile, not live catalog rows.

## Files

- `data/catalog/schema/001_initial.sql`: normalized database schema.
- `data/catalog/seed/catalog-seed.json`: controlled seed input.
- `data/catalog/efis-catalog.db`: generated artifact, not hand edited.
- `config/active/aircraft-profile.json`: active installed-aircraft snapshot.
- `tools/catalog/catalog_tool.py`: builder and safety validator.

Catalog updates must be staged, integrity checked, checksummed and installed
atomically. Rollback keeps the previous database and active profile.
