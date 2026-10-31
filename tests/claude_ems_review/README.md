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

Atteso: `Ran 51 tests ... OK`, exit code `0`.
Dipendenze: solo la libreria standard di Python 3, più `git` per la sola classe
`OpenStackRelationship`.

Quella classe verifica il rapporto fra `main`, la baseline dichiarata dal piano
e la testa dello stack EMS aperto, e la mappa delle collisioni della sezione 7
della review. Richiede che lo stack sia stato fetchato:

```bash
git fetch origin fix/ems-responsive-header-r108-v1
```

Senza quel fetch la classe si limita a segnalare uno skip e il resto della
suite passa comunque (`OK (skipped=1)`, exit code `0`).

## 3. `test_ems_setup_overlay_contract_r130.py` — lotto L0, implementato

Verifica l'implementazione del finding E-05 su `qml/EmsSetupOverlay.qml`, il
solo file assegnato in esclusiva al lotto L0. Senza Qt il QML non si può
istanziare, quindi il test è statico: nessuna identità di motore o velivolo
cablata, ogni riferimento `controller.*` risolto contro le `Q_PROPERTY` e le
`Q_INVOKABLE` di `ems_controller.hpp`, delimitatori bilanciati, etichette nuove
vincolate da `width` ed `elide`, e contratto DATA LINK di
`tests/test_avionix_hmi_contract.py` intatto.

```bash
python3 tests/claude_ems_review/test_ems_setup_overlay_contract_r130.py
```

Atteso: `Ran 28 tests ... OK`, exit code `0`.
Dipendenze: solo la libreria standard di Python 3.

## 4. `test_ems_gap_invariants_r130.py` — invarianti desiderati

Dichiara il comportamento che la review ritiene corretto. **Fallisce di
proposito**: ogni fallimento è l'evidenza riproducibile di un finding, e il
nome del caso ne riporta l'identificativo (`e01a`, `e02c`, …).

```bash
python3 tests/claude_ems_review/test_ems_gap_invariants_r130.py
```

Atteso dopo il lotto L0: `Ran 39 tests ... FAILED (failures=48)`, exit code
`1`. Erano 51 sulla baseline della review: i casi `e05b`, `e05c` ed `e05d` sono
passati a verdi con L0, mentre `e05a` resta rosso perché la capacità dei
serbatoi vive in `qml/AdaptiveEmsView.qml`, che L0 non possiede. Non è un gate di regressione; diventa verde quando i gap sono
chiusi, e va aggiornato insieme alla chiusura.
Dipendenze: solo la libreria standard di Python 3.

## Nota sul ramo di esecuzione

I test leggono l'albero di lavoro corrente. Eseguiti sulla testa dello stack
EMS aperto (`22de6c3`, PR #59) alcuni casi di `test_ems_gap_invariants_r130.py`
cambiano esito: la review indica per ogni finding se è aperto su `main`, sullo
stack, o su entrambi.
