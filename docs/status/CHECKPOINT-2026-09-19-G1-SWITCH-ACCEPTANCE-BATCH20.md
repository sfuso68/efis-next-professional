# Checkpoint G1 — switch acceptance batch 20

Data: 2026-09-19

Stato: `PREPARED-SW / NOT_RUN / NO-GO`

## Implementato

- contratto macchina G1-S20 con venti controlli fail-closed;
- template risultati privo di affermazioni hardware;
- validatore di identità, capacità, isolamento e provenance;
- verifica digest/path degli artefatti per un futuro bundle;
- test negativi e runbook non esecutivo.

## Verificato solo software

- template `NOT_RUN` accettato;
- venti mutazioni, una per controllo, rifiutate;
- bundle sintetico completo accettato solo come `BENCH_PASS`;
- path escape, digest errato e flight approval rifiutati.

## Verificato su hardware reale

Nulla. Il MokerLink non è stato contattato, alimentato, collegato o installato.

## Aperto

- confermare modello e pinout dalla targhetta/manuale reale;
- acquisire dieci evidenze con operatore presente;
- definire e approvare il protocollo elettrico G4;
- selezionare separatamente lo switch VIDEO/PoE;
- sottoporre ogni eventuale installazione a nuova decisione.

Decisione invariata: `NO_GO_PENDING_ARCHITECTURE_AND_HARDWARE_REVIEW`.
