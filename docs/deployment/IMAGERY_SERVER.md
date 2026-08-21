# EFIS NEXT Imagery Server

The first server process is deliberately read-only. It publishes health, catalogue, dataset manifests and immutable tile files. Provider credentials and ingestion jobs are separate and never exposed to an EFIS display.

## Production topology

- `/srv/mapdata/imagery`: versioned tile store on NVMe, separate from OSM, DEM and aeronautical data;
- `mapapp`: private FastAPI service on the Docker network;
- Caddy or nginx: HTTPS, authentication, rate limits and access logs;
- optional MinIO/S3: replace the local tile store when fleet distribution is required;
- second disk or NAS: backup, not a live substitute for the active catalogue.

## API V1

- `GET /v1/health`
- `GET /v1/catalog`
- `GET /v1/datasets/<dataset_id>/manifest`
- `GET|HEAD /tiles/<dataset>/<z>/<x>/<y>.<format>`

Tile responses support ETag, immutable caching and HTTP byte ranges. Paths are resolved below the configured root and traversal is rejected.

## Initial mini-PC recommendation

- Linux server distribution with unattended security updates;
- 4 CPU cores and 8 GB RAM minimum;
- 2 TB NVMe for the first North Italy archive;
- wired Gigabit Ethernet to the hangar/router;
- separate backup disk;
- UPS and controlled shutdown.

Exact storage requirements must be measured from the encoded North Italy tile build before purchasing final hardware.

## EFIS device access

Each display uses a dedicated, revocable machine account. Never reuse the server
administrator password and never publish an unauthenticated endpoint. The Raspberry Pi
must resolve `mappe.casa.lan` to the server, trust the internal Caddy CA, and store its
credentials in a root-owned environment file (`0600`).

The cache synchroniser reads `EFIS_IMAGERY_USER` and `EFIS_IMAGERY_PASSWORD`; secrets are
not accepted in a plan file or command-line argument. Downloads are written to a partial
file, checked as PNG/WEBP and atomically promoted, so an interrupted transfer cannot
replace a valid flight cache.

Current pilot dataset: `north-italy-sentinel2-development`, two one-degree mosaics around
Treviso and the Dolomites, zooms 8–14. This is a pipeline validation dataset, not yet full
North Italy coverage and not an authorised navigation source.
