# EFIS NEXT — Registro canonico delle release sui nodi

Aggiornamento documentale: 2026-10-26
Stato: DEVELOPMENT / NON-CERT

R1-20: installati credential/helper di banco SIL ristretto serverha→PFD,
non nuova release avionica. Renderer7051a47 e puntatore PFD invariati;
nessuna modifica release MFD/EMS/Blackbox. Due run60s bounded con ritorno
automatico verificato; finale UTC2026-10-26T21:39:46Z PFD attivo PID1745346,
recorder1419 invariato,power0x0,nessun bench attivo. Helper root persistenti,
adapter SHA7d1c5880 invariato; vedere evidence/r1-20-restricted-sil-ssh-20261026.

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

R1-19, UTC2026-10-26T21:19:42Z: prova fisica LIVE terminata, PFD normale
nuovamente attivo PID1743673, recorder1419 invariato/0x0. Stesso binario e
pointer7051a47; solo adapter di banco r119-build-v4 aggiunto in cartella
separata, NON release prodotto. Doppia finestra live da60s verificata, nessuna
modifica MFD/EMS/Blackbox. Stato grafico/touch e qualificazione restano aperti.

R1-18 osservato UTC2026-10-26T13:35:11Z: stesso pointer PFD e binario
7051a47/SHA a64edcf2d0fcf5f64f30d771926a60d4e0c2854763a2f082375c7630df828854.
Sessione transitoria SIM/replay virata, lease900s e backstop930s; ripristino
normale verificato prima di riarmare. Recorder1419 invariato/power0x0.
Non inferire che la sessione sia ancora attiva oltre la lease. Nessuna nuova
release permanente o cambio MFD/EMS/Blackbox; touch PFD ancora non accettato.

Osservazione R1-17 del2026-10-25: PFD.45 ha eseguito la STESSA release
7051a47 in una sessione temporanea PrivateNetwork/SIM node65000 da unità
transitoria, con l'unità utente normale sospesa per la durata del banco.
Pointer e hash renderer invariati; ripristino originale verificato nel drill.
Sessione finale riarmata a UTC24T23:28:36Z, vista23:28:46Z, lease900s: non dedurre che sia ancora
attiva leggendo questa nota più tardi. Recorder PID1419 invariato,0x0.
Non è un rollout di R1-16/R1-17 sui renderer. Dettagli/evidenze nel registro
stato corrente R1-17; MFD/EMS/Blackbox non modificati.

| Nodo | Ruolo | Indirizzo noto | Ultima release osservata | Data evidenza | Stato canonico corrente | Evidenza | Prossima verifica |
|---|---|---:|---|---|---|---|---|
| `efis-pfd-01` | PFD + recorder RPi5 | `192.168.1.45` | `pfdrec-7051a47-retention-v1` (`7051a47839389f739f2d1e0ad22b385173299f61`) | 2026-09-30 activation + reboot | ATTIVA IMMUTABILE / RETENTION VERIFY-HW | archivio pre-potatura serverha 9.208 file/33.777.901.735 byte con secondo `rsync -aHnc --delete` a zero differenze; build ARM64 e test mirati PASS; prima potatura 2.222 segmenti/8.008.825.406 byte, rollback drill e reboot PASS; una seconda potatura post-reboot ha rimosso 12 segmenti/43.890.672 byte; PFD/recorder attivi dai binari canonici, status V3 crescente, DDP LIVE e `get_throttled=0x0`; root 71% con 17,4 GB disponibili; candidata meteo `pfdrec-3c7bb4d-weather-async-v1` preparata soltanto a livello software e non attivata | eseguire sul PFD build/test ARM64, attivazione con rollback, prova I/O meteo e reboot acceptance della candidata; completare soak >=24 h; indagare startup retry; visuale/touch/terrain e 27 gap tile restano aperti |
| `pi5` | MFD RPi5 | `192.168.1.49` | `mfd-3432402-primary-v1` (`3432402ae35c168a14a14fe408a2b635fe750c92`) | 2026-09-29 18:48 UTC | ATTIVA IMMUTABILE / POWER VERIFY-HW | unità attiva, eseguibile canonico, `NRestarts=0`; `get_throttled=0x50000` con due undervoltage kernel il 2026-09-23 | qualificare alimentatore/cavo, poi acceptance touch/terrain e soak |
| `efis` | EMS RPi4 | `192.168.1.36` | `ems-a0be9a5-main-v1` (`a0be9a52494ea7ccb5a3890e94d03b671162383b`) | 2026-10-25 preflight, UTC 24T22:19:58Z | ATTIVA CONFERMATA / SIM | current e /proc/948/exe risolti in /opt/efis-next-ems-releases/ems-a0be9a5-main-v1, manifesto SHA 9bd369fa..., servizio active/restart 0 e 0x0; sola osservazione, nessuna attivazione in R1-15 | sorgenti reali/calibrazione e acceptance display aperte; non commutare a SIL senza ingresso isolato/rollback e gate del sistema |
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

## Riconferma preflight SIL 2026-10-25 — nessuna attivazione

Questa osservazione successiva prevale sulle descrizioni storiche sopra.
SSH UTC 2026-10-24T22:19:57/58Z, data locale Europe/Rome 25 ottobre:

| Nodo | Evidenza letta ora | Esito e limite |
|---|---|---|
| PFD .45 | current pfdrec-7051a47-retention-v1; servizio UTENTE efis-pfd active, processo 1495 | puntatore/processo riconfermati; hash completo artefatti non ripetuto; 0x0 |
| MFD .49 | current mfd-3432402-primary-v1; servizio direct active, PID 1328/restart 0 | puntatore/processo riconfermati, hash non ripetuto; 0x50000 e undervoltage recenti |
| EMS .36 | current /opt/efis-next-ems-releases/ems-a0be9a5-main-v1; /proc/948/exe stesso binario; servizio active/restart 0 | ATTIVA CONFERMATA da manifesto source a0be9a52494ea7ccb5a3890e94d03b671162383b, SIM=true; 0x0; nessun nuovo deployment in questo blocco |
| Blackbox .47 | current /opt/efis-blackbox-releases/blackbox-7655799-v1; servizio active, PID 1294/restart 0 | puntatore/processo riconfermati, hash non ripetuto; 0x50000; broker/endpoint non modificati |

Manifesto EMS SHA256: 9bd369facf5a348ec83d9dd6f9eaa388f7dc90f9b839861a62cd67528095e625.
I percorsi della vecchia candidata EMS sotto /home/pi descrivevano lo staging,
non il runtime ora osservato sotto /opt. L'attivazione intermedia non è
attribuita a questo lavoro. Nessuna unità/puntatore cambiato, nessun reboot.
