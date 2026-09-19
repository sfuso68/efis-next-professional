# Checkpoint — G1 upstream proposal, venti punti

Data: 2026-09-19

Branch: `work/skyview-ems-hmi-contract-v1`

## Implementato

- Proposta machine-readable ERAP V1 per `sensor-rpi4 → Blackbox`.
- Riserva non operativa di indirizzo, porta e SourceId append-only.
- Separazione raw/normalizzazione/EDP e autorità Blackbox.
- Registro di nove blocker, incluso lo switch da 5 porte insufficiente.
- Validator fail-closed, test, CTest e Project Control CI.

## Verificato solo software

- 20/20 controlli strutturali validati.
- Tutte le 20 mutazioni del catalogo rifiutate.
- Suite dedicata: 8/8 PASS.
- Cross-check con ICD V1, enum C++, piano rete, G2 verticale e readiness R100.
- Regressione CTest repository: 46/46 PASS.
- Project status `--check`: PASS.

## Verificato su hardware reale

Nulla. Nessuna configurazione IP, bind UDP, modifica firewall, scansione,
lettura bus o deployment.

## Stato aperto

- Review e approvazione della proposta ERAP/SourceId.
- Inventario visuale 4/4, channel map e calibrazione.
- Decisione su topologia/switch con almeno sette porte operative.
- Rate, freshness e runbook live.

Decisione: `NO_GO_PENDING_ARCHITECTURE_AND_HARDWARE_REVIEW`.
