# CHECKPOINT — contratto HMI EMS comportamentale

Data: 2026-09-20  
Branch: `safety/hmi-behavioral-contract-v1`  
Classificazione: verifica software

## Difetto riprodotto

`test_operational_ems_exposes_edp_state_and_age` leggeva il testo di header e
sorgente e cercava nomi di stati. La presenza delle parole `LIVE`, `STALE` o
`LOST` non dimostrava che il controller applicasse la policy corretta. Il nuovo
test nativo è stato inserito prima della policy condivisa ed è fallito in
compilazione sull'header mancante.

## Correzione

- `present_ems_link(LinkState, simulated)` definisce in modo puro stato dati,
  stato motore, identità sensore, autorità della rete e reset dell'età.
- `EmsController::updateSnapshot()` usa direttamente quel risultato.
- `ems_link_presentation_tests.cpp` verifica le sei transizioni significative,
  incluse le combinazioni LIVE/STALE con dato simulato.
- Il test Python basato su `read_text()` è stato eliminato; gli altri controlli
  strutturali HMI restano invariati.

## Evidenza

- CTest comportamentale: PASS.
- Build `efis-ems-qt`: PASS.
- Contratto HMI Python: 16/16 PASS.

## Limiti

- Nessuna esecuzione sul RPi4 in questo branch.
- Nessuna prova di rete live, touch o fault injection e nessuna autorizzazione
  operativa.
