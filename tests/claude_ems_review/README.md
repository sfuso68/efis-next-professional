# Test isolati dell'audit EMS R1-30

Questa cartella appartiene alla review indipendente
`docs/reviews/CLAUDE_EMS_GAP_REVIEW_R130.md` e contiene soltanto test nuovi.

**Nessuno di questi test è dichiarato in CMake o in CI**: l'incarico vieta di
modificare `CMakeLists.txt`, i workflow, i registri canonici e il codice di
runtime. Vanno quindi eseguiti a mano con i comandi qui sotto. Nessun test
scrive file, apre socket o tocca hardware.

## 1. `ems_core_behaviour_r130.cpp` — nucleo motore, eseguibile

Esercita le unità del nucleo che non dipendono da Qt: `RuntimeTracker`,
`engine_integration_input`, `present_ems_link`.

```bash
g++ -std=c++20 -Wall -Wextra -Wpedantic -I include \
  -o /tmp/ems_core_behaviour_r130 \
  tests/claude_ems_review/ems_core_behaviour_r130.cpp \
  src/core/engine_runtime.cpp src/core/derived_state.cpp
```

```bash
/tmp/ems_core_behaviour_r130
```

Atteso: `29 checks, 0 failures`, exit code `0`.
Dipendenze: GCC con C++20. Nessun Qt, nessuna rete.

## 2. `test_ems_limit_authority_r130.py` — stato osservato

Fotografa i numeri e le strutture presenti oggi nelle sorgenti e nei registri,
così una deriva diventa visibile. Passa sulla baseline della review.

```bash
python3 tests/claude_ems_review/test_ems_limit_authority_r130.py
```

Atteso: `Ran 45 tests ... OK`, exit code `0`.
Dipendenze: solo la libreria standard di Python 3.

## 3. `test_ems_gap_invariants_r130.py` — invarianti desiderati

Dichiara il comportamento che la review ritiene corretto. **Fallisce di
proposito**: ogni fallimento è l'evidenza riproducibile di un finding, e il
nome del caso ne riporta l'identificativo (`e01a`, `e02c`, …).

```bash
python3 tests/claude_ems_review/test_ems_gap_invariants_r130.py
```

Atteso sulla baseline della review: `Ran 39 tests ... FAILED (failures=51)`,
exit code `1`. Non è un gate di regressione; diventa verde quando i gap sono
chiusi, e va aggiornato insieme alla chiusura.
Dipendenze: solo la libreria standard di Python 3.

## Nota sul ramo di esecuzione

I test leggono l'albero di lavoro corrente. Eseguiti sulla testa dello stack
EMS aperto (`22de6c3`, PR #59) alcuni casi di `test_ems_gap_invariants_r130.py`
cambiano esito: la review indica per ogni finding se è aperto su `main`, sullo
stack, o su entrambi.
