# AVIONIX G1 — kernel health adapter sensori, batch 20

Data: 2026-09-18

Stato: **IMPLEMENTED-SW / HARDWARE-NOT-RUN / NOT-FLIGHT-APPROVED**

## Scopo

Questo incremento implementa nel core C++20 una macchina a stati per ciascun
canale del futuro adapter ASSO5. Il componente non contiene driver Sequent,
non legge I²C/Modbus e non pubblica in rete: riceve osservazioni già acquisite e
decide in modo fail-closed se possano essere considerate utilizzabili.

File principali:

- `include/efis/providers/sensor_adapter_health.hpp`;
- `src/providers/sensor_adapter_health.cpp`;
- `tests/sensor_adapter_health_tests.cpp`.

## Venti punti implementati e verificati

1. Identità canale completa obbligatoria: signal, node, channel, unit e
   calibration revision.
2. Policy temporale valida soltanto con `stale > 0`, `lost > stale` e recovery
   maggiore di zero.
3. Stato iniziale `UNKNOWN` e non utilizzabile.
4. Primo campione REAL coerente porta a `VALID`.
5. Modalità SIM strutturale e persistente porta a `SIMULATED`.
6. Campione SIM su canale REAL, o viceversa, rifiutato senza freshening.
7. Sequenza duplicata rifiutata e contata senza aggiornare il dato buono.
8. Sequenza regressa rifiutata e contata.
9. Gap di sequenza accettato ma contato come campioni mancanti.
10. Regressione del tempo monotono rifiutata.
11. `board_absent` porta a `FAILED`.
12. `bus_timeout` porta a `FAILED`.
13. `open_input` porta a `FAILED`.
14. `short_input` porta a `FAILED`.
15. `invalid_range` porta a `FAILED`.
16. Silenzio oltre soglia produce `STALE`, poi `LOST`.
17. `FAILED` non si cancella con il solo trascorrere del tempo.
18. Recovery con dwell configurabile; nei test servono tre campioni consecutivi.
19. Restart elimina freshness e sequenza della sessione precedente.
20. Solo `VALID` e `SIMULATED` sono utilizzabili; gli altri stati mappano a
    `STALE` o `UNAVAILABLE`.

## Confini conservati

- Le soglie sono parametri iniettati: non vengono dichiarate adatte alle
  Sequent reali prima dell'inventario.
- Nessun nuovo `SourceId` viene aggiunto all'ICD congelato.
- Nessuna conversione elettrica, calibrazione, channel map o indirizzo viene
  inventato.
- Nessuna API di comando, relay o scrittura è presente nel componente.
- Il publisher RPi4→Blackbox resta `UNRESOLVED`.

## Verifica

Build MSVC Debug del target dedicato: PASS.

CTest:

- `efis_sensor_adapter_health_tests`: PASS, 20/20 punti;
- `efis_engine_tests`: PASS;
- `efis_core_tests`: PASS.

Suite core completa: 21/21 target CTest PASS.

La CI Linux rimane l'autorità multipiattaforma. Questa verifica non esercita
RPi4, Sequent, I²C, alimentazione o rete.
