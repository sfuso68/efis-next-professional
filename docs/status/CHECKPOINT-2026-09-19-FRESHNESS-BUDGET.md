# Checkpoint — Unified freshness budget V1

Data: 2026-09-19

Branch: `safety/staleness-budget-contract-v1`

ID: `CORE-004`, `BBX-003`

ADR: `ADR-017`

Un solo contratto deriva soglie link, campione, nodo e banco da un tick di
250 ms. DDP applica anche un tick come margine conservativo per latenza di rete
non misurabile tramite clock monotoni fra host.

Test pre-fix: FAIL. Build core e CTest post-fix: 22/22 PASS.

Nessuna verifica o distribuzione hardware. Stato non `DONE`.
