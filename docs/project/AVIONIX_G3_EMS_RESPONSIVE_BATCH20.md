# AVIONIX NEXT — G3 EMS responsive state matrix batch 20

Data: 2026-09-18

Attività: `AVX-G3-04` / `PRJ-004`

Stato: VERIFIED-SW / VERIFY-HW

## Venti punti implementati

| ID | Controllo | Esito |
|---|---|---|
| HMI-M20-01 | schema configurazione V1 e radice JSON | PASS |
| HMI-M20-02 | proprietà configurazione chiuse | PASS |
| HMI-M20-03 | classificazione software-only | PASS |
| HMI-M20-04 | ruolo EMS e fixture corretti | PASS |
| HMI-M20-05 | risoluzioni 800×480, 1024×600, 1280×800 ordinate | PASS |
| HMI-M20-06 | sei stati canonici ordinati | PASS |
| HMI-M20-07 | record di stato a schema chiuso | PASS |
| HMI-M20-08 | età deterministiche per stato | PASS |
| HMI-M20-09 | severità e visibilità età coerenti | PASS |
| HMI-M20-10 | invarianti responsive fail-closed | PASS |
| HMI-M20-11 | dichiarazione golden non interattiva | PASS |
| HMI-M20-12 | schema manifesto golden chiuso | PASS |
| HMI-M20-13 | manifesto senza falsa hardware acceptance | PASS |
| HMI-M20-14 | provenance platform/role/fixture | PASS |
| HMI-M20-15 | timestamp UTC verificabile | PASS |
| HMI-M20-16 | esattamente 18 catture | PASS |
| HMI-M20-17 | prodotto cartesiano 3×6 unico e ordinato | PASS |
| HMI-M20-18 | record e nomi file sicuri | PASS |
| HMI-M20-19 | file, dimensioni e byte count verificati | PASS |
| HMI-M20-20 | SHA-256 file e pixel verificati | PASS |

## Risultato visuale

- 800×480 conserva il layout compact senza sovrapposizioni critiche.
- 1024×600 e 1280×800 mantengono la gerarchia expanded, con gauge, CAS,
  airframe, fuel, electrical e matrice cilindri leggibili.
- `STALE/DEGRADED` restano ambra e mostrano `0.6s` a ogni risoluzione.
- `LOST/FAIL` restano rossi, invalidano fuel/electrical/CHT/EGT e sopprimono
  endurance, hottest cylinder e spread a ogni risoluzione.
- `LIVE` e `SIM` restano distinguibili; SIM conserva cue testuale e ciano.

## Evidenza software

- 18 PNG native Windows generate dal renderer Qt e ispezionate.
- Manifesto `avionix.ems-responsive-state-golden.v1` con dimensioni, byte,
  SHA-256 del file e SHA-256 dei pixel.
- Validatore fail-closed e test di mutazione per tutti i venti controlli.
- `DEGRADED` aggiunto agli stati canonici del validatore golden generale.

## Limiti

- Nessuna prova è stata eseguita su RPi4 o RPi5 in questo incremento.
- Golden e digest non qualificano touch, sunlight/night, ≥40 FPS, termica,
  reboot, fault fisico o replay EDP.
- I limiti motore rimangono `UNVERIFIED` fino al manuale applicabile alla
  specifica installazione.
