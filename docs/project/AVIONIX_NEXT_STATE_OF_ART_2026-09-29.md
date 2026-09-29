# FUSO AVIONICS NEXT — Stato dell'arte 2026-09-29

## Sintesi esecutiva

Il progetto dispone di un core avionico, HMI PFD/MFD/EMS, broker Blackbox,
synthetic vision, simulatori, aggiornamento/rollback e una catena di controllo
progetto sostanziale. Non è però un prodotto qualificato per il volo: gran
parte delle funzioni è `PARTIAL`, `VERIFY-HW` o `TODO` e le nuove pipeline di
prodotto restano `NOT_RUN` sui dati reali.

| Indicatore | Stato |
|---|---:|
| Avanzamento ponderato | 46,8% |
| Requisiti | 71 |
| DONE | 4 |
| VERIFY-HW | 11 |
| PARTIAL | 38 |
| TODO | 14 |
| BLOCKED | 2 |
| LEGACY | 2 |

## Risultati consolidati

- core C++20 indipendente dalla UI e modello dati con provenance;
- simulazione motore esplicitamente marcata SIM;
- aggiornamento immutabile e rollback verificati sui nodi gestiti;
- ICD congelato e riprodotto su x86-64 e tre nodi ARM64;
- PFD, MFD, EMS e Blackbox operativi in configurazioni di sviluppo;
- contratti HMI, synthetic vision e pacchetti di acceptance pronti;
- catena prodotto software completa da contratto/intake fino al piano di
  implementazione, sempre fail-closed e senza autorità operativa.

## Blocchi software completati ma non eseguiti realmente

Per `EMS-004`, `PROD-001`, `FPV-002`, `VOI-003` e `VER-002` sono disponibili:

1. contratto e registri;
2. intake e review gate;
3. evidence packager e review indipendente;
4. session manager e gate disposition;
5. change-control request e decisione CCB;
6. implementation plan con baseline, work package, rollback e verifica.

Tutti partono `NOT_RUN`. Nessuna fonte motore, firma reale, prova FPV/audio,
fixture HIL o manifest SKU è stata approvata tramite questa catena.

## Principali lavori interattivi

1. Inventario ASSO5: quattro Sequent, DIP/ID, alimentatore, Ethernet e mapping.
2. Verticale sensore→RPi4→Blackbox→RPi5 con dataset e fault reali.
3. Acceptance PFD/MFD/EMS: touch, leggibilità, frame-rate, termica e fault.
4. Pixhawk/M10/Matek: GNSS outdoor, bussola, airspeed e recovery registrati.
5. Synthetic vision: deploy C12 gemello, run C9–C11 e qualità terreno reale.
6. Blackbox: endurance, storage fault su fixture e recovery/replay.
7. Alimentazione e rete: switch, PSU, brownout, isolamento FLIGHT-DATA/VIDEO.
8. FPV e voce: hardware identificato, banco isolato, latenza/intelligibilità.
9. Fonti controllate: manuali Rotax/velivolo/elica e fraseologia applicabile.
10. Backup/restore: seconda copia Pixhawk e restore drill NAS/serverha.

## Blocchi reali

- `PAN-002`: mancano POH e manuali controllati dell'esemplare;
- `PAN-003`: servi, meccanica, interlock e HIL non identificati/qualificati.

## Valutazione critica

L'architettura e la disciplina software sono più mature della validazione del
sistema fisico. Il collo di bottiglia non è aggiungere altre schermate o altri
template: è acquisire evidenza reale, chiudere sorgenti/autorevolezza dei dati,
qualificare alimentazione/rete/sensori e provare gli scenari di guasto sui
nodi. Qualunque percentuale superiore senza queste attività sarebbe solo
documentale e non rappresenterebbe maturità avionica.

## Ordine operativo raccomandato

1. chiudere G1 inventario e switch;
2. eseguire G2 verticale reale;
3. eseguire acceptance HMI/SV sui tre pannelli;
4. qualificare M10 e Matek in monitor-only;
5. chiudere Blackbox/power/endurance;
6. acquisire fonti motore e avviare per primo il dominio `engine` nella nuova
   pipeline; gli altri quattro domini seguono soltanto con evidenza reale.

Stato globale: **DEVELOPMENT / NON-CERTIFIED / NO FLIGHT APPROVAL**.
