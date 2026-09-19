# CHECKPOINT — HMI golden EMS con rerender reale

Data: 2026-09-20  
Branch: `safety/hmi-golden-rerender-v1`  
Classificazione: verifica software, non accettazione hardware

## Difetto riprodotto

La precedente prova `test_ems_responsive_state_matrix.py` verificava schema,
presenza e digest dei 18 file già presenti in `tests/golden`, senza avviare il
renderer. Un'immagine obsoleta ma internamente coerente poteva quindi superare
il gate. Il primo test della modifica è fallito perché il modulo di confronto
del rerender non esisteva.

## Correzione

- `verify_ems_golden_rerender.ps1` avvia realmente `efis-pfd-qt
  --hmi-golden` per 3 risoluzioni e 6 stati.
- `verify_ems_golden_rerender.py` controlla la stessa matrice semantica e
  confronta i pixel decodificati. Le soglie sono `max <= 8`, media `<= 0.001`
  e frazione modificata `<= 0.001`.
- CTest usa il percorso PowerShell su Windows. Il baseline esistente dichiara
  `platform=windows`, quindi non viene confrontato automaticamente con un
  renderer Linux avente font e backend differenti.
- Il target corretto è `efis-pfd-qt`: è quello che incorpora la fixture
  `HmiGoldenMain.qml`. Il nodo `efis-ems-qt` carica invece la pagina operativa
  `EmsMain` e non è il produttore di questo baseline.

## Evidenza software

- Build Release MSVC/Qt 6.11.1 `efis-pfd-qt`: PASS.
- Rerender 18/18 e confronto visuale: PASS.
- La differenza osservata su un campione 1024×600/SIM era limitata a un delta
  massimo di 1 su 0,0045% dei canali, coerente con antialiasing e sotto soglia.

## Non verificato

- Nessun pannello RPi4/RPi5 è stato usato.
- Nessuna prova touch, sole/notte, frame-rate, termica o fault injection.
- Nessuna autorizzazione operativa o di volo.
