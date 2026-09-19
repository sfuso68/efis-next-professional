# Checkpoint — G1 visual inventory intake

Data: 2026-09-19

Branch: `work/skyview-ems-hmi-contract-v1`

## Implementato

- Template V1 per la trascrizione visuale di quattro schede e alimentatore.
- Importatore con preview predefinita e applicazione esplicita tramite
  `--apply`.
- Verifica JPEG/PNG, SHA-256, path confinati, metadati, dichiarazione di
  sicurezza, anti-overwrite e manifest NO-GO.
- Integrazione in CTest e Project Control CI.

## Verificato solo software

- Suite dedicata: 9/9 PASS.
- Suite combinata G1 inventory/readiness: 25/25 PASS.
- CTest repository: 45/45 PASS.
- Project status `--check`: PASS.

## Verificato su hardware reale

Nessuna nuova verifica. Non sono state acquisite fotografie reali e non sono
stati contattati nodi, bus o schede.

## Stato aperto

- Fornire nove fotografie nitide e compilare i metadati osservati.
- Eseguire prima preview, poi import solo dopo revisione dell'operatore.
- Schede reali: 0/4.
- Decisione: `NO_GO_PENDING_PHYSICAL_BOARD_INVENTORY`.
