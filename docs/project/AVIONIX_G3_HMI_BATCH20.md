# AVIONIX NEXT — G3 HMI fail-closed batch 20

Data: 2026-09-17

Attività: `AVX-G3-04`

Stato: IMPLEMENTED-SW / VERIFY-HW

Approvazione al volo: no

## Scopo

Questo incremento rende il manifesto HMI SkyView/EMS un contratto chiuso e
verificabile. Non modifica il runtime avionico, non cambia il wire protocol e
non distribuisce software sui nodi. Ogni controllo ha un identificatore stabile
e un test negativo che dimostra il rifiuto della relativa deviazione.

## Venti controlli

| ID | Vincolo fail-closed | Esito software |
|---|---|---|
| HMI-B20-01 | radice, schema e stato non-flight chiusi | PASS |
| HMI-B20-02 | canvas logico esatto 1024×600 | PASS |
| HMI-B20-03 | soli target SkyView/RPi5 ed EMS/RPi4 | PASS |
| HMI-B20-04 | matrici risoluzioni complete e ordinate | PASS |
| HMI-B20-05 | minimi touch, testo e spessore linea | PASS |
| HMI-B20-06 | palette semantica chiusa, RGB uppercase e colori univoci | PASS |
| HMI-B20-07 | stati dati canonici completi e ordinati | PASS |
| HMI-B20-08 | freshness LIVE/STALE 250/1000 ms | PASS |
| HMI-B20-09 | zone SkyView complete e ordinate | PASS |
| HMI-B20-10 | V-speed VS0/VS1/VX/VY/VFE/VNO/VNE | PASS |
| HMI-B20-11 | arco HSI visibile ±35° | PASS |
| HMI-B20-12 | terrain con source, age e runway-context obbligatori | PASS |
| HMI-B20-13 | overlay weather/traffic/obstacles/route ordinati | PASS |
| HMI-B20-14 | minimo 40 FPS e target almeno 50 FPS su SkyView/EMS | PASS |
| HMI-B20-15 | policy compact EMS 800×480 completa | PASS |
| HMI-B20-16 | gerarchia gauge primari e secondari EMS | PASS |
| HMI-B20-17 | quattro cilindri con canali CHT/EGT ordinati | PASS |
| HMI-B20-18 | detent flap e tre consensi carrello | PASS |
| HMI-B20-19 | pagine EMS e canali elettrici chiusi | PASS |
| HMI-B20-20 | invarianti degradati esatti e nessuna autorità di comando | PASS |

## Evidenza software

- `python tools/ui/validate_hmi_contract.py`: PASS.
- `python -m unittest tests.test_avionix_hmi_contract`: 17/17 PASS; include
  venti mutazioni isolate, una per controllo.
- `py_compile` di validatore e suite: PASS.
- La suite è registrata in CTest come `efis_avionix_hmi_contract_tests`.

Le golden EMS già versionate coprono 800×480, 1024×600 e 1280×800. Esse sono
regressioni raster software, non acceptance ergonomica o prestazionale.

## Limiti e lavoro aperto

- Non esiste ancora una matrice golden SkyView completa, in particolare
  1920×1080.
- Non sono verificati pannello reale, touch, sunlight/night mode, 40 FPS,
  termica o fault fisici.
- Nessun file QML, C++ operativo o configurazione installata è stato cambiato
  da questo batch.
- `AVX-G3-04` e `PRJ-004` restano aperti a verifica hardware; non dichiarare
  `DONE`.
