# EFIS NEXT — Registro canonico delle release sui nodi

Aggiornamento documentale: 2026-10-19
Stato: DEVELOPMENT / NON-CERT

Questo file è l'unica fonte documentale autorizzata per rispondere alla domanda
«quale release è attiva su un nodo?». I checkpoint restano evidenze storiche e
non possono sovrascrivere questo registro.

## Regola di lettura

- `ULTIMA OSSERVATA` significa che la release è stata letta sul nodo nella data
  indicata; non dimostra che sia ancora attiva oggi.
- `ATTIVA CONFERMATA` richiede una nuova lettura read-only del symlink o del
  processo in esecuzione, più commit completo e timestamp UTC.
- `DA RICONFERMARE` è il solo stato corretto quando esistono evidenze storiche
  successive ma manca una lettura corrente.
- Una build, un test ARM64, un archivio trasferito o una release disponibile per
  rollback non valgono come prova di attivazione.

## Registro corrente

| Nodo | Ruolo | Indirizzo noto | Ultima release osservata | Data evidenza | Stato canonico corrente | Evidenza | Prossima verifica |
|---|---|---:|---|---|---|---|---|
| `efis-pfd-01` | PFD + recorder RPi5 | `192.168.1.45` | `pfdrec-7051a47-retention-v1` (`7051a47839389f739f2d1e0ad22b385173299f61`) | 2026-09-30 activation + reboot | ATTIVA IMMUTABILE / RETENTION VERIFY-HW | archivio pre-potatura serverha 9.208 file/33.777.901.735 byte con secondo `rsync -aHnc --delete` a zero differenze; build ARM64 e test mirati PASS; prima potatura 2.222 segmenti/8.008.825.406 byte, rollback drill e reboot PASS; una seconda potatura post-reboot ha rimosso 12 segmenti/43.890.672 byte; PFD/recorder attivi dai binari canonici, status V3 crescente, DDP LIVE e `get_throttled=0x0`; root 71% con 17,4 GB disponibili; candidata meteo `pfdrec-3c7bb4d-weather-async-v1` preparata soltanto a livello software e non attivata | eseguire sul PFD build/test ARM64, attivazione con rollback, prova I/O meteo e reboot acceptance della candidata; completare soak >=24 h; indagare startup retry; visuale/touch/terrain e 27 gap tile restano aperti |
| `pi5` | MFD RPi5 | `192.168.1.49` | `mfd-3432402-primary-v1` (`3432402ae35c168a14a14fe408a2b635fe750c92`) | 2026-09-29 18:48 UTC | ATTIVA IMMUTABILE / POWER VERIFY-HW | unità attiva, eseguibile canonico, `NRestarts=0`; `get_throttled=0x50000` con due undervoltage kernel il 2026-09-23 | qualificare alimentatore/cavo, poi acceptance touch/terrain e soak |
| `efis` | EMS RPi4 | `192.168.1.36` | `ems-62ba90b-hmi-cas-v1` (`62ba90bcc53799303af2fa1de5b8ec3b1b5fb35f`) | 2026-09-29 18:48 UTC | ATTIVA IMMUTABILE CONFERMATA / SIM | manifesto HMI/CAS, unità attiva, eseguibile canonico, `NRestarts=0`, `simulation_mode=true`, `get_throttled=0x0` | sorgenti motore reali, calibrazioni, touch e soak restano aperti |
| `efisblackbox` | Blackbox RPi5 | `192.168.1.47` | `blackbox-7655799-primary-v1` (`765579959f7970a17eb212ca1ef13628aaa4d71f`) + drop-in NAV `4a2e33ce...` | 2026-09-29 19:36 UTC | ATTIVA IMMUTABILE / SIM / POWER VERIFY-HW / NAV FAIL-CLOSED | broker attivo `NRestarts=0`, `--engine-simulation`; drop-in esterno alla release ferma `bbxnav` su exit 2 con `NRestarts=0`; cinque transienti recenti senza errori NVMe/USB correlati | qualificare alimentatore/cavo; integrare il drop-in nella prossima release e qualificare ricevitori NAV; CAN reale, endurance e fault restano aperti |
| `ems-sensor` | ASSO5 sensor RPi4 | `192.168.1.89` | nessuna release distribuita | 2026-09-18 | NON DISTRIBUITA | identità nodo verificata; adapter reale non distribuito | nessun deployment prima di inventario e approvazione G1 |
| `PORTATILE` | Ground station | `192.168.1.39` | non applicabile/untracked | 2026-09-15 | DA INVENTARIARE | QGC e firewall osservati separatamente | registrare package/versioni/config hash in sola lettura |
| `serverha` | Server mappe | `192.168.1.6` | non applicabile/untracked | 2026-09-14 | DA INVENTARIARE | container osservati Up; nessun release manifest canonico | acquisire image digest e compose revision |

## Conflitti chiusi documentalmente

I valori PFD `1d25e62`, `93bbe44`, `e902973` e `60b3526` descrivono una
sequenza storica, non quattro stati correnti simultanei. La lettura hardware
del 2026-09-20 confermava `60b3526`; il rollout del 2026-09-21 ha poi attivato
la release dual-role `pfdrec-d52e377-primary-v1`. Per lo stesso motivo
`f27508a`, `1d02b84` e
`3432402` sono la sequenza storica MFD; la lettura corrente conferma il processo
e il binario `3432402`, ma non il suo supervisore.

L'inventario iniziale è documentato in
[`evidence/release-inventory-20260920/README.md`](evidence/release-inventory-20260920/README.md).
La migrazione autorizzata MFD/EMS/Blackbox e le prove di rollback/reboot sono in
[`evidence/release-immutability-20260920/README.md`](evidence/release-immutability-20260920/README.md).
Nessuna riga di questo registro autorizza deployment, rollback o uso in volo.

## Riconferma read-only del 2026-10-17

Le quattro release del registro sono state rilette tramite SSH, manifesto e
processo, e rimangono attive senza sostituzioni. Copie dei manifesti ed esiti
sono in `evidence/rollout-20261017/`. Questa osservazione aggiorna le date
storiche della tabella senza promuovere i gate ancora aperti.

| Nodo | Release riconfermata | Processo | Gate rollout main a0be9a5 |
|---|---|---|---|
| PFD `.45` | `pfdrec-7051a47-retention-v1` | PFD 1495, recorder 1419 | `0x0`; attende canary MFD |
| MFD `.49` | `mfd-3432402-primary-v1` | 1395, NRestarts 0 | `0x50000`; solo staging, niente attivazione |
| EMS `.36` | `ems-62ba90b-hmi-cas-v1` | 949 | `0x0`; sudo non interattivo richiede password |
| Blackbox `.47` | `blackbox-7655799-primary-v1` | broker 1301, NRestarts 0 | `0x50000`; bbxnav exit 2, NRestarts 0 |

Il puntatore Blackbox effettivo è `/opt/efis-blackbox-current`, risolto in
`/opt/efis-blackbox-releases/blackbox-7655799-v1`, non un path sotto `/home/pi`.
Il sensore `.89` non risponde alla connessione SSH: nessuna nuova attestazione
del suo stato, nessun deployment prima di G1. Il meteo Blackbox termina il
refresh osservato con exit 0; l'attività transitoria non è stata scambiata per
un errore. I bit `0x50000` sono storici, non prova di sottotensione attiva.
Nessun reboot, servizio riavviato o puntatore modificato in questo controllo.

Candidata MFD separata: `mfd-a0be9a5-main-v1`, sorgente main
`a0be9a52494ea7ccb5a3890e94d03b671162383b`, sotto
`/home/pi/efis-next-release-mfd-a0be9a5-v1`. Build ARM64, cinque gruppi CTest,
manifesti candidata e rollback PASS. **STAGING SOLTANTO**, non root-owned e
non attivata; no acceptance visuale/reboot/rollback drill della nuova release.

## Preparazione autonoma del 2026-10-18 — nessuna attivazione

Evidenze: `evidence/rollout-20261018/`. Main resta a0be9a5; il MFD esegue
15/15 regressioni software ARM64 più un test storage recorder PASS.

| Candidata | Nodo dove depositata | Stato |
|---|---|---|
| `mfd-a0be9a5-main-v1` | MFD `.49`, path della sezione precedente | Staging, power gate invariato |
| `pfdrec-a0be9a5-main-v1` | PFD `.45`, `/home/efis/efis-release-pfdrec-a0be9a5-stage/pfdrec-a0be9a5-main-v1` | Due artefatti compilati su MFD RPi5; manifesti candidata/rollback e dipendenze verificati sul PFD, nessuno switch |
| `blackbox-a0be9a5-main-v1` | Blackbox `.47`, `/home/pi/efis-release-blackbox-a0be9a5-stage/blackbox-a0be9a5-main-v1` | Broker compilato su MFD RPi5; manifesti/dipendenze PASS sul destinatario; mai eseguito |
| `ems-a0be9a5-main-v1` | EMS `.36`, `/home/pi/efis-next-releases/ems-a0be9a5-main-v1` | Build nativa RPi4, 7/7 test, manifesti/dipendenze e smoke QML isolato PASS; non attivata |

I pacchetti depositati restano staging non sigillato root-owned. Le release
attive sono ancora quelle riconfermate il 17; il 18 i PID PFD 1495, recorder
1419, MFD 1395, Blackbox 1301 sono invariati. Il PFD riporta NRestarts=1
storico e recorder/MFD/Blackbox 0: non attribuire quell'1 a questo lavoro.
Nessuna variazione a retention, configurazione, rete, unità o bus.
Anche EMS conserva current `ems-62ba90b-hmi-cas-v1`, PID 949/NRestarts 0;
la cattura offscreen della candidata non è una cattura del pannello operativo.

Il preflight read-only del 19 riconferma i medesimi current, manifesti e PID
sui quattro nodi (`evidence/activation-preflight-20261019/`). PFD supera i
controlli automatici ma non è autorizzato prima del canary; MFD/Blackbox hanno
power history 0x50000, EMS richiede finestra sudo operatore. Nessuno switch.

Preparato sul solo EMS l'attivatore pinned `activate_ems_a0be9a5.py`, preflight
read-only PASS e 17 test software ARM64 PASS; istruzioni in
`docs/deployment/EMS_A0BE9A5_ACTIVATION.md`. Il target sigillato previsto sotto
`/opt/efis-next-ems-releases/` NON è ancora stato creato/attivato: corrente
resta `ems-62ba90b-hmi-cas-v1`, PID 949/NRestarts 0 alla verifica di consegna.

## EMS: osservazione post-attivazione operatore — 2026-10-19 19:18 UTC

Questa osservazione sostituisce per EMS lo stato di sola preparazione sopra.
Current `/home/pi/efis-next-current` ora punta al target root/read-only
`/opt/efis-next-ems-releases/ems-a0be9a5-main-v1`, sorgente
`a0be9a52494ea7ccb5a3890e94d03b671162383b`. Servizio active/running,
PID 46812, NRestarts 0, ExecMainStatus 0; attivo dalle 19:17:07 UTC.
Manifesto `9bd369facf5a348ec83d9dd6f9eaa388f7dc90f9b839861a62cd67528095e625`
e binario `8256045bf8707a7c269562799dafdb79f6ab4bee21341946aeafcf0dcfce33f6`
verificati. Rollback reale candidata→precedente→candidata PASS, precedente
`ems-62ba90b-hmi-cas-v1` conservata e validata. Power 0x0, EDP BLACKBOX 47 SIM
e heartbeat rendering positivi. Evidenza `ems-activation-20261019/post-activation.json`.
**Reboot e acceptance visuale/touch ancora NOT_RUN; nessuna approvazione al volo.**
Gli altri nodi non sono stati attivati o riavviati in questa verifica.
