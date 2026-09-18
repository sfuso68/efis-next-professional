# Checkpoint — G1/G2 readiness batch 100

Data: 2026-09-18

Branch: `work/skyview-ems-hmi-contract-v1`

## Implementato

- Contratto machine-readable di cento controlli divisi in inventario, segnali,
  calibrazione, trasporto ed evidence/release.
- Validator con cross-check verso manifest fisico, contratto verticale G2,
  `SignalId`, `EngineSnapshot` e ADR EDP V1.
- Test che muta individualmente tutti i cento controlli e verifica i principali
  confini negativi.
- Integrazione CTest e Project Control CI.

## Verificato solo software

- Contratto: 100/100 controlli validati.
- Catalogo: 100/100 mutazioni rifiutate.
- Suite dedicata: 8/8 PASS.
- Trasporto downstream: Blackbox→EMS EDP V1/UDP 49003 one-way coerente con ADR.
- Modello software: 18 segnali motore e quattro canali CHT/EGT coerenti.

## Verificato su hardware reale

Nessuna nuova prova hardware. Restano valide soltanto le evidenze precedenti
del nodo `ems-sensor`; questo batch non verifica le quattro Sequent.

## Stato aperto

- Schede reali: 0/4.
- Indirizzi: non rilevati e probing non autorizzato.
- Mappa canali e tarature: irrisolte.
- `SourceId` reale: decisione architetturale aperta.
- Trasporto RPi4→Blackbox: irrisolto.
- Esecuzione live e uso in volo: non autorizzati.

Decisione:
`NO_GO_PENDING_G1_PHYSICAL_INVENTORY_AND_ARCHITECTURE_REVIEW`.
