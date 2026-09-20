# EFIS NEXT — Inventario read-only delle release attive

Data acquisizione: 2026-09-20 03:48 UTC

Stato prodotto: DEVELOPMENT / NON-CERT

Modalità: SSH read-only; nessun riavvio, deployment, switch di symlink o modifica
di configurazione.

## Scopo e metodo

Sono stati eseguiti cinque controlli su ciascuno dei quattro nodi principali:
identità/tempo/architettura, root o symlink release, stato dell'unità, processo
effettivo e metadati/hash del binario. Il risultato documenta lo stato osservato
in quell'istante; non costituisce acceptance al volo né prova di rollback.

Chiave operatore usata per il solo accesso:
`SHA256:eYHW5PBYxymnOUqbcQuPYb6P5qLyBn7AhcRZ+BOFbPA`
(`codex-efis-pfd-01`). Nessuna chiave privata o credenziale è inclusa.

## Esito dei 20 controlli

| # | Nodo | Controllo | Esito | Evidenza sintetica |
|---:|---|---|---|---|
| 1 | PFD `.45` | identità | PASS | `efis-pfd-01`, `aarch64` |
| 2 | PFD `.45` | release root | PASS | `/home/efis/efis-next-current` → `/home/efis/efis-next-avionics-60b3526` |
| 3 | PFD `.45` | supervisione | PASS | `efis-pfd.service` loaded/active/running, PID `31417`, `NRestarts=0` |
| 4 | PFD `.45` | processo | PASS | `.../efis-next-current/build-rpi5/efis-pfd-qt` |
| 5 | PFD `.45` | hash | PASS | PFD SHA-256 `86055f3b9712949d0e5b550cff40f5b4433858656f5591f2ded45fde8479d73d` |
| 6 | MFD `.49` | identità | PASS | `pi5`, `aarch64` |
| 7 | MFD `.49` | release root | PASS | `/home/pi/efis-next-current` → `/home/pi/efis-next-avionics-3432402` |
| 8 | MFD `.49` | supervisione | PARTIAL | `efis-mfd.service` non esiste; supervisore effettivo non identificato |
| 9 | MFD `.49` | processo | PASS | PID `1330`, `.../efis-next-current/build-rpi5/efis-pfd-qt` |
| 10 | MFD `.49` | hash | PASS | SHA-256 `5c3c1a42ad1f068fa742f2f3bb2f5225a9ba58a97bde6bf6145baf5b5abed64e` |
| 11 | EMS `.36` | identità | PASS | `efis`, `aarch64` |
| 12 | EMS `.36` | release root | FAIL-CLOSED | `/home/pi/efis-next-current` non è un symlink a release immutabile |
| 13 | EMS `.36` | supervisione | PARTIAL | `efis-ems.service` non esiste; fuel event forwarder attivo |
| 14 | EMS `.36` | processo | NON CONFORME | PID `4430`, `/home/pi/efis-next/build/dev/efis-ems-qt` |
| 15 | EMS `.36` | hash/manifest | NON DISPONIBILE | nessun binario/manifest sotto `efis-next-current` |
| 16 | Blackbox `.47` | identità | PASS | `efisblackbox`, `aarch64` |
| 17 | Blackbox `.47` | release root | FAIL-CLOSED | nessun symlink o manifest di release corrente |
| 18 | Blackbox `.47` | supervisione | PASS | `efis-blackbox-broker.service` loaded/active/running, PID `2448`, `NRestarts=0` |
| 19 | Blackbox `.47` | processo | PARTIAL | `/opt/efis-blackbox/bin/efis-blackbox-broker`; opzione `--engine-simulation` attiva |
| 20 | Blackbox `.47` | hash | CONFLITTO | attivo `648c7c...f8e0`; copia `.e916548` `c621c2...c712`, quindi non coincidono |

## Anomalie aggiuntive osservate

- Sul PFD è ancora in esecuzione un recorder legacy da
  `/home/efis/efis-next/build/dev/efis-recorder`, fuori dalla release immutabile
  corrente. I forwarder fuel ed emergency risultano attivi.
- Il processo MFD usa la release attesa ma non è governato dall'unità nominativa
  interrogata; boot, restart policy e proprietà del processo restano da
  ricostruire senza assumerli.
- Il processo EMS proviene dal checkout mutabile `build/dev`; questa condizione
  impedisce di identificare e riprodurre con certezza la release attiva.
- La Blackbox dispone di numerosi servizi di supporto attivi, ma non di un
  manifest univoco di nodo. Il broker corrente non può essere chiamato
  `e916548` sulla sola base del nome della copia affiancata.

## Decisione fail-closed

Solo PFD è `ATTIVA CONFERMATA` come release applicativa immutabile. MFD è
confermato come processo e binario, ma resta `PARTIAL` finché il supervisore non
è identificato. EMS e Blackbox non hanno una release immutabile identificabile:
restano `NON CONFORME`/`NON IDENTIFICATA`. Nessun requisito hardware viene
promosso a `DONE` con questa acquisizione.
