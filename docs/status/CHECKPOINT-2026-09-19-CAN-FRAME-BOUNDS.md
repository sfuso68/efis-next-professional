# Checkpoint — CAN frame bounds V1

Data: 2026-09-19

Branch: `safety/can-frame-bounds-v1`

ID: `ESP-001`

## Modifica

Le primitive di estrazione CAN restituiscono `optional` e rifiutano frame corti
o offset che eccedono DLC/capienza. Non possono più completare implicitamente
una misura usando gli zeri residui dell'array.

## Evidenza software

- Riproduzione pre-fix: errore di compilazione, API incapace di rappresentare
  la lettura fallita.
- Test mirato ESP32: PASS.
- Build core: PASS.
- CTest: 20/21 PASS; un eseguibile estraneo non avviato da Smart App Control.

## Evidenza hardware

Nessuna. Stato `PARTIAL`, non `DONE` e non flight approved.
