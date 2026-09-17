# AVIONIX NEXT — G3 EMS state presentation batch 20

Data: 2026-09-18

Attività: `AVX-G3-04` / `PRJ-004`

Stato: VERIFIED-SW / VERIFY-HW

## Venti punti implementati

| ID | Controllo | Esito |
|---|---|---|
| HMI-E20-01 | schema EMS state V1 e radice chiusa | PASS |
| HMI-E20-02 | stato software-only non flight-approved | PASS |
| HMI-E20-03 | ruolo EMS su RPi4 conforme ad ADR-013 | PASS |
| HMI-E20-04 | fixture compact fissata a 800×480 | PASS |
| HMI-E20-05 | sei stati canonici e ordinati | PASS |
| HMI-E20-06 | proprietà di stato fail-closed | PASS |
| HMI-E20-07 | età deterministiche per la fixture | PASS |
| HMI-E20-08 | severità NORMAL/CAUTION/WARNING/SIMULATION | PASS |
| HMI-E20-09 | età visibile per STALE/DEGRADED/LOST | PASS |
| HMI-E20-10 | verde normale vietato nei quattro stati degradati | PASS |
| HMI-E20-11 | policy di presentazione chiusa | PASS |
| HMI-E20-12 | archi normali soppressi in degrado/perdita | PASS |
| HMI-E20-13 | ultimo valore solo con invalidità esplicita | PASS |
| HMI-E20-14 | CAS caution e warning mappati separatamente | PASS |
| HMI-E20-15 | funzioni derivate inibite in LOST/FAIL | PASS |
| HMI-E20-16 | colore mai unico cue di sicurezza | PASS |
| HMI-E20-17 | SIM sempre riconoscibile | PASS |
| HMI-E20-18 | nessuna autorità di comando | PASS |
| HMI-E20-19 | fixture golden non interattiva e firmata | PASS |
| HMI-E20-20 | sei PNG, dimensioni, ordine e SHA-256 verificati | PASS |

## Correzioni funzionali emerse dalla matrice

- `STALE` e `DEGRADED` conservano i valori leggibili, ma rimuovono archi e
  riempimenti verdi normali, applicano il cue ambra e mostrano `0.6s`.
- `LOST` e `FAIL` mantengono l'ultimo valore dei gauge soltanto come riferimento
  attenuato e circondato da warning rosso; fuel, electrical e CHT/EGT non
  presentano più un valore corrente valido.
- Endurance non viene più calcolata/presentata in `LOST/FAIL`.
- Hottest cylinder e spread non vengono più derivati in `LOST/FAIL`; i footer
  mostrano `CHT UNAVAILABLE` ed `EGT UNAVAILABLE`.
- La matrice golden comprende `LIVE`, `STALE`, `DEGRADED`, `LOST`, `FAIL` e
  `SIM` a 800×480.

## Evidenza software

- Build MSVC/Qt 6.11.1 e QML AOT per PFD ed EMS: PASS.
- Sei catture native Windows 800×480 generate e ispezionate.
- Validatore EMS state: 20 controlli fail-closed PASS.
- Manifesto `avionix.ems-state-golden.v1` con dimensioni e SHA-256 per immagine.

## Limiti

- Le immagini sono riferimenti software, non acceptance del pannello RPi4.
- Restano aperti touch, contrasto diurno/notturno, 40 FPS, replay EDP reale,
  fault Ethernet, reboot, endurance e termica.
- I limiti motore restano `UNVERIFIED` finché non viene acquisito il manuale
  applicabile alla specifica installazione Rotax 912 UL.
