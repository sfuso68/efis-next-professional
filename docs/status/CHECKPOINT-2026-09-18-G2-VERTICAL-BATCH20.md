# Checkpoint — G2 verticale, venti controlli software — 2026-09-18

## Implementato

- Contratto machine-readable V1 del verticale live.
- Venti controlli `G2-V20-01`…`G2-V20-20` fail-closed.
- Cross-check con network plan e ICD congelato.
- Validator CLI e dieci test di regressione/mutazione.
- Esecuzione in Project Control CI.

## Verificato solo software

- Validator: 20/20 PASS.
- Unit test: 10/10 PASS.
- Stato restituito: `NOT_RUN`.
- Autorizzazione esecuzione e flight approval: false.

## Verificato su hardware reale

- Nulla in questo batch.

## Blocco / decisione

Il segmento sensore/Sequent→adapter RPi4→Blackbox non ha ancora un trasporto
autorevole. Il validator lo mantiene `UNRESOLVED` e lega il NO-GO a
`AVX-G1-01/02/03`. DDP/EDP downstream non colmano questa lacuna.

## Aperto

- Inventario reale nodo ASSO5 e quattro Sequent.
- Definizione e test adapter real/SIM con health.
- Proposta del trasporto RPi4→Blackbox.
- Revisione del runbook e autorizzazione utente separata.
- Solo dopo: smoke 60 s, fault matrix, endurance 3600 s e replay.

Nessuna configurazione, servizio o rete operativa è stata modificata.
