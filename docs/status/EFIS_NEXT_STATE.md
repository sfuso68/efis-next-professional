# EFIS NEXT — stato verificato

Aggiornamento: 2026-09-09

Branch di lavoro: `work/avionics-ethernet-v1`

Base documentale letta prima delle modifiche: `CLAUDE_CODE_HANDOFF_2026-08-21.md`, `CHECKPOINT-2026-08-21.md`, `CHECKPOINT-2026-09-07-BLACKBOX-DDP.md` e ADR-001…ADR-010.

Checkpoint corrente: `CHECKPOINT-2026-09-09-PIXHAWK-ETHERNET.md`. Il concept
consultivo `Emergency Assist / Panic V1` è fissato in
`docs/design/EMERGENCY_ASSIST_V1.md`; non esiste ancora codice della funzione e
non è stata concessa autorità di comando al sistema EFIS.

Questo documento non esisteva nel repository al momento dell'intervento ed è stato creato come stato operativo autorevole. Le parole **implementato**, **verificato software** e **verificato su hardware reale** indicano livelli di evidenza diversi. Una funzione non è considerata conclusa se manca la verifica pertinente.

## Implementato in questo branch

### Display Data Protocol V2 — codec snapshot

- Aggiunto un codec binario C++20 indipendente da Qt e dai trasporti in
  `efis::protocol::ddp`.
- L'envelope `EFN2` usa schema 2, ordine di byte di rete, nodo publisher,
  istanza di avvio, sequenza, tempo monotono, lunghezza e CRC32C Castagnoli.
- Lo snapshot trasporta health e 23 campioni di volo con segnale, valore,
  sorgente, validità, qualità, unità, età e flag simulazione.
- Il decoder rifiuta schema sconosciuto, magic errato, lunghezza incoerente,
  payload oltre 64 KiB, CRC errato e payload malformato.
- `SequenceGate` rifiuta duplicati e sequenze arretrate per nodo; una nuova
  istanza publisher consente la ripartenza controllata dopo reboot.
- ADR-008 registra formato, motivazioni e limiti.

### Display Data Protocol V2 — session layer

- Aggiunti `SnapshotPublisher` e `SnapshotConsumer`, indipendenti da Qt e dal
  tipo di socket.
- Il publisher assegna una sequenza wire propria e valida identità, istanza e
  profilo prima di produrre datagrammi.
- Il consumer ricostruisce lo `FlightSnapshot` senza scegliere sorgenti,
  conserva provenienza, unità, validità, qualità, age e flag simulazione e può
  essere vincolato all'identità della blackbox installata.
- Sono espliciti gli stati locali `WAITING`, `LIVE`, `STALE` e `LOST`.
- Le metriche distinguono pacchetti accettati, CRC errati, identità estranee,
  duplicati, sequenze arretrate, gap e riavvii del publisher.
- Un pacchetto corrotto, duplicato o fuori ordine non sostituisce l'ultimo
  snapshot valido.
- Il decoder V2 rifiuta inoltre segnali duplicati, booleani non canonici e
  snapshot che non contengono esattamente i 23 campioni previsti dallo schema.

### Display Data Protocol V2 — adapter UDP unicast

- Aggiunti socket UDP IPv4 non bloccanti e multipiattaforma dietro
  `efis::io`, senza dipendenze Qt nel core.
- Aggiunti `DdpUdpPublisher` e `DdpUdpFlightSource`: il primo invia snapshot
  completi, il secondo è una sorgente read-only consumabile dal router PFD.
- La sorgente PFD drena un numero limitato di datagrammi per ciclo, conserva le
  metriche e rende i campioni `STALE` e poi `UNAVAILABLE` alla perdita del link.
- Il controller PFD abilita la sorgente tramite `EFIS_DDP_BIND_ADDRESS`,
  `EFIS_DDP_LISTEN_PORT` ed `EFIS_DDP_SOURCE_NODE`; stato e metriche entrano nel
  log periodico `render_health`.
- Il servizio RPi5 è predisposto per ascoltare `0.0.0.0:49002`, nodo Blackbox
  logico `47`, con simulazione in fallback esplicitamente riconoscibile.
- Aggiunto `efis-ddp-bench-publisher`, che richiede il flag esplicito
  `--simulate` e non viene presentato come publisher di volo.
- ADR-009 sceglie UDP unicast per la fase di sviluppo e fissa i gate di rete e
  autenticazione ancora aperti.

### Publisher operativo Blackbox

- Aggiunto `BlackboxPublisherSession`, punto di composizione che collega lo
  stato della pipeline `MavlinkPipeline` al publisher DDP senza introdurre una
  sorgente simulata o un percorso di comando.
- Aggiunto `efis-blackbox-broker`: riceve MAVLink su UDP e, quando configurato,
  frame ESP32 su SocketCAN; pubblica a 20 Hz lo snapshot normalizzato verso il
  PFD tramite UDP unicast.
- Lo stesso snapshot distribuito viene scritto nel recorder segmentato della
  Blackbox, preceduto dagli eventi di nodo e cambio sorgente del ciclo.
- In assenza di dati il broker pubblica campioni non disponibili: il display
  può quindi distinguere il link DDP vivo dalla disponibilità dei sensori.
- L'eseguibile espone contatori per datagrammi/byte MAVLink, errori e
  troncamenti socket, frame CAN e datagrammi/byte DDP.
- Aggiunti unità systemd e guida di deployment; le sezioni hardware seguenti
  documentano la successiva installazione e attivazione sulla Blackbox reale.
- Il monitor dei nodi conserva ora anche le transizioni di recupero prodotte
  da un battito valido (`STALE/LOST -> ONLINE`) fino al successivo ciclo di
  ingest. In precedenza lo stato tornava online, ma l'evento veniva perso prima
  che adapter e recorder potessero osservarlo.

### Bridge Pixhawk MAVLink strettamente sola-lettura

- Aggiunto `tools/groundstation/mavlink_readonly_forwarder.py`, bridge Python
  senza dipendenze esterne che apre la seriale Pixhawk con `O_RDONLY`, non
  contiene un percorso di scrittura e inoltra i byte invariati su una o più
  destinazioni UDP.
- Il bridge non interpreta MAVLink, non genera heartbeat, non richiede stream
  e non può modificare parametri o stato dell'autopilota. Parsing,
  normalizzazione, arbitraggio e registrazione restano concentrati sulla
  Blackbox come richiesto da ADR-003.
- Aggiunti test con pseudo-terminale Linux, unità systemd utente, configurazione
  di esempio e runbook di installazione/manutenzione. Il percorso seriale usa
  l'identità USB persistente del Pixhawk 6X e non il nome variabile `ttyACM0`.
- QGroundControl deve ricevere la copia UDP locale e non aprire la stessa
  seriale mentre il bridge è attivo; le operazioni di manutenzione richiedono
  l'arresto esplicito del bridge.

### Piano Ethernet avionico segmentato

- Aggiunta ADR-010: la Blackbox resta broker applicativo e non diventa router o
  bridge IP. Pixhawk, Blackbox e display usano FLIGHT-DATA `10.77.10.0/24`;
  camere e VIDEO-CORE usano VIDEO `10.77.20.0/24`.
- Aggiunto `config/network/avionics-network-v1.json` con indirizzi statici,
  flussi UDP unicast, MTU 1500, assenza di DHCP/default gateway e separazione
  esplicita delle quattro camere.
- La targhetta fotografata conferma il Waveshare `Gigabit-Switch-5P (EU)`,
  plug-and-play non gestito. È destinato soltanto al banco FLIGHT-DATA e non è
  accettato come switch unico per telemetria e quattro flussi video.
- EMS e VIDEO-CORE restano computer distinti: il decoder/mosaico video non può
  condividere failure mode e carico con il nodo strumenti motore.
- Aggiunti runbook di migrazione reversibile e validatore offline del piano. I
  parametri `NET_*` del Pixhawk non vengono scritti da questo lavoro.
- Blackbox, PFD ed EMS reali sono collegati al Waveshare e configurati
  rispettivamente come `10.77.10.2/24`, `10.77.10.11/24` e
  `10.77.10.12/24`. Tutti hanno negoziato 1000 Mbit/s full duplex, MTU 1500 e
  nessun gateway Ethernet; la WLAN resta recovery separata.
- Il broker reale pubblica DDP a `10.77.10.11:49002`. Dopo l'assestamento, un
  campione di 30 secondi ha consegnato 601 snapshot senza nuovi gap, rifiuti,
  errori socket o overflow UDP kernel. Il PFD è rimasto `LIVE` a circa 25 fps.
- Un fault test logico di cinque secondi sul profilo Ethernet del PFD ha
  verificato `LIVE -> LOST -> LIVE`; broker e recorder sono rimasti attivi e
  senza failure. I 112 gap aggiunti sono coerenti con la finestra di perdita
  intenzionale. Non equivale ancora a una prova di scollegamento fisico.
- Installata e abilitata su Blackbox, PFD ed EMS la tabella nftables separata
  `inet efis_avionics`: le destinazioni `10.77.10.0/24` sono ammesse soltanto
  quando la route sceglie `eth0`. Con `eth0` Blackbox disattiva, Linux ha scelto
  il gateway WLAN ma la regola ha rifiutato 148 pacchetti; 146 invii DDP hanno
  prodotto `Operation not permitted` mentre il recorder continuava a crescere.
  Sul PFD e sull'EMS le prove simmetriche hanno rifiutato e contato due ping.
  Tutti i nodi hanno recuperato con servizio egress `enabled/active`; il DDP
  PFD è tornato `LIVE`.
- Dopo i fault, due finestre brevi hanno però mostrato gap DDP intermittenti
  anche a link attivo (10 su 390 snapshot in 20 secondi in una finestra, 6 su
  394 in un'altra), senza incremento di `RcvbufErrors`, errori driver o failure
  publisher. EEE era attivo su tutti i link; sul RPi4 il renderer `eglfs`
  senza affinità produceva inoltre 13–15% di perdita ICMP e burst fino a
  2.040 ms. Con EEE disabilitato, e tutti i thread EMS confinati a CPU1–3 per
  lasciare CPU0 agli IRQ Ethernet, il collaudo successivo ha dato 200/200 ping
  EMS con massimi 0,276/0,324 ms. In parallelo il PFD ha accettato 900 snapshot
  senza nuovi gap; una prova separata Blackbox-PFD ha dato 300/300 ping e
  1.201 snapshot senza gap in 60 secondi. La mitigazione è installata e
  verificata a runtime. Sul nodo EMS anche il rollback del servizio è stato
  provato (`EEE enabled-inactive`) e il successivo start ha ripristinato
  `EEE disabled`; endurance e reboot restano aperti.

### Distribuzione DDP multi-display

- Il broker accetta più opzioni `--display` e crea per ciascuna un'uscita UDP
  unicast indipendente, mantenendo un solo stato arbitrato e un solo recorder.
- Tutte le uscite devono condividere profilo, nodo, istanza e sequenza iniziale;
  destinazioni duplicate o identità divergenti sono rifiutate in configurazione.
- Errori di invio sono isolati per destinazione e non interrompono recorder o
  gli altri pannelli; metriche aggregate e per-display sono esposte nel log.
- L'unità systemd usa `EFIS_DDP_DISPLAY_ARGS` per rendere configurabile la lista
  degli endpoint senza duplicare processi di ingestion.
- Aggiunto il workflow `EFIS Core CI`, che configura e prova su Linux il core e
  i processi senza Qt. La prima esecuzione completa sulla PR #6 ha superato
  configurazione, build e test.

### Inventario firmato dei segmenti CAN della Blackbox

- Aggiunto il comando `seal-can` a `tools/blackbox/fuel_event_maintenance.py`.
- Il registratore legacy `bbxlog` e il formato binario `FUSOBBX1` non sono stati modificati.
- Il comando considera soltanto segmenti chiusi `bbx-*.bbx`; i file aperti `*.bbx.part` sono esclusi.
- Ogni segmento viene controllato per magic, versione, dimensione record e numero interfacce.
- Un record finale incompleto viene classificato `RECOVERED_TRUNCATED`; un header non valido viene classificato `REJECTED_*`.
- Per ogni segmento sono registrati SHA-256, dimensione, timestamp, numero di record, byte finali e stato formato.
- Il manifesto complessivo `can-segment-manifest.json` è scritto atomicamente e accompagnato da SHA-256 e HMAC-SHA256.
- Il riuso del digest è ammesso solo se dimensione, `mtime_ns` e `ctime_ns` coincidono. I segmenti eliminati dalla rotazione sono eliminati anche dal manifesto successivo.
- L'hashing è eseguito a blocchi da 1 MiB, evitando il caricamento completo del segmento in RAM.
- La verifica manutentiva comprende ora sia i pacchetti fuel/incident sia il manifesto CAN.

### Servizio Blackbox

- Aggiunti `efis-can-seal.service` e `efis-can-seal.timer`.
- Il servizio gira come utente `blackbox`, con `Nice=10` e classe I/O `idle`.
- `/var/lib/blackbox/can` è montato in sola lettura nel sandbox systemd; la sola area scrivibile è `/var/lib/blackbox/ethernet`.
- Pianificazione: due minuti dopo il boot e poi ogni cinque minuti, con ritardo casuale massimo di 30 secondi.
- Il journal riceve solo il riepilogo del ciclo, non l'intero manifesto.

### API e display

- `/v1/archives` integra i 50 segmenti CAN più recenti e un riepilogo completo (`segments`, `valid`, `rejected`, `bytes`, stato e timestamp del manifesto).
- Il server pubblica i dati CAN solo quando SHA-256 e HMAC del manifesto sono validi.
- Il forwarder esistente non richiede variazioni: trasferisce l'intera risposta `/v1/archives` nella cache locale autenticata.
- I controller EMS e PFD espongono `centralBlackBoxArchiveSummary`.
- La pagina setup Blackbox è predisposta a mostrare conteggio CAN, pacchetti firmati e stato manifesto.

### Catalogo operativo aeromobile V2 — 2026-09-09

- Aggiunto un dizionario controllato di 447 parametri in 28 domini per modello,
  variante ed esemplare installato. Ogni parametro viene materializzato con
  `VALUE`, `INHERIT`, `NOT_APPLICABLE`, `NOT_INSTALLED` oppure `UNKNOWN`; non
  esiste omissione silenziosa.
- Aggiunti schema SQLite V2, provenance campo per campo, revisioni/rilasci,
  risultati di validazione, tipizzazione e gate per dati safety-critical.
- Aggiunti un modello Asso V Jolly/Rotax 912 UL e il relativo overlay di
  esemplare. Entrambi restano `SIMULATION`: i dati noti sono indicativi e i
  numeri legacy non documentati non sono stati promossi.
- Il database generato conserva 70 aeromobili e 24 motori del catalogo V1 e
  aggiunge 2 profili, 447 definizioni e 894 valori materializzati, senza campi
  mancanti. Registra inoltre un report di validazione strutturale per ciascun
  profilo e distingue esplicitamente `release_ready=false`.

### Editor Setup del catalogo operativo V2 — 2026-09-09

- L'indice generato espone ora i 28 domini, le 447 definizioni tipizzate e, per
  ciascun profilo, valore grezzo, valore effettivo risolto lungo l'eredità,
  disposizione, provenienza e riepilogo dei blocker.
- PFD ed EMS condividono lo stesso modello C++ e la stessa pagina QML
  `OperationalProfileSetup`: ricerca, selezione per dominio, confronto dello
  stato, modifica tipizzata e riferimenti a documento/revisione/pagina.
- Ogni modifica viene salvata atomicamente nel formato separato
  `efis-next.operational-profile-draft.v2`. Il file dichiara
  `activation_allowed=false` e non è leggibile dal runtime come profilo attivo.
- Non esiste nella pagina un comando di attivazione. I campi `UNKNOWN` e i
  blocker safety-critical restano visibili; `release_ready` è forzato a falso.
  Un profilo radice non può usare `INHERIT` e N/A/non installato richiedono una
  motivazione.

## Verificato solo software

### Catalogo operativo aeromobile V2

- `tests/catalog_tool_tests.py`: 11/11 test PASS con Python 3.12; coperti
  dizionario, unicità, materializzazione completa, eredità, rifiuto di chiavi
  sconosciute, obbligo della motivazione per N/A/non installato, build SQLite e
  conservazione atomica del catalogo precedente quando la rigenerazione fallisce;
  il registro Markdown contiene tutti e 447 gli identificativi.
- `PRAGMA integrity_check=ok`; 447 definizioni, 2 profili, 894 valori e zero
  coppie profilo/parametro mancanti. Validazione del seed: struttura PASS.
- La struttura PASS non significa pronta al volo: il modello segnala 309
  blocker safety-critical e 429 campi `UNKNOWN`; l'esemplare aggiunge sei
  override `UNKNOWN`. Nessuna verifica su hardware reale è stata eseguita per
  questo catalogo.
- Build MSVC 19.51/Qt 6.11 completata per PFD ed EMS con compilazione AOT della
  nuova pagina QML. Il test `efis_operational_profile_model_tests` passa e
  verifica selezione profilo, isolamento della bozza, tipizzazione, motivazione
  obbligatoria, divieto di eredità sul profilo radice, scrittura atomica,
  `activation_allowed=false`, filtro e cancellazione della sola bozza.
- Smoke test EMS con backend Qt offscreen concluso con exit code 0. È una prova
  di caricamento software, non un'accettazione visiva sul pannello reale.
- La build completa Windows del branch è riuscita. Nel tentativo CTest completo
  14 test preesistenti sono passati; Windows Application Control ha bloccato
  prima dell'avvio `efis_safety_tests.exe` e il run non ha completato il test
  successivo entro il timeout. Il nuovo test Setup è stato quindi rieseguito
  separatamente con CTest ed è passato. I test non avviati non sono dichiarati
  superati da questa sessione.

- Build MSVC 19.51 del codec e del nuovo eseguibile di test completata.
- Round-trip snapshot, preservazione provenienza/unità/simulazione, CRC32C con
  vettore standard, corruzione, schema futuro, duplicati, riordino e reboot del
  publisher verificati da `efis_ddp_v2_tests`.
- Sessione publisher/consumer verificata nello stesso test per sequenza propria,
  ricostruzione, identità attesa, timeout, perdita, CRC, duplicati, riordino,
  reboot, conservazione ultimo dato valido e reset diagnostico.
- Build completa MSVC 19.51 del branch eseguita dopo il session layer: tutti i
  target, inclusi PFD, EMS e strumenti, compilati e 14 test su 14 superati.
- Build MSVC del receiver PFD, del publisher da banco e del test UDP completata.
- `efis_ddp_udp_tests` ha verificato con socket loopback reale ricezione,
  provenienza, identità nodo, metriche, transizioni `LIVE/STALE/LOST` e rifiuto
  di un publisher estraneo.
- Una precedente esecuzione Windows aveva visto due test bloccati prima
  dell'avvio da Application Control (`BAD_COMMAND`). La verifica completa più
  recente di questo branch ha compilato tutti i target e superato **15 test su
  15**, inclusi `efis_core_tests` ed `efis_engine_runtime_tests`.
- Build MSVC di `efis-blackbox-broker`, PFD e test UDP completata dopo
  l'innesto del publisher operativo.
- Il test UDP end-to-end ora introduce frame MAVLink V2 nella pipeline
  Blackbox e verifica sul consumer DDP velocità normalizzata, provenienza
  Pixhawk, assenza del marcatore simulato e metriche di pubblicazione; rilegge
  inoltre il segmento locale e ritrova la stessa misura e provenienza.
- Esecuzione locale del broker per un secondo, senza CAN e senza ingresso
  MAVLink, riuscita: 21 snapshot esplicitamente non disponibili pubblicati e
  21 registrati, zero errori socket. È una verifica software del ciclo, non
  hardware.
- Test fan-out software verso due consumer UDP: entrambi ricevono lo stesso
  snapshot normalizzato; dopo la rimozione di un consumer il rimanente continua
  a ricevere e il recorder continua a scrivere. Verificati inoltre contatori
  aggregati/per-endpoint e rifiuto di destinazioni duplicate.
- La build MSVC completa del branch è riuscita. Dopo il relink Windows
  Application Control ha bloccato prima dell'avvio tutti i 15 eseguibili di
  test (`BAD_COMMAND`), senza eseguire asserzioni. Nessuno di questi tentativi è
  dichiarato superato; è stata quindi richiesta una verifica indipendente.
- Il job indipendente `linux-core` della PR #6 ha completato con esito `success`
  gli step configure, build e test, includendo il test fan-out multi-display.
- `py_compile` del bridge e del relativo test è riuscito con Python 3.13 sia
  sul sistema di sviluppo sia sul portatile Debian. Il test Linux con
  pseudo-terminale ha verificato che un payload binario MAVLink-like di 10 byte
  viene ricevuto via UDP identico, con un solo datagramma e zero errori. Tre
  test su tre sono passati, includendo validazione destinazioni e retry
  controllato quando la seriale non esiste.
- Il piano Ethernet V1 ha superato cinque test Python: schema repository,
  rifiuto indirizzo duplicato, rifiuto camera su FLIGHT-DATA e rifiuto di un
  gateway sulla rete operativa, oltre al rifiuto di uno switch FLIGHT-DATA
  sottodimensionato. Il comando di validazione riporta 2 segmenti, 11 nodi, 3
  flussi obbligatori e 2 switch distinti.

- Tre test unitari Python su segmenti sintetici `FUSOBBX1`, eseguiti con Python 3.13.5:
  - segmento chiuso valido, firma valida e riuso digest;
  - esclusione del corrispondente `.bbx.part`;
  - record finale incompleto classificato recuperabile senza perdita dei record completi;
  - magic errato rifiutato;
  - segmento ruotato/rimosso eliminato dal manifesto.
- `py_compile` superato per `fuel_event_maintenance.py` e `fuel_event_hub.py`.
- Generazione della cache QML e compilazione degli oggetti C++ PFD contenenti la nuova proprietà completate su RPi5.
- Il link finale del nuovo binario PFD **non è verificato**: la copia sorgente sul target ha un drift precedente e il suo `CMakeLists.txt` non include implementazioni già richieste da `pfd_controller.cpp` (`EngineSourceRouter`, runtime motore e simulatore di sistema). Non è stato installato alcun binario UI parziale.

## Verificato su hardware reale

### Blackbox `efisblackbox` — 192.168.1.47

- Debian 13 aarch64, NVMe da circa 466 GiB.
- `bbxlog`, `efis-fuel-event-hub` ed `efis-can-seal.timer` attivi dopo l'installazione.
- Nessuna unità systemd fallita al controllo del 2026-08-31.
- Primo inventario reale completato senza arrestare `bbxlog`.
- Ciclo incrementale successivo completato e registrato con riepilogo compatto.
- Evidenza dell'ultimo controllo: 7.595 segmenti chiusi, 7.595 validi, zero rifiutati, 11.110.728.848 byte inventariati.
- Verifica manutentiva: quattro oggetti su quattro validi, cioè tre pacchetti esistenti più il manifesto CAN (`can_manifest=VALID`).
- Il manifesto reale contiene solo segmenti `VALID`; non sono stati rilevati segmenti troncati o rifiutati nell'archivio esistente.
- La registrazione CAN legacy resta in modalità listen-only e continua a produrre segmenti durante le prove.
- Accesso SSH dedicato V2 verificato il 2026-09-07 come utente `pi`; nessuna
  credenziale è stata scritta nel repository.
- Audit aggiornato: root filesystem NVMe con 420 GiB disponibili, nessuna unità
  systemd fallita e `bbxlog`, `bbxevent`, `bbxmqtt`, `bbxnav`, servizi edge e
  fuel hub attivi.
- `can_a` è a 500 kbit/s, `ERROR-ACTIVE`, in modalità kernel `LISTEN-ONLY` e
  trasporta frame standard `0x100`, `0x101`, `0x200`, `0x201`, `0x300`,
  `0x301`, `0x302`; il traffico osservato corrisponde alla famiglia di frame
  sensore/ESP32 documentata. `can_b` è a 500 kbit/s ma non ha mostrato traffico
  nel campione. Entrambe le interfacce avevano zero errori e zero bus-off.
- Il commit approvato `35dff11c3f969eb2efb81430accb02cbbea396c0` è stato
  compilato nativamente aarch64 in una directory separata con GCC 14.2,
  CMake 3.31.6 e Ninja 1.12.1; i 15 test non-Qt hanno superato la prova sul
  nodo reale.
- Il broker è installato in
  `/opt/efis-blackbox/bin/efis-blackbox-broker`; SHA-256
  `389125b7d4a99801faf0e1d14c32c0c6417b69cacd79860a9721f27d67f5a0cd`.
  Dopo il collaudo end-to-end l'unità systemd è stata abilitata al boot ed è
  attiva.
- Prova temporanea dell'unità come utente/gruppo `blackbox`: 204 frame CAN
  letti, 122 datagrammi DDP inviati senza errori e segmento recorder chiuso da
  101.835 byte, proprietà `blackbox:blackbox`. Prima e dopo la prova `can_a`
  riportava zero pacchetti TX; `bbxlog` è rimasto attivo. Il servizio è stato
  arrestato correttamente prima del collaudo end-to-end.
- Collaudo con il servizio PFD definitivo: 184 snapshot inviati nel campione
  Blackbox senza failure; il PFD è passato da `WAITING` a `LIVE`, ha accettato
  138 snapshot senza rifiuti, gap o errori socket. Dopo l'abilitazione il
  contatore PFD osservato è salito a 538 snapshot, ancora senza errori.
- Fault test di perdita publisher: dopo l'arresto controllato del broker il PFD
  è passato a `LOST` mantenendo 160 snapshot accettati e zero errori. La
  cadenza del log non ha fornito evidenza separata dello stato intermedio
  `STALE`, quindi quella transizione non è dichiarata verificata su hardware.

### PFD RPi5 `efis-pfd-01` — 192.168.1.45

- Il forwarder reale è attivo e ha ricevuto via API autenticata il riepilogo Blackbox.
- Cache end-to-end verificata con manifesto `VALID`, conteggi e segmenti CAN più recenti.
- Il commit `35dff11` è stato compilato in una directory release separata con
  Qt 6.8.2; build completa e 15 test su 15 superati sul nodo.
- Il vecchio servizio eseguiva un file già eliminato da `build/dev`: il processo
  vivo funzionava, ma dopo l'arresto non poteva essere riavviato. Il deployment
  usa ora il puntatore atomico `/home/efis/efis-next-current` verso la release
  verificata e un drop-in systemd versionato nel repository.
- `efis-pfd.service` è attivo e abilitato, ascolta in sola ricezione su UDP
  49002 e accetta esclusivamente il nodo logico Blackbox 47.
- Cattura reale 1024×600 acquisita dal compositor Wayland e conservata in
  `docs/status/evidence/pfd-ddp-2026-09-07.png`: renderer completo visibile con
  PFD, terreno ed EMS; in assenza DDP espone correttamente
  `SIMULATION ACTIVE`. Durante il link DDP il renderer ha mantenuto circa
  24 fps. Temperatura finale 52,7 °C e `get_throttled=0x0`.

### RPi5 storico `pi5` — WLAN 192.168.1.49

- Hardware reale identificato come Raspberry Pi 5 Model B Rev 1.1, Debian 13
  ARM64, 8 GiB RAM e root filesystem su microSD da 53 GiB con circa 34 GiB
  liberi. `can1` e' attivo. `eth0` e' ora collegata allo switch, usa
  `10.77.10.13/24` senza gateway/DNS e negozia 1000/full.
- Il software esistente `/home/pi/efis`, branch `sv3d-fondamenta`, resta
  invariato. `fuso-sv3d.service` continua ad avviare il renderer GLES diretto
  con CAN; `/home/pi/efis2` e i suoi circa 4,9 GiB di dati HGT/nav non sono
  stati modificati.
- Installata la toolchain Qt 6.8.2/CMake 3.31.6 necessaria. Dopo lo staging
  iniziale `ab0429e`, il commit `3458552` con radici terrain portabili e broker
  multi-display e' stato caricato nella release separata
  `/home/pi/efis-next-avionics-3458552` e compilato nativamente in Release:
  core, PFD, EMS, strumenti, broker e simulatori sono stati prodotti; 16/16
  CTest sono passati e `generate_status.py --check` ha restituito zero.
- Verifica hardware grafica: la selezione EGLFS automatica fallisce perche'
  sceglie `/dev/dri/card0` (V3D senza connettori). Con il nuovo profilo KMS
  esplicito `/dev/dri/card1` il PFD ha occupato correttamente il display reale
  per una prova limitata a 15 secondi. Terminata la prova, il servizio legacy e'
  tornato `active`; il rollback e' quindi verificato sul nodo.
- EEE e' disabilitato e il guard egress fail-closed e' attivo. Dopo il primo
  transitorio di attivazione, tre prove da 50 pacchetti verso Blackbox, PFD ed
  EMS hanno ottenuto 0% perdita, massimi rispettivamente 0,219/0,398/0,382 ms
  e zero errori RX. Non e' ancora una prova endurance.
- Il broker Blackbox reale e' stato aggiornato con backup e drop-in versionato:
  dichiara `displays=2` e pubblica a `10.77.10.11` e `10.77.10.13`, senza
  failure osservate. Nella prova MFD di 30 secondi DDP era `LIVE`, con 579
  snapshot accettati e zero reject, gap o errori socket.
- Le radici Quick3D non sono piu' codificate su `/home/efis`: il servizio usa
  `/home/pi/efis-data`. Dal PFD principale sono stati trasferiti 34 asset GPU,
  10 file active-textures e la cache da 1326 tile piu' report. Conteggi e hash
  dei file critici coincidono con la sorgente. Il runtime ha caricato il
  manifesto `OFFLINE_READY`, 1326 tile e 27 gap di copertura, senza errori di
  percorso osservati. Non e' stata acquisita una cattura hardware che dimostri
  la qualita' visiva del terrain.
- Aggiunti e installati una configurazione KMS versionata e un servizio systemd
  direct-DRM. Un primo test dell'unita' ha rilevato una corsa nel rilascio del
  DRM (`page flip: Permission denied`); l'ordine `After/Conflicts` e una pausa
  di un secondo hanno risolto il problema. Nel secondo test il servizio Next e'
  rimasto `active` per 12 secondi, il legacy `inactive`, senza errori DRM; al
  termine Next e' tornato `inactive` e il legacy `active`.
- `/home/pi/efis-next-current` punta alla release `3458552`, ma il servizio Next
  resta disabilitato e il renderer legacy resta abilitato al boot. Prima dello
  switch definitivo servono profiling fino al target e acceptance visiva.
- Dopo la correzione dichiarata e il reboot `get_throttled` era `0x0`, ma e'
  tornato a `0x50000` durante la compilazione. Nella prova display la CPU del
  processo era circa 102-105%, il clock ARM 2,4 GHz, il core 910 MHz, la
  temperatura massima osservata 54,9 °C e il renderer 13,8-14,6 FPS.
  Alimentatore/cavo non sono quindi ancora validati sotto carico e il frame
  rate non soddisfa il gate display; non abilitare Next al boot.

### Ground Station `Port-hp` — 192.168.1.39 / Pixhawk 6X

- Il Pixhawk 6X reale è presente come
  `/dev/serial/by-id/usb-ArduPilot_Pixhawk6X_3F001D000751333439393536-if00`;
  la porta era libera e QGroundControl non era in esecuzione durante il test.
- Prima della migrazione Ethernet e' stato acquisito il backup completo
  `arduplane-4.7.0-pre-ethernet-20260908-055450`: 1050 parametri dichiarati e
  ricevuti, nessun indice mancante, SHA-256
  `8893d6bddd638751aee9b515b505e9c9f547ff5b60d58e3f72b9690f3666751d`.
  Il firmware ha riportato 4.7.0.255 e UID2
  `3f001d000751333439393536000000000000`.
- Dal backup iniziale risultavano soltanto `NET_ENABLE=0` e
  `NET_P1_TYPE`...`NET_P4_TYPE=0`; come previsto, gli altri parametri sono
  nascosti finche' la rete non viene abilitata. In sessione manutentiva separata
  il solo `NET_ENABLE` e' stato portato da 0 a 1: dry-run, guardia sul valore
  precedente, eco, riletta indipendente e audit hanno tutti avuto esito
  verificato. Il bridge USB read-only e' stato riavviato ed e' tornato attivo.
- Dopo il power-cycle del 2026-09-08 e' stato acquisito il secondo backup
  completo `arduplane-4.7.0-net-enabled-20260908-175339`: 1067/1067 parametri,
  nessun indice mancante, SHA-256
  `59dbc2e033d80eada44d4bf163d088733df0790603dfc88349a5f9ab84166827`.
  Il firmware e l'UID2 coincidono con il backup precedente.
- Sul Pixhawk reale sono poi stati applicati e riletti singolarmente, con
  guardia e audit, soltanto i parametri di rete: `NET_DHCP=0`, indirizzo statico
  `10.77.10.3/24`, gateway `0.0.0.0` e `NET_P1_TYPE=1` (UDP client). Tutte le
  dieci scritture hanno esito `verified`; nessun parametro di volo e' stato
  modificato. Il bridge USB e' nuovamente attivo.
- Prima delle scritture lo strumento di manutenzione ha intercettato un
  `TypeError` intermittente interno a pymavlink durante una prova a vuoto;
  quindi non era avvenuta alcuna scrittura. La ricezione ora scarta soltanto la
  firma multi-istanza nota entro il timeout originario e lascia emergere ogni
  altro errore. Sintassi e banco software hanno superato 45/45 verifiche; la
  sequenza completa di dry-run e read-back e' stata poi verificata sul Pixhawk.
- Restano esplicitamente aperti: secondo riavvio per applicare l'indirizzo ed
  esporre `NET_P1_IP*`/porta/protocollo, configurazione unicast MAVLink2 verso
  `10.77.10.2:14550`, backup/diff finale e cattura su `eth0` che dimostri la
  provenienza `10.77.10.3`. Non disattivare ancora il bridge USB.
- Dopo il secondo power-cycle il Pixhawk reale ha risposto dalla Blackbox a
  `10.77.10.3`: 4/4 ping, 0% perdita, RTT medio 0,201 ms e neighbor Ethernet
  `c2:af:51:69:1d:d7`, coincidente con `NET_MACADDR0..5`. Il terzo backup
  `arduplane-4.7.0-p1-exposed-20260908-20260908-180416` e' completo 1073/1073,
  senza indici mancanti, SHA-256
  `37a94cc4ccbd62f9e8259943edbedd7d57de2c48ee425041c7256613ebad424c`;
  firmware e UID2 sono invariati.
- I sei parametri P1 appena esposti sono stati applicati sul Pixhawk reale con
  dry-run, guardia, audit e riletta indipendente: destinazione `10.77.10.2`,
  porta `14550`, protocollo `2` (MAVLink2). Tutti gli esiti sono `verified` e il
  bridge USB e' tornato attivo. Resta necessario il riavvio di applicazione,
  seguito da backup/diff e cattura filtrata su `eth0`; la sola risposta ICMP non
  dimostra ancora il flusso MAVLink Ethernet.
- Dopo il terzo power-cycle la cattura sulla Blackbox ha verificato il percorso
  operativo: 20/20 datagrammi da `10.77.10.3:62510` a
  `10.77.10.2:14550`, checksum UDP valido, zero drop kernel. Il filtro sul primo
  byte del payload ha trovato `0xfd` e l'hexdump contiene messaggi ArduPlane
  reali, quindi il protocollo sul filo e' MAVLink2 e non semplice traffico UDP.
- Con il bridge USB fermato, in 12 secondi sono stati catturati 296 datagrammi
  Ethernet. Nello stesso intervallo sono cresciuti i contatori MAVLink, recorder
  e DDP del broker, con zero errori/troncamenti/failure; sul PFD lo stato e'
  rimasto `LIVE`, `ddp_accepted` e' cresciuto, con zero rifiuti e zero errori
  socket. Questa e' evidenza HW end-to-end breve, non endurance.
- Il backup finale
  `arduplane-4.7.0-ethernet-final-20260908-20260908-181340` e' completo
  1084/1084, senza indici mancanti, SHA-256
  `960f36c4ad5359b6eea43284ac2b2a681c289c298b794e120baefc14aa05e641`.
  Rispetto al backup P1 precedente contiene i sei valori voluti, gli undici
  parametri `MAV5_*` esposti automaticamente dal canale MAVLink2 e normali
  valori adattivi/contatori variati al reboot; firmware e UID2 sono invariati.
- Il bridge USB non inoltra piu' alla Blackbox: resta attivo soltanto verso
  `127.0.0.1:14550` come endpoint locale di manutenzione. La cattura finale ha
  misurato 0 pacchetti da `192.168.1.39` a `wlan0:14550` e 139 pacchetti dal
  Pixhawk su `eth0` in sei secondi. Questo evita due copie indistinguibili dello
  stesso flusso; i file precedenti del servizio/env sono conservati sul target
  come rollback.
- La telemetria riporta correttamente pre-arm non superato per calibrazione
  accelerometro 3D e bussola. Non e' un difetto Ethernet e non va aggirato:
  calibrazione/installazione e verifica pre-arm restano gate hardware prima di
  qualunque uso operativo.
- Il fault fisico del cavo Pixhawk del 2026-09-08 ha prodotto 100% perdita ICMP,
  arresto completo del contatore MAVLink a 1.951.512 e nessun nuovo datagramma
  utile su `eth0`; broker, recorder e DDP hanno continuato senza errori e il PFD
  e' rimasto collegato alla Blackbox. Dopo la riconnessione: 4/4 ping, RTT medio
  0,197 ms, pacchetti MAVLink2 nuovamente catturati con checksum valido e
  contatore broker nuovamente crescente. Il bridge USB era destinato solo a
  localhost e non ha mascherato perdita o recupero.
- Il fault ha anche rivelato che `NodeHealthMonitor` registrava le due
  transizioni di peggioramento ma non restituiva quella di recupero: lo stato
  interno tornava `ONLINE` direttamente in `heartbeat()`. La correzione e il
  test di regressione hanno superato build MSVC e 16/16 test. La release esatta
  `e916548` ha poi superato build GCC 14.2 e 16/16 test nativi ARM in una
  directory separata sulla Blackbox.
- Il broker `e916548` e' installato e attivo con SHA-256
  `c621c272f2039123e7aee90232ae9ae3cc5488a11654873330ae15c79823c712`;
  il precedente e' conservato come
  `efis-blackbox-broker.pre-e916548-20260908`. Lo smoke test isolato ha prodotto
  21 snapshot/21 datagrammi e zero failure. Dopo lo switch, MAVLink, CAN,
  recorder, EDP e le due uscite DDP sono cresciuti senza errori; il PFD e'
  rimasto `LIVE` e `ddp_gaps` non e' aumentato. La generazione dell'evento di
  recovery non e' ancora verificata sul broker reale: richiede un secondo fault
  dopo questo deployment.
- Una prova temporanea di 12 secondi ha letto 100.036 byte reali, prodotto 588
  invii UDP verso due destinazioni, senza errori di lettura, invio o
  riconnessione.
- L'ispezione kernel del processo attivo ha mostrato la seriale su file
  descriptor 4 con flag ottali `02104000`; i bit di accesso sono zero e quindi
  confermano `O_RDONLY`.
- Il servizio utente `efis-mavlink-readonly-bridge.service` è installato,
  abilitato e attivo con la sorgente USB persistente e la sola destinazione
  locale `127.0.0.1:14550`. `systemd-analyze --user verify` e
  `py_compile` sono riusciti sul target.
- Sulla Blackbox reale, durante il servizio attivo, i contatori sono passati
  da 643 datagrammi / 209.052 byte a 863 / 282.505 in nove secondi, con zero
  errori socket e zero troncamenti. Il broker ha continuato nello stesso
  intervallo a registrare e pubblicare DDP senza failure.
- Sul PFD reale il consumer è tornato `LIVE`, con contatore accepted crescente,
  zero pacchetti rifiutati e zero errori socket. Il log corrente non espone la
  provenienza per singolo segnale.
- La rilettura in sola lettura dell'ultimo segmento recorder ha confermato
  `source=2` (`pixhawk_primary`) e `simulated=0` per rollio, beccheggio, prua,
  quota pressione e velocità verticale. Rollio, beccheggio e prua erano
  classificati `DEGRADED`; quota pressione e velocità verticale `VALID`.
- Nello stesso snapshot IAS risultava ancora selezionata da `source=3`
  (`esp32_legacy`) e valida. Latitudine, longitudine e ground speed erano
  attribuite all'ESP32 ma `UNAVAILABLE`. Il trasporto e una parte del mapping
  Pixhawk sono quindi verificati; non sono ancora chiusi airspeed Matek, GNSS
  M10, health completo e priorità finali.
- Durante una precedente interruzione di raggiungibilità WLAN il PFD è passato
  `LOST` e poi nuovamente `LIVE`; i gap cumulativi sono aumentati. È evidenza
  concreta che il banco Wi-Fi non soddisfa ancora il trasporto avionico finale.

### RPi4 EMS — WLAN 192.168.1.36 / FLIGHT-DATA 10.77.10.12

- Identità reale verificata: utente `pi`, hostname `efis`, Raspberry Pi 4
  Model B Rev 1.5, Debian 13 aarch64. Il precedente lease `192.168.1.48` è ora
  assegnato a un telefono e non identifica più l'EMS.
- `efis-ems.service` è abilitato e attivo con renderer `eglfs`. Il processo
  installato apre EDP V1 esclusivamente su `10.77.10.12:49003` e mantiene
  `EngineSimulationProvider` come fallback esplicitamente indicato. DDP V2
  resta limitato allo snapshot di volo.
- `eth0` usa `10.77.10.12/24`, 1000/full, EEE disabilitato e guard egress
  verificato. Il drop-in systemd confina il renderer a CPU1–3; dopo il restart
  del servizio l'affinità effettiva è `1-3` e la rete resta senza perdita.
- `can1` è `ERROR-ACTIVE` a 500 kbit/s e riceve frame, ma mostrava 293 drop RX
  cumulativi. Il servizio storico `fuso-can.service` funzionava nonostante
  `[init]` non valido; il file è stato corretto a `[Unit]`, validato e
  installato senza riavviare il bus. Provenienza e correttezza semantica dei
  dati CAN non sono verificate.

### Audit Ethernet dei nodi reali — 2026-09-07

- PFD, Blackbox ed EMS sono `UP/LOWER_UP` sul Waveshare verificato, negoziano
  1000/full e comunicano su `10.77.10.0/24`.
- La destinazione DDP Blackbox -> PFD è stata trasferita dalla WLAN a Ethernet;
  rotta, contatori interfaccia, log publisher e contatori consumer dimostrano
  il percorso reale. La WLAN resta volutamente attiva soltanto come recovery.
- Il PFD ascolta DDP su `0.0.0.0:49002`; la Blackbox ascolta MAVLink su
  `0.0.0.0:14550`. Sono bind da restringere con firewall/interfaccia durante il
  collaudo cablato.
- L'EMS è raggiungibile sulla WLAN di recovery `192.168.1.36` e sulla rete
  avionica `10.77.10.12`; è consumer EDP V1 dei dati motore normalizzati. Il
  feed attuale è ancora simulato sulla Blackbox e dichiarato `LIVE • SIM`.
- Ground Station e Pixhawk non sono stati collegati al dominio FLIGHT-DATA;
  nessun parametro Pixhawk è stato modificato.
- Il rischio di fallback sul gateway WLAN è mitigato da una regola nftables
  fail-closed persistente e verificata sui tre nodi. Il reboot completo dei
  nodi e una cattura Ethernet/WLAN correlata restano gate separati.

## Aperto — non dichiarare DONE

### Catalogo operativo aeromobile V2

- Acquisire i documenti controllati dell'esemplare: AFM/POH e supplementi,
  targhette, weight-and-balance, manuali motore/elica, configurazione elettrica,
  inventario avionica/sensori/servi e registri di calibrazione/manutenzione.
- Risolvere o motivare tutti i 447 campi; nessun dato safety-critical può
  diventare `VERIFIED` senza fonte, documento, revisione e pagina.
- Selezione e modifica controllata in bozza sono integrate nelle pagine Setup;
  restano da implementare confronto revisioni, revisione a quattro occhi,
  firma, release immutabile e rollback. Solo una release approvata potrà essere
  collegata al runtime, senza leggere direttamente bozze o righe non rilasciate.
- Verificare su hardware reale caricamento, rollback, separazione SIM/LIVE e
  comportamento fail-closed con profilo mancante, incompleto o corrotto.
- Installare la build su RPi5 e RPi4 e acquisire evidenza visiva della pagina a
  1024x600 e 800x480; ergonomia touch e leggibilità non sono ancora verificate
  su hardware reale.

### Emergency Assist e servi autopilota — concept 2026-09-09

- È documentata una V1 consultiva: procedure ordinate, voce, navigazione di
  emergenza, glide footprint conservativo e registrazione Blackbox. Non è
  implementata né verificata software/hardware.
- Il percorso critico sarà deterministico e basato su procedure POH/AFM
  versionate; un sistema AI generativo non potrà decidere comandi, velocità,
  assetto o modalità di volo.
- Un eventuale `Stabilize Assist` futuro potrà soltanto richiedere al Pixhawk
  una modalità prevalidata tramite comando fisico indipendente; PFD e Blackbox
  non comanderanno posizioni servo.
- Il registro hardware esterno al repository identifica AP-SERVO-01 come
  TruTrak DSB-B 8100-006, ma pinout definitivo, drive, coppia e prove al banco
  mancano. AP-SERVO-02 non ha ancora modello/P/N identificati. Entrambi sono
  quindi bloccati per l'attuazione.
- Emergency Glide automatico/Autoland resta fuori scope V1: rollio e
  beccheggio, da soli, non costituiscono un sistema capace di atterraggio
  automatico.
- Il punto di acquisizione POH/checklist è stato avviato con
  `docs/safety/EMERGENCY_PROCEDURE_SOURCE_AUDIT.md` e registry JSON. Le fonti
  pubbliche Asso/Pioneer sono state classificate come riferimenti non
  operativi; il manuale Asso pubblico contiene campi non compilati e non prova
  l'applicabilità all'esemplare. La libreria Rotax ufficiale e la famiglia
  `OM-912` sono identificate, ma mancano seriale motore e revisione applicabile.
- L'audit ha rilevato differenze importanti nel profilo simulato: geometria,
  flap, velocità e diversi limiti motore non hanno ancora una fonte controllata;
  il valore CHT massimo di 230 °C è anomalo. Nessun valore è stato promosso o
  cambiato senza evidenza dell'esemplare.
- La perdita di potenza motore è stata congelata come primo scenario. La
  specifica `docs/design/EMERGENCY_ASSIST_ENGINE_POWER_LOSS_V1.md` definisce
  attivazione, macchina a stati, footprint conservativo, ranking aeroporti,
  stati degradati, log e piano di test. È solo design: nessuna checklist
  operativa, soglia motore, best-glide o funzione di comando è implementata.

### RPi5 PFD — deployment completo 2026-09-07

- Nodo identificato su hardware reale: `efis-pfd-01`, Raspberry Pi 5 Model B
  Rev 1.1, ARM64, FLIGHT-DATA `10.77.10.11`, WLAN recovery `192.168.1.45`,
  Qt 6.8.2, 8 GiB RAM e 36 GiB liberi.
- Tutti i file versionati del commit
  `12b469729bfd8d5bb723efc9e086f2daa07c76eb` sono stati caricati nella release
  immutabile `/home/efis/efis-next-avionics-12b4697`. Build Release completa
  riuscita per core, PFD, EMS, broker, strumenti e simulatori; 16/16 test CTest
  passati e nessuna libreria PFD mancante.
- Lo switch atomico `efis-next-current` dalla release `efis-next-ddp-35dff11`
  alla nuova release è riuscito; la release precedente resta disponibile per
  rollback. Il servizio utente avvia il binario della nuova release.
- Verifica hardware runtime: DDP `LIVE`, zero reject, gap ed errori socket nel
  periodo osservato; catalogo 70 aeromobili/24 motori, navigation 65 aeroporti
  e pacchetto imagery `OFFLINE_READY` da 1326 tile caricati. Il frame rate si è
  stabilizzato a circa 23-24 FPS, equivalente alla release precedente;
  temperatura 56 °C e `get_throttled=0x0` durante la verifica.
- Non verificato: acceptance visiva mediante cattura, 60 minuti continuativi,
  reboot completo e fault fisico. `SIGUSR1` non è un'interfaccia di cattura del
  PFD e ha provocato un riavvio automatico del servizio; il processo è tornato
  attivo e DDP `LIVE`, ma questa non costituisce verifica grafica.

### EDP V1 Blackbox → EMS — implementazione 2026-09-07

- Implementato un protocollo motore separato e versionato, EDP V1/UDP 49003,
  con snapshot atomico di 18 campioni, provenienza completa, valore raw,
  revisione di calibrazione, marker simulated, identità, istanza, sequenza e
  CRC32C. DDP V2 non è stato esteso e resta riservato ai dati di volo.
- Implementati publisher Blackbox, consumer UDP EMS e stati visibili
  `WAITING/LIVE/STALE/LOST`; in perdita del collegamento l'EMS usa la
  simulazione soltanto come `SIM FALLBACK`, senza presentarla come dato reale.
- Verifica software: build core e broker ARM riuscita sulla Blackbox; test EDP
  di round-trip, UDP loopback, CRC, identità, duplicati e timeout riusciti. La
  build Qt ARM dell'EMS è riuscita sul kit Qt 6.8 del Raspberry Pi 4. Dopo la
  build completa sulla Blackbox sono passati tutti i 16 test CTest, inclusi
  ingest, safety, DDP, DroneCAN, engine ed EDP.
- Verifica su hardware reale: binari installati con backup su Blackbox ed EMS;
  publisher Blackbox attivo a 20 Hz verso `10.77.10.12:49003`; EMS in ascolto
  solo sull'indirizzo FLIGHT-DATA e transizione osservata `WAITING` →
  `LIVE • SIM`. Fermando per due secondi il broker sono state osservate
  `STALE • SIM` → `SIM FALLBACK • LINK LOST`; dopo il riavvio il nodo è tornato
  `LIVE • SIM`. DDP PFD, registrazione Blackbox e servizi sono rimasti/rientrati
  attivi. Questa è una prova di fault del processo, non di cavo fisico.
- Il publisher di banco è ancora una simulazione esplicitamente abilitata con
  `--engine-simulation`; decoder e arbitraggio CAN motore reali, reboot dei
  nodi, fault fisico ed endurance restano aperti. Sul nodo EMS l'ambiente EDP è
  per ora esportato dal launcher versionato perché l'installazione della nuova
  unità systemd richiede una successiva operazione amministrativa; il processo
  effettivo e il bind `10.77.10.12:49003` sono stati verificati.

- Broker e receiver PFD sono installati, attivi e verificati end-to-end sulla
  prima tratta Ethernet avionica cablata. L'egress fail-closed è verificato a
  runtime. EEE-off e l'affinità EMS hanno eliminato gap e burst nelle prove
  brevi; restano reboot, cattura correlata, scollegamento fisico, `STALE` ed
  endurance. Il fault logico e `LOST` sono verificati su hardware reale.
- Confermare con cablaggio e inventario finale la destinazione di `can_b` per
  Pixhawk/DroneCAN. `can_a` porta il flusso sensori corrente ed è stato usato in
  sola lettura; `can_b` era silente durante l'audit.
- Il percorso MAVLink reale Pixhawk 6X USB → Ground Station → UDP Blackbox →
  DDP PFD è operativo sul banco. Restano da verificare mapping e provenienza di
  ogni segnale, timeout/failover, scollegamento e riconnessione USB, assenza di
  variazioni parametri mediante confronto backup e la futura sostituzione del
  tratto USB/WLAN con Ethernet avionica cablata.
- Definire cadenza operativa e autenticazione/provisioning. UDP unicast è
  deciso per lo sviluppo, ma una LAN non isolata non soddisfa il gate prodotto.

1. RPi5 riallineato alla release immutabile 12b4697 e verificato; resta da riallineare in modo controllato la copia sorgente RPi4 senza perdere configurazioni locali.
2. Installare i binari risultanti, aprire la pagina Blackbox su entrambi i display e acquisire evidenza visiva leggibile del riepilogo e della lista CAN.
3. Aggiungere un test API automatico per manifesto valido, mancante, HMAC errato e JSON non valido.
4. Eseguire una prova reale di power-loss/reboot durante la scrittura `.bbx.part` e documentare il recupero; il comportamento è previsto dal formato ma non è stato provocato su hardware in questo intervento.
5. Definire ed eseguire una prova di restore/esportazione completa dei segmenti CAN. L'attuale `EXPORT_USB` esporta i pacchetti fuel/incident, non l'archivio CAN da 11+ GB.
6. Definire una politica di firma più forte per uso professionale: HMAC protegge l'integrità all'interno del sistema che custodisce la chiave, ma non equivale a una firma asimmetrica con verificabilità esterna.
7. Aggiungere TLS o una rete avionica isolata prima di considerare professionale il trasporto HTTP dell'hub; oggi autenticazione bearer e rete locale non costituiscono una certificazione avionica.
8. Collegare il parsing/normalizzazione dei messaggi CAN e MAVLink al modello dati arbitrato secondo ADR-003/ADR-005. L'inventario firmato prova l'esistenza e integrità dei segmenti, non la correttezza semantica di ogni misura.
9. Verificare carico CAN reale, latenze end-to-end, stasis/cross-check e comportamento degradato con Pixhawk 6X, M10 e Matek ASPD-DLVR collegati simultaneamente.
10. Le tratte Ethernet Blackbox-PFD-EMS, guard egress, EEE-off, affinità EMS ed
    EDP V1 simulato sono attive e verificate a runtime; mancano reboot, Ground
    Station, sorgente motore CAN reale, endurance e fault fisico.
11. Il servizio utente Ground Station è abilitato ma `Linger=no`: parte con la
    sessione utente. Decidere se il nodo deve funzionare prima del login e, in
    quel caso, abilitare il linger con autorizzazione amministrativa.
12. Il modello Waveshare è identificato come Gigabit-Switch-5P non gestito.
    Servono un secondo switch video e, per la configurazione finale completa,
    almeno sei porte operative sul dominio FLIGHT-DATA.

## Vincoli architetturali mantenuti

- Core C++20 indipendente da Qt e protocolli (ADR-001).
- Blackbox separata dai renderer e registrazione indipendente dal display (ADR-002).
- Acquisizione bus concentrata sulla Blackbox e percorso display read-only (ADR-003).
- Nessuna modifica alle protezioni stasis/cross-check o alle regole di arbitraggio (ADR-004/ADR-005).
- EMS resta nodo dedicato con componenti QML condivisi (ADR-006).
- Nessuna trasmissione introdotta sul CAN; il nodo resta listen-only (ADR-007).
