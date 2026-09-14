# EFIS NEXT — stato verificato

Aggiornamento: 2026-09-14

Branch di lavoro: `work/mavlink-observer-gateway-v1`

Base documentale letta prima delle modifiche: `CLAUDE_CODE_HANDOFF_2026-08-21.md`, `CHECKPOINT-2026-08-21.md`, `CHECKPOINT-2026-09-07-BLACKBOX-DDP.md` e ADR-001…ADR-015 pertinenti.

Checkpoint corrente:
`CHECKPOINT-2026-09-09-OPERATIONAL-PROFILE-WORKFLOW.md`. Il PFD operativo usa la
release immutabile `1d25e62`; la release precedente `23cd411` è conservata come
rollback immediato e le release `9c5c0f1`/`74a85f9` restano disponibili. Emergency
Assist è incluso nel binario installato, ma la funzione non ha ancora acceptance
dell'operatore sul pannello reale. Non è stata concessa autorità di comando al
sistema EFIS. Il secondo RPi5 MFD `pi5` usa invece la release immutabile
`f27508a`, avviata direttamente in EGLFS sulla pagina Canvas con profilo unita'
imperiali; il renderer storico è stato rimosso dal percorso operativo e
conservato in quarantena recuperabile.

Il workflow del profilo operativo è fissato separatamente in
`CHECKPOINT-2026-09-09-OPERATIONAL-PROFILE-WORKFLOW.md`.

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

- ADR-013 fissa l'allocazione standard: EMS su RPi4 dedicato; FPV/VIDEO-CORE su
  un secondo RPi4 fisicamente distinto; PFD, MFD/Canvas, navigazione, synthetic
  vision, setup e applicazioni generali su RPi5; Blackbox su RPi5. Pixhawk,
  ESP32-S3, camere e computer manutentivo restano classi esterne.
- Il PFD/MFD RPi5 può visualizzare lo stream FPV selezionato e una fascia EMS,
  ma non esegue il decoder video primario né sostituisce il nodo EMS. Questa
  distinzione mantiene una sola codebase senza condividere failure mode.
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
- Aggiunti runbook di migrazione reversibile e validatore offline del piano. Il
  validatore richiede piattaforma e host fisico, rifiuta assegnazioni RPi4/RPi5
  errate e impedisce la co-locazione EMS/FPV. Undici test su undici sono PASS.
  Il piano dichiara correttamente DDP/49002 verso PFD/MFD ed EDP/49003 verso
  EMS. I parametri `NET_*` del Pixhawk non vengono scritti da questo lavoro.
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

### Profilo completo di simulazione separato — 2026-09-09

- Aggiunto `simulation.asso-v-jolly-912-ul`, profilo separato dall'esemplare
  reale e generato deterministicamente da tutti i 447 campi del dizionario.
- I valori sono sintetici o analoghi e servono esclusivamente a simulatore,
  layout, allarmi, logging e replay. Ogni campo è `VALUE`, `INDICATIVE` e porta
  il marcatore persistente `simulated=true`; nessun numero è dichiarato
  applicabile all'aereo reale.
- Schema SQLite e indice espongono il marcatore per campo. Il validatore rifiuta
  qualsiasi valore simulato se un profilo viene posto in `VERIFIED` o
  `APPROVED`, anche quando tutti i campi risultano compilati.
- La pagina Setup condivisa PFD/EMS mostra conteggio `SIMULATED`, badge `SIM`
  sui campi e fascia rossa `SIMULATION DATA • NOT FOR FLIGHT`. Il profilo
  installato reale resta quello selezionato inizialmente e conserva i propri
  blocker senza ricevere valori dal profilo sintetico.

### Workflow guidato di caricamento, inserimento e controllo — 2026-09-09

- La pagina condivisa `OperationalProfileSetup` espone quattro fasi coerenti
  su PFD e EMS: `Overview`, `Guided Entry`, `Load / Prefill` e
  `Check & Review`.
- L'Overview mostra i tre profili e il completamento di tutti i 28 domini senza
  confondere il profilo installato con lo scenario completo SIM. Ogni sezione
  apre direttamente l'inserimento dei relativi campi.
- L'inserimento guidato conserva tipizzazione e provenienza già presenti e
  aggiunge l'avanzamento al primo problema ordinato per priorità:
  safety-critical non risolto, obbligatorio non risolto, simulato, evidenza
  documentale mancante.
- L'importatore accetta soltanto bozze V2 fino a 4 MiB con profilo e revisione
  corrispondenti. Rifiuta senza sostituire la bozza corrente JSON/schema errati,
  campi sconosciuti, tipi/unità/disposizioni non validi, metadati non ammessi e
  qualsiasi `activation_allowed=true`; rifiuta anche `INHERIT` su un profilo
  radice, che non avrebbe una sorgente effettiva.
- Il prefill completo richiede una pressione prolungata, copia i 447 valori
  analoghi esclusivamente nella bozza e mantiene ogni campo `simulated=true`.
  Il controllo restituisce quindi `BLOCKED_SIMULATION`, non una falsa
  completezza operativa.
- `Check & Review` calcola una coda deterministica, distingue incompletezza,
  simulazione ed evidenza mancante e può al massimo dichiarare
  `READY_FOR_INDEPENDENT_REVIEW`; `activation_allowed` resta sempre falso.

### Correzione accesso workflow operativo dal PFD — 2026-09-12

- La prova dell'operatore ha smentito l'interpretazione del checkpoint del
  2026-09-09: la pagina condivisa era compilata nel modulo PFD, ma
  `MaintenanceOverlay` non la istanziava. Sul PFD il riquadro `AIRCRAFT
  PROFILE` apriva quindi il catalogo di staging e non il workflow a quattro
  fasi.
- Il riquadro `AIRCRAFT PROFILE` apre ora `OperationalProfileSetup`; il
  catalogo modelli rimane disponibile dal riquadro separato `CATALOG`.
- Gli argomenti di anteprima del workflow sono ora gestiti anche da `Main.qml`,
  rendendo verificabili Overview, Guided Entry, Load/Prefill e Check & Review
  nel target PFD.
- Aggiunta una tastiera touch QML interna e condivisa da PFD ed EMS. Si apre sui
  campi del workflow, comprende lettere, numeri, simboli, maiuscole/minuscole,
  spazio, cancellazione e chiusura e non dipende da una tastiera virtuale del
  desktop.

### Telemetria grafica non ambigua — 2026-09-12

- Il precedente record `render_health` chiamava `fps` il numero di segnali
  `frameSwapped` e derivava da quello `frame_ms`. La seconda grandezza era
  soltanto l'intervallo medio fra presentazioni, non il tempo di rendering; la
  denominazione poteva quindi produrre una diagnosi falsa di saturazione GPU.
- PFD e nodo EMS registrano ora separatamente `presentation_hz`,
  `presentation_interval_ms` e `snapshot_hz`; il PFD aggiunge `ddp_hz`. Restano
  presenti sequenza, stato DDP e contatori cumulativi di accettazione, rifiuto,
  gap ed errori socket.
- Il percorso di cattura PFD legge `EFIS_CAPTURE_EXIT` prima della callback,
  registra l'intenzione nel log e usa un'uscita esplicita con codice 0 dopo un
  salvataggio riuscito oppure 2 dopo un salvataggio fallito.
- Verifica software Windows: target PFD ed EMS compilati; una cattura PFD
  offscreen ha prodotto il PNG ed è terminata con exit code 0. I test 1–16 della
  suite sono PASS. I test 17 e 18 non sono dichiarati PASS su Windows: il primo
  è andato in timeout e il secondo non ha caricato una DLL Qt nell'ambiente
  CTest locale. La release ARM e la cattura EGLFS devono quindi essere provate
  sui target prima della promozione.
- Verifica diagnostica su RPi5 `.49`, ancora con release `9c5c0f1`: PFD, NAV,
  EMS e Setup hanno presentato 13,7–15,0 frame/s, ma `QSG_RENDER_TIMING=1` ha
  misurato `syncAndRender` circa 10–14 ms. Il render thread rientra quindi nel
  budget di 16,7 ms in questa prova breve; il valore di presentazione non prova
  un collo di bottiglia GPU. Temperatura finale 53,8 °C e
  `get_throttled=0x0`. Questa non è una prova termica o di alimentazione
  prolungata.

### Composizione grafica a pagina attiva — 2026-09-12

- `AdaptiveShell` non mantiene più contemporaneamente istanziate PFD, Canvas,
  Mission, Navigation, EMS, FPV ed Emergency Assist. Un loader sincrono crea
  soltanto la pagina corrente; il selettore e le transizioni restano nel guscio
  comune. La qualità e le impostazioni del renderer di ogni pagina non sono
  state ridotte.
- Anche Maintenance/Setup e System Simulator vengono creati soltanto quando
  sono aperti. In precedenza i loro binding restavano attivi dietro alla pagina
  di volo, compreso il workflow operativo da 447 campi.
- Verifica software Windows: compilazione QML AOT riuscita; PFD, Canvas,
  Mission, Navigation, EMS, FPV, Emergency Assist e System Simulator sono stati
  caricati e catturati separatamente, sempre con exit code 0. È stata inoltre
  catturata la pagina Setup del profilo operativo. Le catture offscreen provano
  caricamento e geometria, non resa GL o prestazioni del Raspberry.
- Verifica hardware sul secondo RPi5 `.49`: release `1d25e62` compilata in
  313/313 step e 18/18 test PASS. Due sessioni EGLFS reali con cattura e uscita
  automatica hanno ottenuto exit code 0, zero errori QML, DDP senza gap,
  rifiuti o errori socket e `get_throttled=0x0`.
- Sullo stesso nodo e con lo stesso display, il PFD è passato da circa 13,6 a
  34,5 `snapshot_hz`; Navigation ha raggiunto 49,6 `snapshot_hz` e 50,6
  `presentation_hz`. L'ottimizzazione è quindi efficace sul Raspberry e non è
  ottenuta riducendo qualità grafica o dati.
- Questa misura su `.49` era stata eseguita con `/home/pi/efis-next-current`
  puntato a `1d25e62` e con il renderer storico ancora operativo. La successiva
  promozione MFD permanente e' descritta nella sezione hardware dedicata;
  cambio pagina touch, Setup touch, resa terrain e soak prolungato restano
  comunque aperti.
- Sul PFD principale `.45` la release `1d25e62` è stata trasferita con consenso
  esplicito, verificata tramite SHA-256, compilata nativamente in 313/313 step e
  ha superato 18/18 test ARM64. Lo switch atomico da `23cd411` è riuscito e la
  release precedente resta il rollback immediato.
- Nella verifica post-switch il servizio è rimasto `active`, DDP è `LIVE` a
  20,0–20,1 Hz con zero gap, rifiuti ed errori socket, `presentation_hz` è
  rimasto fra 34,0 e 38,0 e `snapshot_hz` fra 34,2 e 34,8. Il log contiene zero
  `TypeError`, `ReferenceError` o componenti non caricati; temperatura 54,3 °C
  e `get_throttled=0x0`. Cambio pagina, Setup e tastiera touch, resa terrain e
  soak continuativo di 60 minuti restano aperti e non sono dichiarati verificati.

### Emergency Assist consultivo Software V1 — 2026-09-09

- Aggiunto un core C++20 indipendente da Qt per otto famiglie di emergenza,
  macchina a stati, conferme pilota, gate di degradazione, modello di planata,
  ranking candidati, procedure versionate ed eventi deterministici.
- Il rilevatore perdita potenza richiede due segnali, persistenza e
  configurazione approvata; nel runtime installato resta inerte e può soltanto
  proporre `SUSPECTED`, mai aprire una checklist o comandare il volo.
- Best glide, planata e `REACHABLE` sono fail-closed. Un modello o input SIM
  produce soltanto `SIM_ESTIMATE`; senza AGL/vento/terrain/ostacoli/polare
  validi il PFD mostra `NOT AVAILABLE`.
- Una checklist richiede ID, revisione, fonte, hash, approvazione e passi. Il
  package reale non esiste; lo scenario software contiene soltanto passaggi di
  test esplicitamente SIM.
- Aggiunta pagina PFD FLY/NAVIGATE/COMMUNICATE/MANAGE, selezione delle otto
  famiglie, accesso touch prolungato, F12 di banco e comandi DONE/SKIP/REPEAT,
  recupero e monitoraggio.
- Gli eventi sono scritti nello spool locale JSONL V1. Non sono ancora
  consegnati alla Blackbox centrale e la UI lo dichiara `LOCAL SPOOL`.
- ADR-012 congela l'assenza di qualunque API verso Pixhawk, modi ArduPilot,
  servi, radio, transponder, flap, carrello o altri attuatori.

## Verificato solo software

### Emergency Assist Software V1

- Build MSVC 19.51 del core e del PFD Qt 6.11, inclusa compilazione QML AOT:
  PASS.
- `efis_emergency_assist_tests`: PASS. Copre attivazione/conferma, dati
  mancanti, best glide bloccata, planata SIM non autorevole, ranking ed
  esclusione candidati, package incompleto, step/skip e detector a due segnali
  con dwell; un detector non approvato resta inerte.
- PFD avviato offscreen con `--emergency-preview`, exit code 0 e cattura
  1024×600 prodotta. La geometria dei pannelli è renderizzata; il font viene
  sostituito dal backend Windows, quindi la cattura non vale come acceptance di
  leggibilità.
- La build completa di tutti i target Windows è riuscita. La suite CTest
  completa non è dichiarata PASS: Windows Application Control ha impedito
  l'avvio di cinque eseguibili e il launcher è rimasto in attesa sul test
  successivo fino al timeout. Il nuovo test Emergency è stato sia avviato da
  CTest sia eseguito direttamente con esito PASS.

### Catalogo operativo aeromobile V2

- `tests/catalog_tool_tests.py`: 12/12 test PASS con Python 3.12; coperti
  dizionario, unicità, materializzazione completa, eredità, rifiuto di chiavi
  sconosciute, obbligo della motivazione per N/A/non installato, build SQLite e
  conservazione atomica del catalogo precedente quando la rigenerazione fallisce;
  il registro Markdown contiene tutti e 447 gli identificativi.
- `PRAGMA integrity_check=ok`; 447 definizioni, 3 profili, 1.341 valori e zero
  coppie profilo/parametro mancanti. Validazione del seed: struttura PASS.
- Il profilo sintetico contiene 447/447 valori marcati simulati, zero campi
  irrisolti e zero blocker strutturali, ma resta correttamente
  `release_ready=false`. Un test dedicato dimostra che il tentativo di
  promozione a `VERIFIED` viene rifiutato per la presenza dei marcatori SIM.
- La struttura PASS non significa pronta al volo: il modello segnala 309
  blocker safety-critical e 429 campi `UNKNOWN`; l'esemplare aggiunge sei
  override `UNKNOWN`. Nessuna verifica su hardware reale è stata eseguita per
  questo catalogo.
- Build MSVC 19.51/Qt 6.11 completata per PFD ed EMS con compilazione AOT della
  nuova pagina QML. Nella revisione precedente il test
  `efis_operational_profile_model_tests` era passato verificando selezione
  profilo, isolamento della bozza, tipizzazione, motivazione obbligatoria,
  divieto di eredità sul profilo radice, scrittura atomica,
  `activation_allowed=false`, filtro e cancellazione della sola bozza.
- In questa sessione l'eseguibile aggiornato del test modello Qt è stato
  compilato e linkato, ma Windows Application Control ne ha impedito l'avvio
  (`BAD_COMMAND`): le nuove asserzioni UI/modello non sono quindi dichiarate
  eseguite. La compilazione AOT QML di PFD ed EMS è invece riuscita.
- Per il workflow guidato successivo lo stesso eseguibile Qt è stato compilato
  e avviato direttamente con esito PASS. Verifica 28 domini/447 campi, coda
  problemi, prefill completo con 447 marker SIM, stato `BLOCKED_SIMULATION`,
  importazione di una bozza corrispondente e rifiuto fail-closed di una bozza
  con `activation_allowed=true`, preservando la bozza valida precedente; copre
  inoltre il rigetto di `INHERIT` su un profilo radice.
- La build MSVC/Qt 6.11 di `efis-pfd-qt`, `efis-ems-qt` e del test è riuscita;
  la cache QML AOT della pagina condivisa è stata generata per entrambi i
  target. Il comando CTest è rimasto in attesa fino al timeout sul test, mentre
  l'esecuzione diretta dello stesso file è terminata in circa 13 secondi con
  exit code 0: l'anomalia del launcher resta distinta dall'esito del test.
- Le pagine `Load / Prefill` e `Check & Review` sono state aperte e catturate
  con backend Qt offscreen a 1024x600; struttura e geometria non mostrano
  sovrapposizioni evidenti. Il backend Windows non ha caricato il font e ha
  sostituito i glifi: la cattura non vale come verifica di leggibilità né come
  acceptance sul pannello reale.
- Smoke test EMS con backend Qt offscreen concluso con exit code 0. È una prova
  di caricamento software, non un'accettazione visiva sul pannello reale.
- La build completa Windows del branch è riuscita. Nel tentativo CTest completo
  14 test preesistenti sono passati; Windows Application Control ha bloccato
  prima dell'avvio `efis_safety_tests.exe` e il run non ha completato il test
  successivo entro il timeout. Il nuovo test Setup è stato quindi rieseguito
  separatamente con CTest ed è passato. I test non avviati non sono dichiarati
  superati da questa sessione.
- Il 2026-09-12 la build MSVC/Qt 6.11 di `efis-pfd-qt` e `efis-ems-qt`, inclusa
  la compilazione QML AOT di `MaintenanceOverlay`, `OperationalProfileSetup` e
  `EfisTouchKeyboard`, è terminata con successo.
- `efis_operational_profile_model_tests` è terminato con exit code 0 sia
  avviato direttamente sia tramite CTest: 1/1 test pertinente PASS. Un primo
  tentativo senza le DLL Qt nel `PATH` non aveva avviato il processo; ripetuto
  nell'ambiente toolchain completo ha superato tutte le asserzioni.
- Le catture offscreen 1024x600 mostrano la pagina Overview raggiunta dal PFD e
  la tastiera interna sovrapposta a Guided Entry senza uscire dal pannello. La
  sostituzione dei font del backend Windows resta presente; queste catture sono
  prova di caricamento/geometria, non acceptance visiva o touch.
- La release `9c5c0f1` è stata compilata nativamente sul Raspberry Pi 5 ARM64:
  313/313 step completati. La suite CTest del target ha superato 18/18 test,
  inclusi profilo operativo ed Emergency Assist; il controllo stato progetto è
  PASS, il binario PFD è eseguibile e `ldd` non riporta librerie mancanti.

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

Il profilo completo di simulazione, il nuovo marcatore campo per campo e la
relativa presentazione Setup non hanno ancora acceptance touch/visiva su
RPi4/RPi5 reale in questa sessione. Il deployment controllato PFD ha però
prodotto le evidenze negative e di rollback registrate qui sotto.

### Tentativo workflow operativo sul PFD — 2026-09-09

- Il commit `b29fc03` è stato trasferito in una release separata sul PFD `.45`,
  compilato nativamente ARM64 (302/302) e verificato con 17/17 CTest PASS;
  `generate_status.py --check`, temperatura e alimentazione erano regolari.
- Il primo switch non ha avviato il processo perché il percorso
  `logs/pfd.log` non poteva essere aperto: la directory vuota `logs/` non era
  inclusa da `git archive`. Il rollback automatico alla release `12b4697` ha
  ripristinato servizio, eseguibile e DDP LIVE.
- Creato `logs/`, il secondo avvio ha raggiunto DDP LIVE ma ha emesso due
  errori QML dell'editor: `FieldBox` esponeva l'alias `value`, mentre il codice
  assegnava `text`. È stato eseguito un secondo rollback a `12b4697`.
- La correzione usa ora l'alias `text`, conserva `logs/` tramite `.gitkeep` e
  comprime l'editor affinché pulsante successivo e nota di sicurezza rientrino
  nel pannello 1024x600. Build e smoke offscreen Windows sono PASS.
- Il commit correttivo `74a85f9` è stato estratto in una nuova release,
  compilato ARM64 con 302/302 step e 17/17 CTest PASS e attivato con switch
  atomico. Il processo corretto è attivo, imagery è `OFFLINE_READY` e DDP è
  rimasto `LIVE`; il log non contiene errori QML.
- Nel campione successivo di 45 secondi gli snapshot DDP accettati sono passati
  da 1.817 a 2.718 con zero reject, gap ed errori socket. Frame rate finale
  24,2 fps, temperatura 52,7 °C e `throttled=0x0`.
- Una cattura reale Wayland 1024x600 dimostra il renderer PFD della nuova
  release. Non mostra però le pagine Setup: ergonomia touch, tastiera,
  scorrimento e leggibilità del workflow restano esplicitamente aperti.
- Il controllo in sola lettura del 2026-09-12 conferma che il PFD esegue ancora
  `/home/efis/efis-next-avionics-74a85f9`, con `efis-pfd.service` attivo. Nella
  release installata `OperationalProfileSetup` è referenziato soltanto da
  `EmsSetupOverlay.qml`, non da `MaintenanceOverlay.qml`; non risultano inoltre
  installati pacchetti Qt Virtual Keyboard. L'operatore ha confermato sul
  pannello che il comportamento descritto non funziona. Questa è evidenza
  hardware negativa, non una verifica positiva della correzione.
- La correzione è stata quindi installata come release immutabile separata
  `/home/efis/efis-next-avionics-9c5c0f1` e attivata con switch atomico; la
  release `74a85f9` non è stata sovrascritta ed è il rollback disponibile.
- Il servizio esegue il binario della nuova release, è rimasto `active` con
  `NRestarts=0` e il log non contiene errori QML. Il runtime ha caricato profilo,
  catalogo, navigazione e imagery `OFFLINE_READY`; DDP è `LIVE` con contatore
  accepted crescente fino ad almeno 1.323 e zero reject, gap ed errori socket.
- Nel campione post-switch il frame rate è rimasto fra 22,2 e 24,7 fps;
  temperatura finale 51,0 °C e `throttled=0x0`. Queste sono evidenze hardware di
  deployment e continuità runtime, non acceptance touch del workflow.
- Restano da provare personalmente sul pannello: apertura di `AIRCRAFT PROFILE`,
  quattro pagine, tastiera interna, scorrimento, pressione prolungata del
  prefill, coda di controllo e ritorno al PFD. Nessuna di queste voci è ancora
  dichiarata verificata.

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
- Il 2026-09-12 è stata installata senza sovrascrivere le release precedenti la
  release immutabile `/home/efis/efis-next-avionics-1d25e62`. L'archivio ha
  SHA-256
  `9d3e40e40731ccc10ac61ef4ea6f060d25751c4a5eb804bb5ea052fc649a8f5e`;
  build ARM64 313/313 e 18/18 CTest sono PASS.
- Lo switch atomico da `23cd411` ha superato i gate automatici e non ha
  richiesto rollback. Due campioni runtime hanno mostrato servizio `active`,
  DDP `LIVE` 20,0–20,1 Hz, zero reject/gap/errori socket, zero errori QML,
  `presentation_hz` 34,0–38,0 e `snapshot_hz` 34,2–35,2. Temperatura massima
  osservata 54,9 °C e `get_throttled=0x0`.
- Questa prova verifica installazione, test nativi, avvio, continuità DDP e
  prestazioni brevi su hardware reale. Non verifica ancora interazione touch,
  qualità visiva di ogni pagina/terrain o soak di 60 minuti.
- Il primo reboot completo del 2026-09-12 ha conservato correttamente il
  puntatore a `1d25e62`, ma ha rivelato che `efis-pfd.service`, pur abilitato,
  non partiva: su questa sessione Debian/Wayland `graphical-session.target`
  restava inattivo. Il log mostrato subito dopo il reboot era precedente al
  boot e non è stato usato come evidenza positiva.
- L'unità versionata ora è installata sotto `default.target`, senza
  `PartOf=graphical-session.target`, e disabilita lo start limit affinché
  `Restart=always` possa coprire il breve intervallo prima della disponibilità
  di Wayland. La sintassi è stata verificata con `systemd-analyze --user
  verify`; la precedente unità è conservata sul nodo come backup.
- Un secondo reboot completo ha verificato l'avvio autonomo: boot alle
  14:37:49 CEST, servizio `active` dalle 14:38:00, release `1d25e62`, DDP
  `LIVE`, zero errori QML e `get_throttled=0x0`. È stato osservato un solo
  blocco grafico iniziale di circa cinque secondi (`presentation_hz` 4,8 e
  `ddp_hz` 7,2), seguito da recupero a 33–37 presentazioni/s; durante il blocco
  il contatore ha registrato 146 gap DDP. Il valore è rimasto poi stabile ma
  il transitorio resta un difetto aperto da profilare.
- Il soak fail-closed di 60 minuti, dalle 14:41:07 alle 15:41:09 CEST, ha
  terminato con `SOAK_RESULT=PASS`. Nei 61 campioni la release è rimasta
  `1d25e62`, il servizio è rimasto `active` con `NRestarts=1`, DDP sempre
  `LIVE` a 19,8–20,3 Hz, zero reject ed errori socket e nessun incremento oltre
  la baseline di 146 gap. Errori QML e throttling sono rimasti a zero.
- Durante il soak `presentation_hz` è rimasto fra 32,4 e 37,4,
  `snapshot_hz` fra 32,0 e 34,2 e la temperatura fra 53,2 e 56,0 °C. Il report
  completo resta sul nodo in
  `/home/efis/efis-next-soak-1d25e62-20260912-v2.log`. Questa è evidenza
  hardware positiva per il runtime continuativo successivo al transitorio;
  non chiude il difetto dei 146 gap all'avvio né l'acceptance touch/visiva.
- L'analisi del transitorio ha correlato il blocco del renderer ai timeout del
  portale desktop GTK durante l'avvio Wayland. Nello stesso intervallo il kernel
  ha registrato esattamente 146 `Udp: RcvbufErrors`; link Ethernet e driver non
  hanno mostrato errori. È stato quindi implementato un candidato di correzione
  a due livelli: il servizio kiosk seleziona esplicitamente il tema Qt `qt6ct`
  per non dipendere dal portale desktop e il receiver DDP richiede 4 MiB di
  `SO_RCVBUF`, espone la dimensione effettiva e la registra all'avvio.
- Verifica esclusivamente software del candidato: build MSVC/Qt completata e
  test UDP loopback superato anche con 128 snapshot accumulati prima del drain,
  senza gap di sequenza. Gli altri test non-Qt avviabili su Windows sono
  passati; tre eseguibili della suite completa sono stati bloccati o sono andati
  in timeout nell'ambiente Windows e non costituiscono un risultato positivo.
  Il commit `93bbe44` è stato poi compilato sul PFD in due build separate,
  entrambe complete 313/313; i 18 test ARM64 sono passati in entrambe.
- La release immutabile `/home/efis/efis-next-avionics-93bbe44` è stata attivata
  con rollback immediato a `1d25e62`. Lo switch a caldo ha mostrato buffer DDP
  effettivo di 8 MiB, DDP 19,9–21,0 Hz, zero gap e 33,8–36,0 presentazioni/s.
  Dopo reboot il contatore kernel è ripartito da zero e non ha registrato
  `RcvbufErrors`; anche il consumer DDP è rimasto a zero gap.
- La prova reboot non chiude però il difetto grafico: il servizio ha avviato un
  primo processo prima della disponibilità del socket Wayland; quel processo ha
  attivato il portal GTK senza display. I successivi timeout hanno ancora
  prodotto una finestra a 4,9 presentazioni/s, pur senza perdere DDP grazie al
  nuovo buffer. È stato aggiunto un gate systemd `ExecStartPre` sul socket
  `%t/wayland-0` per impedire a Qt di partire durante questa race.
- Il gate è stato installato e verificato con un secondo reboot: il primo
  `ExecStartPre` è stato respinto, Qt è partito soltanto quando il socket era
  disponibile e non ha più prodotto l'errore QPA iniziale. DDP e kernel sono
  rimasti a zero gap/`RcvbufErrors`. È rimasto però un singolo intervallo di
  event loop bloccato per circa 8,7 s, quindi il difetto grafico non è chiuso.
- Una traccia DBus con riavvio controllato dei portal e del solo PFD ha mostrato
  che il client interroga `org.freedesktop.portal.Screenshot` e che il portal
  raggiunge lo stato operativo in circa 0,22 s. Nei successivi 12 campioni da
  cinque secondi il renderer è rimasto tra 32,7 e 37,2 presentazioni/s, con DDP
  19,4–20,6 Hz e zero gap. La correlazione iniziale con il portal non è quindi
  sufficiente a spiegare lo stall residuo specifico del boot.
- Implementata telemetria diagnostica con timestamp millisecondi, watchdog
  dell'event loop e misura dei callback `pfd_update_snapshot` e
  `weather_simulation`. Il commit `cdf5645` è stato compilato integralmente sul
  PFD (313/313 target, 18/18 CTest ARM64) e osservato durante un reboot reale.
  La misura ha identificato due blocchi consecutivi del thread grafico:
  `weather_simulation` per 4126 ms e `pfd_update_snapshot` per 4480 ms, per un
  unico stall dell'event loop di 8650 ms. Nello stesso intervallo DDP è rimasto
  senza gap e il kernel senza `RcvbufErrors`, confermando che il difetto residuo
  è I/O sincrono nel renderer e non perdita Ethernet.
- Il commit `e902973` sposta i frame radar simulati nel runtime filesystem
  volatile e impedisce al fuel computer PFD di eseguire un
  `QSettings::sync()` incondizionato al primo callback e ogni dieci secondi
  quando i dati sono simulati. La persistenza periodica rimane attiva, con
  dirty flag, solo quando il consumo reale cambia; le modifiche esplicite del
  pilota e il flush all'uscita restano invariati.
- Verifica software: build MSVC/Qt e test DDP loopback passati; build ARM64
  completa 313/313 e 18/18 CTest passati sul PFD. La release immutabile
  `/home/efis/efis-next-avionics-e902973` è attiva e `cdf5645` resta
  disponibile per rollback.
- Verifica su hardware reale: switch a caldo stabile a 33,4–37,5
  presentazioni/s. Due reboot hanno mantenuto DDP `LIVE`, zero gap/reject/errori
  socket e `UdpRcvbufErrors=0`; temperatura 53,8–54,3 °C e
  `get_throttled=0x0`. Nel reboot pulito osservato per 112 s non sono comparsi
  `callback_stall`; il file fuel persistente è rimasto invariato e il frame
  radar è stato aggiornato in `/run/user/1000`. I precedenti blocchi mirati da
  4126/4480 ms sono quindi eliminati nella prova eseguita.
- Il gate PFD non è ancora chiuso: nel reboot pulito restano un transitorio di
  avvio da 958 ms e un singolo stall generico da 590 ms, non associati ai due
  callback corretti. Una prova precedente ha inoltre registrato 12607 ms in
  coincidenza esatta con una sessione SSH di misura, senza callback lento e
  senza perdita DDP; non è riprodotto nella finestra pulita e resta da
  distinguere fra interferenza dell'osservazione e scheduling di sistema.
  Mancano inoltre touch acceptance e verifica visiva delle pagine/terrain.

### RPi5 storico `pi5` — WLAN 192.168.1.49

- Hardware reale identificato come Raspberry Pi 5 Model B Rev 1.1, Debian 13
  ARM64, 8 GiB RAM e root filesystem su microSD da 53 GiB con circa 34 GiB
  liberi. `can1` e' attivo. `eth0` e' ora collegata allo switch, usa
  `10.77.10.13/24` senza gateway/DNS e negozia 1000/full.
- Implementato nel repository il profilo di deployment dedicato MFD
  `efis-next-mfd-direct.service`: accesso direct-DRM/EGLFS con KMS esplicito su
  `/dev/dri/card1`, pagina iniziale `CANVAS`, consumer DDP V2 su UDP 49002 e
  radici terrain/imagery persistenti sotto `/home/pi/efis-data`. Il profilo
  installato in `~/.config/EFIS-NEXT/ems-units.ini` seleziona `IMPERIAL`, quindi
  °F, PSI, IN HG, GAL e GPH secondo il modello di unita' comune del progetto.
- Verifica software sul target: la release immutabile
  `/home/pi/efis-next-avionics-f27508a` e' stata compilata nativamente con Qt
  6.8.2, CMake 3.31.6 e Ninja in 313/313 step; 18/18 CTest ARM64 sono passati,
  `systemd-analyze verify` non ha segnalato errori e `ldd` non ha rilevato
  librerie mancanti.
- Verifica hardware reale: switch atomico di `/home/pi/efis-next-current` a
  `f27508a`, avvio a caldo e due reboot completi riusciti. Dopo entrambi i reboot
  `efis-next-mfd-direct.service` era `active/enabled`, `lightdm` inattivo,
  `NRestarts=0`, `ExecMainStatus=0` e il runtime riportava `platform=eglfs`.
  DDP e' rimasto `LIVE` a circa 20 Hz: nella prima finestra sono stati accettati
  oltre 1300 pacchetti e nella prova successiva 1061, sempre con zero reject,
  gap ed errori socket. Il renderer ha misurato circa 34 presentazioni/s nella
  finestra stabilizzata; dopo il reboot finale il primo campione era 27,8/s.
- Il renderer precedente non e' piu' operativo: `fuso-sv3d.service` e
  `fuso-sv3d-sim.service` sono stati disabilitati, arrestati e rimossi da
  `/etc/systemd/system`; `/home/pi/efis` e `/home/pi/efis2` sono stati spostati
  in `/home/pi/legacy-disabled-20260912/`. La quarantena e' intenzionalmente
  recuperabile fino all'acceptance dell'operatore. `/home/pi/efis-data`, gli
  asset terrain GPU e il manifesto imagery sono stati preservati e caricati dal
  nuovo runtime (`OFFLINE_READY`, 1326 tile, 27 gap dichiarati).
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
- Le prove di staging precedenti avevano rilevato una corsa nel rilascio DRM
  (`page flip: Permission denied`), corretta con `After/Conflicts` e una pausa
  di un secondo; la promozione permanente `f27508a` non ha riprodotto l'errore.
  Le release Next precedenti restano disponibili come rollback software.
- Una prova temporanea EGLFS precedente aveva aperto realmente il workflow
  profilo a 1024x600;
  la cattura è conservata in
  `docs/status/evidence/mfd-operational-profile-9c5c0f1.png`. Profilo, catalogo,
  navigazione e imagery `OFFLINE_READY` sono stati caricati e DDP è risultato
  `LIVE`, fino a 392 snapshot accettati, zero reject/errori socket e 7 gap
  iniziali. L'interazione touch non è stata provata.
- Nelle due verifiche della promozione permanente `get_throttled=0x0`; le
  temperature finali erano 59,3 °C e 58,7 °C. Questo non sostituisce il soak a
  carico combinato: stabilita' alimentazione prolungata, cambio pagina e Setup
  touch, resa/qualita' terrain, corretta visualizzazione delle unita' imperiali
  e acceptance dell'operatore restano aperti. La release e' operativa sul banco,
  ma non e' dichiarata pronta al volo.
- Test hardware grafico del 2026-09-12: PFD, Canvas, NAV, Mission, EMS e Setup
  sono stati caricati uno alla volta sul display EGLFS reale e catturati a
  1024x600 con uscita controllata. Tutte le sei catture hanno `exit code 0` e i
  log non contengono `TypeError`, `ReferenceError`, moduli mancanti o fallimenti
  QML. Il servizio Canvas e' stato ripristinato dopo ogni gruppo di prove.
- La verifica visiva non chiude l'acceptance: PFD e Canvas mostrano il valore
  ALT e la cella VS troppo vicini, fino a sembrare un unico riquadro; la texture
  terrain nel primo piano e' stirata/sfocata e non raggiunge la qualita' target.
  NAV e Mission dichiarano `BLACK BOX OFFLINE` mentre il ricevitore DDP e'
  `LIVE`, perche' `blackBoxState` legge ancora uno status recorder locale che
  non esiste sul display. La pagina EMS del MFD dichiara `BLACKBOX LINK LOST`
  perche' EDP V1 e' destinato al solo RPi4 EMS. Questi stati non devono essere
  confusi con la ricezione DDP di volo, che durante le prove era regolare.
- Il Setup si apre e viene renderizzato, ma presenta valori statici errati per
  questo nodo (`RPi5 • PFD-01`, `PRIMARY DISPLAY`, `NODE: PFD-01`). Le stringhe
  risultano codificate direttamente in `MaintenanceOverlay.qml`; il profilo MFD
  non e' quindi ancora propagato nel modello Setup.
- Il profilo imperiale e' visibile nelle catture: KT/FT sui nastri e IN HG,
  GPH, °F, PSI e GAL nella strumentazione motore. Il caricamento del file unita'
  e' verificato; resta necessaria la conferma umana di leggibilita' sul pannello.
- Il test touch non e' eseguibile nella configurazione corrente: `lsusb` mostra
  soltanto i quattro root hub e `libinput` non elenca alcun dispositivo touch.
  Non e' stata simulata un'interazione per non trasformarla impropriamente in
  evidenza hardware.
- Soak preliminare completato per cinque minuti: 30/30 campioni con servizio
  `active`, zero restart, `ExecMainStatus=0`, temperatura 55,4–60,9 °C e
  `get_throttled=0x0`. Il campione finale riportava DDP `LIVE` a 20,0 Hz, 7970
  pacchetti accettati e zero reject, gap o errori socket; Ethernet aveva zero
  errori RX/TX e `UdpInErrors/UdpRcvbufErrors/UdpInCsumErrors=0`.
- Il soak di acceptance da 60 minuti e' stato avviato come unita' transiente
  `efis-next-mfd-soak-60.service` alle 22:29:50 CEST, invocation
  `f6ee9251ae1d47dbaf5a735628e160b0`. Produce
  `/home/pi/efis-next-test-evidence/soak-60min.csv` e il riepilogo omonimo; e'
  ancora in corso e non costituisce evidenza PASS finche' il riepilogo non e'
  stato letto e validato.

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
- La build è installata sul PFD RPi5; resta da installare sull'EMS RPi4 e da
  acquisire evidenza visiva delle pagine a 1024x600 e 800x480. Ergonomia touch e
  leggibilità non sono ancora accettate dall'operatore.
- Provare sui due pannelli il nuovo workflow a quattro fasi, inclusi pressione
  prolungata, tastiera per il percorso di importazione, scorrimento dei 28
  domini/coda problemi e tempi di risposta con una bozza da 447 campi.
- La correzione del 2026-09-12 è installata come release immutabile `9c5c0f1`
  senza sovrascrivere `74a85f9`. Verificare ora sul touch reale apertura dal
  riquadro `AIRCRAFT PROFILE`, tastiera, scorrimento, prefill prolungato,
  controllo e ritorno al PFD `LIVE`; queste prove di interazione restano aperte.

### Emergency Assist e servi autopilota — concept 2026-09-09

- La V1 consultiva core/UI è implementata, verificata software e inclusa nella
  release PFD `9c5c0f1`. Il corretto avvio generale del binario è verificato su
  hardware, ma la funzione Emergency Assist non ha acceptance dell'operatore.
  Dal 2026-09-13 esistono core software per condizionamento input e priorità
  audio e un trasporto eventi Blackbox con ack; adapter fisici, deployment e
  prove hardware restano aperti.
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
- La perdita di potenza motore è il primo scenario. Macchina a stati, detector
  configurabile, gate planata/ranking e log locale sono implementati; nessuna
  soglia reale, checklist operativa, best-glide dell'esemplare o funzione di
  comando è attiva.

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

### MFD release 3432402 ed Emergency Assist profili consultivi — 2026-09-12

#### Implementato

- Il Setup non contiene più l'identità statica del PFD: `EFIS_NODE_ID`,
  `EFIS_NODE_PLATFORM`, `EFIS_NODE_ROLE` ed `EFIS_SYSTEM_PROFILE` alimentano il
  modello QML. Il profilo systemd MFD imposta `MFD-01`, `RPi5`,
  `MULTIFUNCTION DISPLAY` e `PROFESSIONAL`.
- Sul display remoto `EFIS_BLACKBOX_STATUS_MODE=DDP_LINK` deriva
  `WAITING/LINK LIVE/STALE/LOST` dallo stato del consumer DDP. Non usa più il
  file di stato di un recorder locale e, soprattutto, non dichiara
  `RECORDING` quando ha verificato soltanto il collegamento dati.
- Il nastro ALT è stato allontanato dalla cella VS, la fascia VS è stata
  ristretta e separata da un divisore visivo. Questa modifica riguarda il MFD
  provato; il PFD principale `.45` non è stato aggiornato in questo intervento.
- Emergency Assist dispone ora di messaggi deterministici distinti per tutte
  le otto famiglie: FLY, NAVIGATE, bozza COMMUNICATE e cue vocale testuale. I
  testi sono non procedurali, richiedono verifica e procedura approvata e non
  contengono valori o sequenze di comando inventate.
- La pagina Emergency distingue `SIM TEST PROCEDURE`, procedura approvata reale
  e checklist bloccata; prima dell'acknowledgement FLY una procedura disponibile
  è descritta come tale senza mostrare un passo inesistente.
- Il confine ADR-012 è invariato: nessuna API o trasmissione verso Pixhawk,
  ArduPilot, servi, radio, transponder, flap o carrello è stata aggiunta.

#### Verificato solo software

- Commit funzionali: `7fe46a3` (identità MFD, stato link broker e separazione
  ALT/VS), `1d02b84` (otto profili Emergency e test) e `3432402` (chiarezza UI
  dopo ispezione hardware).
- Windows MSVC/Qt: build completa riuscita; 17/17 test eseguiti escludendo il
  noto blocco Windows di `efis_operational_profile_model_tests`, e test
  Emergency dedicato PASS. Il test escluso non è stato modificato.
- La release ARM64 isolata `1d02b84` ha compilato tutti i 313 step e superato
  18/18 CTest, incluso il test del catalogo operativo e il nuovo attraversamento
  delle otto famiglie Emergency.
- La release finale `3432402`, che differisce solo per due correzioni QML, ha
  compilato nativamente i target `efis-pfd-qt` ed
  `efis_emergency_assist_tests`; il test Emergency è PASS e `ldd` non segnala
  librerie mancanti. Non si attribuisce a `3432402` una nuova esecuzione
  dell'intera suite 18/18.

#### Verificato su hardware reale

- Prima delle modifiche, il MFD `.49` con release `f27508a` ha concluso il soak
  di 60 minuti: 60/60 campioni, temperatura 56,0–60,9 °C, zero riavvii, zero
  throttling, servizio attivo e DDP 19,9 Hz con zero reject, gap ed errori
  socket. È baseline hardware, non soak della release finale.
- `3432402` è attiva su `pi5`/`.49` mediante switch atomico; `1d02b84` è il
  rollback immediato e `f27508a` resta conservata. Il servizio è `active`, con
  `NRestarts=0`, `ExecMainStatus=0`, DDP `LIVE` 19,9–20,1 Hz, oltre 1.100
  pacchetti accettati e zero reject/gap/socket error nel controllo osservato.
  Temperatura 59,3 °C e `get_throttled=0x0`.
- Catture EGLFS reali 1024×600 hanno verificato: Setup con `RPi5 • MFD-01` e
  `MULTIFUNCTION DISPLAY` leggibili; PFD con ALT e VS visivamente distinti e
  stato `LINK LIVE`; scenario motore Emergency con banner SIM, profilo FLY/NAV,
  titolo `SIM TEST PROCEDURE` e dichiarazione esplicita di nessuna autorità di
  volo. Le catture automatiche verificano rendering, non interazione touch né
  acceptance dell'operatore.

#### Aperto / non verificato

- Il touchscreen non compare ancora in `lsusb`/libinput sul MFD: selezione
  pagine, pressione prolungata Emergency e workflow Setup non sono verificati
  fisicamente.
- `3432402` non ha ancora reboot completo né soak di 60 minuti. La prova lunga
  appartiene a `f27508a`; serve una nuova endurance con terrain e video al
  carico peggiore.
- Il terreno vicino resta stirato/sfocato. La correzione richiede lavoro
  separato su LOD, proiezione e campionamento texture; non è stata dichiarata
  risolta da una modifica ai nastri.
- La pagina EMS sul MFD non riceve EDP V1 perché il publisher è destinato al
  nodo EMS RPi4. Va definito un health riepilogativo o un secondo consumer senza
  confondere il link DDP di volo con il link motore.
- Emergency Assist è stato renderizzato su hardware soltanto nello scenario
  perdita potenza simulato. Le altre sette famiglie, input fisico reale, audio
  reale e nuovo sink Blackbox restano da verificare su hardware. Debounce/health,
  priorità audio, inoltro autenticato con ack e replay audit sono ora
  implementati e testati soltanto in software; il replay completo dei bus e i
  dati reali validati AGL/terrain/ostacoli/vento restano aperti.
- Le checklist operative restano bloccate: mancano POH/AFM controllato
  dell'esemplare, revisione/applicabilità OM-912 legata al seriale e
  documentazione dell'elica. Nessun testo consultivo introdotto in questo
  incremento sostituisce tali fonti.
- Stabilize Assist, Emergency Glide automatico e Autoland restano fuori dalla
  V1 e bloccati dai gate hardware/HIL. Nessun servo è stato comandato.

### Emergency Assist I/O, audit e Blackbox — incremento software 2026-09-13

#### Implementato

- Aggiunti `EmergencyInputMonitor` e contratto `EMERGENCY_IO_V1`: debounce,
  hold, riarmo dopo rilascio, health sorgente e diagnosi stuck sono
  deterministici e fail-closed. Il componente non contiene un adapter GPIO,
  evdev o CAN e produce soltanto un fronte di richiesta consultiva.
- Aggiunto `EmergencyAudioArbiter`: quattro priorità, FIFO a parità, preemption
  da priorità superiore, soppressione duplicati, repeat inhibit e mute limitato
  ai cue non critici. Non riproduce audio e non prende il posto degli avvisi
  primari certificabili.
- Lo spool PFD include ora UUID di sessione, tempo monotono, identità nodo,
  sequenze snapshot, stati DDP/motore, un contesto decisionale con validità e
  marker SIM e l'output Emergency mostrato al pilota.
- Aggiunti hub e forwarder Emergency separati dal dominio fuel. Il servizio
  sulla porta 18094 autentica per nodo, valida schema/limiti, deduplica il
  medesimo record e rifiuta con 409 il riuso di sessione/sequenza con payload
  diverso. SQLite usa WAL e `synchronous=FULL`; il forwarder avanza l'offset
  solo dopo verifica di digest, nodo, sessione, sequenza e `persisted=true`,
  salvando lo stato atomicamente con fsync.
- Il canale non espone endpoint di comando o maintenance. Health dichiara
  `command_api=false`; ogni endpoint di comando è assente. Nessuna API
  Pixhawk/MAVLink/servo/radio/transponder è stata introdotta.
- Aggiunto un validatore replay consultivo che rileva gap e regressioni di
  sequenza e riassume famiglie, sessioni, record SIM e copertura del contesto.
  `Main.qml` accetta `--emergency-preview=<FAMIGLIA>` per tutte le otto famiglie.
- Predisposte unità systemd con bind FLIGHT-DATA `10.77.10.2`, token separati e
  sandbox filesystem; non sono state installate sui nodi reali.

#### Verificato solo software

- Build Windows MSVC/Qt completa riuscita dopo l'aggiunta dei due core e del
  nuovo payload PFD. `efis_emergency_assist_tests` ed
  `efis_emergency_io_tests` sono PASS.
- `py_compile` è PASS per hub, forwarder, replay e test. Sette test end-to-end
  stdlib su loopback sono PASS: ack stabile/persistente, deduplica, avanzamento
  offset solo dopo ack verificato, nessun avanzamento su ack invalido, riapertura
  database, rifiuto token errato, conflitto sequenza, assenza API comando e
  rilevamento gap replay.
- Prima dell'ultima ricompilazione sono stati avviati con exit code 0 e
  catturati offscreen tutti gli otto argomenti preview. Questo verifica routing
  e creazione scena, non leggibilità: l'ambiente Windows offscreen non caricava
  i font e mostrava glifi sostitutivi.
- Un tentativo successivo della suite generale ha eseguito 12/18 test PASS;
  sei binari non sono stati avviati perché il criterio di controllo
  applicazioni Windows li ha bloccati (`BAD_COMMAND`). Non sono fallimenti dei
  test e non vengono conteggiati come verificati dal run Windows; la successiva
  suite ARM completa fornisce l'evidenza multipiattaforma mancante.

- Successivamente la release immutabile `60b3526` è stata compilata nativamente
  sul PFD ARM64: configurazione Qt 6.8.2/Ninja completata e build completa di
  319 step riuscita. La suite ARM ha eseguito 20/20 CTest PASS, inclusi
  Emergency Assist, Emergency I/O, i sette casi Python del trasporto e il test
  del catalogo operativo. Questa evidenza supera il limite di esecuzione
  Windows, ma resta verifica software eseguita sul target.

#### Verificato su hardware reale

- Il PFD `efis-pfd-01` è passato atomicamente da `e902973` a `60b3526`; la
  release precedente resta il rollback. Dopo lo switch il servizio è `active`,
  `NRestarts=0`, `ExecMainStatus=0`, DDP `LIVE` 19,9–20,8 Hz con zero reject,
  gap ed errori; rendering osservato 34–36 presentazioni/s.
- Hub Emergency installato sulla Blackbox `efisblackbox` e vincolato soltanto a
  `10.77.10.2:18094`; forwarder installato sul PFD. Sono stati generati token
  dedicati senza esporli nei log. La health letta dal PFD via `eth0` dichiara
  `command_api=false`.
- Le otto famiglie sono state avviate una per volta sul renderer Wayland reale
  e catturate a 1024×600, tutte con `saved=true`: engine, attitude, airspeed,
  electrical, fire/smoke, weather/terrain, sensors e medical. Il comando di
  banco non aveva working directory della release e ha quindi usato profilo e
  cataloghi fallback; la pagina era comunque esplicitamente SIM. Queste sono
  prove di rendering/route, non interazione touch né acceptance ergonomica. Le
  otto immagini, i relativi hash e il perimetro esatto della prova sono ora
  versionati in
  `docs/status/evidence/emergency-assist-rpi5-60b3526/`.
- Le otto sessioni hanno prodotto 24 transizioni JSONL (50.701 byte). Il
  forwarder ha ricevuto ack per l'ultimo record e avanzato allo stesso offset;
  la Blackbox riporta 24 record persistiti e nessun warning del servizio. Il
  PFD operativo è poi ripartito e ha ricaricato profilo, cataloghi e imagery
  dal corretto working directory.
- Dopo il primo controllo permessi, `UMask=0027` è stato applicato a hub e
  forwarder e verificato da systemd. Database, WAL e SHM esistenti sono stati
  portati a `blackbox:blackbox 0640`; la directory resta 0750. Dopo il riavvio
  l'health via `eth0` conserva 24 record, entrambi i servizi hanno zero restart,
  il PFD è `LIVE`, 54,9 °C e `get_throttled=0x0`.
- Fault di processo hub verificato con PFD operativo: dopo l'arresto Blackbox è
  stato accodato un evento SIM di banco. L'offset è rimasto esattamente 51011
  prima/durante il fault mentre il forwarder registrava `Connection refused`;
  al riavvio è avanzato a 51325, l'ack riportava la sessione attesa e la
  Blackbox è salita a 26 record. Non è stato perso né confermato prematuramente
  alcun record nella prova. Il primo tentativo aveva consegnato correttamente
  un evento, ma le letture offset erano invalide per quoting; non viene usato
  come evidenza del comportamento durante il fault.
- Il replay audit reale dei 26 record è `valid=true`, 10 sessioni, zero gap e
  zero regressioni. Ventiquattro record UI contengono il contesto decisionale;
  i due record artificiali di trasporto non lo contengono, quindi il rapporto
  segnala correttamente `complete_decision_context=false`. Tutti i record sono
  SIM e il replay dichiara `advisory_only=true`.
- Inventario I/O PFD eseguito senza modifiche: touchscreen QDtech MPI7003 su
  `/dev/input/event4`; due device ALSA HDMI e sink PipeWire HDMI disponibile a
  volume 0,40; `/dev/gpiochip0` e controller RP1 presenti; utente `efis` nei
  gruppi `audio`, `gpio` e `input`. Questo prova che esistono interfacce
  utilizzabili, non che il display abbia un altoparlante udibile né che esista
  un pulsante Emergency cablato. Nessun tono o GPIO è stato attivato.

#### Aperto / non dichiarare DONE

- Scegliere ingresso, schema elettrico, livello attivo, pull-up/down,
  ridondanza e adapter Linux; poi provare rimbalzo, stuck, cavo aperto/corto,
  reboot e interazione touch sul pannello reale.
- Implementare l'adapter audio e provare device-loss, preemption con stall/TAWS,
  cuffie/altoparlante, intelligibilità e livelli. Il solo arbiter non dimostra
  che un cue sia udibile.
- Token e servizi sono provisionati sul banco e il bind FLIGHT-DATA è attivo;
  arresto/ripristino del processo hub e retry dello spool sono verificati.
  Restano da provare cavo/rete fisici, duplicati da reboot, disco pieno,
  retention, TLS/isolamento e perdita di alimentazione prima di
  accettare l'ack come robustezza del registratore. L'hardening permessi
  `UMask=0027`/0640 è applicato, ma non sostituisce queste fault injection.
- Il replay è di audit JSON, non replay temporale di CAN/MAVLink/DDP/EDP né
  ricostruzione pixel-perfect. AGL, terreno, ostacoli e vento con provenienza
  valida restano gate distinti.
- POH/AFM dell'esemplare, applicabilità OM-912, elica e procedure approvate
  restano bloccanti. Nessun testo è stato promosso a checklist operativa.
- Stabilize Assist, Emergency Glide e Autoland restano fuori scope. Nessun
  comando è stato inviato ai servi o al Pixhawk.

### Emergency/Blackbox — hardening fault storage software 2026-09-13

#### Implementato

- L'hub Emergency tratta ogni errore SQLite come mancata persistenza: esegue
  rollback, non produce una ricevuta positiva e restituisce HTTP 507 con
  `persisted=false`. Il codice di errore è normalizzato e non espone percorsi o
  dettagli interni del filesystem.
- La health espone `storage_ok`, ultimo errore normalizzato, scritture rifiutate
  e ultimo commit riuscito. Dopo un errore resta HTTP 503 finché una nuova
  scrittura non raggiunge davvero il commit; una semplice lettura non cancella
  l'allarme.
- All'apertura vengono impostati esplicitamente busy timeout, WAL,
  `synchronous=FULL` e checkpoint automatico; `PRAGMA quick_check` blocca
  l'avvio su un database che non supera il controllo d'integrità.
- Aggiunto `tools/blackbox/test_emergency_storage_fault_hw.sh`: crea un ext4
  loopback temporaneo da 32 MiB, avvia un hub separato solo su
  `127.0.0.1:28094`, forza ENOSPC, libera spazio, ripete il record e verifica un
  riavvio dopo `SIGKILL`. Il percorso, la porta e il database di produzione non
  sono indirizzati. Il flag `--confirm-isolated-loopback` e la verifica del
  prefisso `/var/tmp/efis-emergency-storage-fault.*` impediscono un avvio o una
  pulizia a bersaglio ambiguo.

#### Verificato solo software

- `py_compile` è PASS per hub, forwarder, replay e test.
- La suite trasporto passa da 7 a 9 test PASS. Un limite SQLite deterministico
  forza `SQLITE_FULL`: l'hub restituisce 507, il forwarder non crea né avanza
  lo stato, la health passa a 503 e lo stesso record viene consegnato una sola
  volta dopo il ripristino della capacità.
- Un processo secondario viene terminato con una transazione WAL aperta e non
  confermata. Alla riapertura il record incompleto non esiste e `quick_check`
  restituisce `ok`. È una prova di crash di processo, non una prova elettrica.
- La sintassi Bash del nuovo harness è PASS su Git Bash. L'esecuzione richiede
  Linux, root per il solo loop mount e l'account di servizio `blackbox`; questa
  verifica sintattica non prova la sequenza hardware.

#### Verificato su hardware reale

- Il 2026-09-14 l'harness è stato eseguito sul nodo ARM64 `efisblackbox`
  (`192.168.1.47`) con filesystem ext4 loopback isolato e porta locale 28094.
  Il rapporto è `PASS`: 201 iniziale, 507 con `persisted=false` a filesystem
  pieno, health 503, recovery 201, due record presenti prima e dopo `SIGKILL` e
  `quick_check` superato alla riapertura.
- Il servizio production è rimasto `active` prima e dopo, con `NRestarts=0`;
  anche `efis-blackbox-broker.service` è rimasto active con zero restart. La
  porta 28094 e i mount temporanei risultano assenti dopo il cleanup.
- I primi due tentativi si sono fermati in sicurezza prima della fault
  injection e hanno corretto due difetti dell'harness: attraversamento della
  directory 0700/root da parte dell'utente `blackbox` e payload `detail` oltre
  il limite protocollo. Le correzioni sono incluse nello script verificato.
- Evidenza salvata in
  `docs/status/evidence/emergency-storage-fault-blackbox-20260914/`.
- Il test è hardware reale per filesystem pieno e crash di processo, ma usa un
  loopback isolato: `power_loss_verified` resta `false` e non prova ENOSPC,
  corruzione o I/O error sul filesystem production.

#### Aperto / non dichiarare DONE

- Restano da provare inode esauriti, I/O error, riavvio host e perdita
  alimentazione controllata. Sul percorso production vanno ancora correlati
  database/WAL, offset PFD, alert health e consegna successiva senza gap o
  duplicati; la prova loopback non autorizza una fault injection sul volume
  production.
- Verificare la health production dal PFD sul collegamento FLIGHT-DATA. Il nodo
  Blackbox non può interrogare localmente `10.77.10.2:18094` perché la policy
  firewall rifiuta il traffico verso la subnet 10.77.10.0/24 non uscente da
  `eth0`; il listener risulta comunque attivo sul bind previsto.
- Definire retention, quota riservata e allarme preventivo. Nessun record
  Emergency viene eliminato automaticamente da questo incremento.
- Touch, adapter pulsante, audio reale, replay temporale dei bus e fonti
  operative approvate restano gate separati. Il confine consultivo ADR-012 è
  invariato e non è stata aggiunta alcuna API di comando.

## Vincoli architetturali mantenuti

- Core C++20 indipendente da Qt e protocolli (ADR-001).
- Blackbox separata dai renderer e registrazione indipendente dal display (ADR-002).
- Acquisizione bus concentrata sulla Blackbox e percorso display read-only (ADR-003).
- Nessuna modifica alle protezioni stasis/cross-check o alle regole di arbitraggio (ADR-004/ADR-005).
- EMS resta nodo dedicato con componenti QML condivisi (ADR-006).
- Nessuna trasmissione introdotta sul CAN; il nodo resta listen-only (ADR-007).

### Emergency Assist — Flight Support grafico software 2026-09-14

#### Implementato

- Ridisegnata la pagina Emergency Assist 1024x600 con gerarchia operativa
  immediata: `FLY` a sinistra, supporto visivo centrale, `NAVIGATE` sotto la
  vista e `COMMUNICATE / MANAGE` a destra.
- Aggiunto `EmergencyFlightSupport.qml`: heading ribbon, scala di beccheggio,
  riferimento aeromobile, roll/pitch, IAS, ALT e VS rimangono visibili durante
  la consultazione della procedura.
- La superficie Terrain 3D/Synthetic Vision viene riusata solo quando posizione,
  quota, assetto e prua sono validi. Se un gate fallisce, il terreno scompare;
  l'orizzonte artificiale resta disponibile soltanto con assetto valido, senza
  inventare una scena, animare dati stale o dichiarare una raggiungibilità.
- Il controller espone gate distinti per attitude, heading, vertical speed e
  position; la UI mostra `INVALID` o trattini quando il relativo dato non è
  utilizzabile.
- Selettore delle otto famiglie, target e stato dati, massimo tre opzioni di
  navigazione, checklist singolo passo, bozza radio, cue vocale testuale e stato
  eventi/Blackbox sono stati ricomposti in pannelli touch più leggibili.
- Il banner consultivo permanente ribadisce che non esistono comandi verso
  Pixhawk, servi, radio o transponder. ADR-012 resta invariata.

#### Verificato solo software

- `qmllint` Qt 6.11.1 termina con exit code 0 e senza warning su
  `EmergencyFlightSupport.qml` ed `EmergencyAssistView.qml`.
- Build Windows Qt/MSVC completa: il target PFD compila 101/101 step e la build
  complessiva termina con 60/60 step; entrambi gli AOT QML e il link sono PASS.
- `efis_emergency_assist_tests`, `efis_emergency_io_tests` ed
  `efis_emergency_event_transport_tests` sono 3/3 PASS.
- Il PFD con `--emergency-preview=ENGINE_POWER_LOSS` ha prodotto una cattura
  automatica offscreen preliminare 1024x600 ed è uscito con codice 0 prima
  dell'ultimo affinamento dei gate. Il backend sostituisce i glifi dei font e,
  dopo la ricompilazione finale, il controllo applicazioni Windows ha bloccato
  l'eseguibile non firmato: la cattura non qualifica la versione finale.
- La suite completa è stata avviata ma non viene dichiarata PASS: sei eseguibili
  Windows non sono partiti perché lo stesso controllo applicazioni li ha
  bloccati e
  `efis_operational_profile_model_tests` è rimasto sospeso fino
  all'interruzione. I tre test Emergency pertinenti, rilanciati isolatamente,
  sono invece PASS.

#### Verificato su hardware reale

- Nessuna nuova verifica hardware in questo incremento. Non sono stati eseguiti
  deploy, cattura EGLFS/Wayland, touch acceptance o test di leggibilità sul
  pannello RPi5.
- Il checkpoint Blackbox fault-storage resta in attesa dell'esecuzione del
  relativo harness sul nodo reale; non viene promosso a completato.

#### Aperto / non dichiarare DONE

- Compilare e distribuire sul RPi5, acquisire catture reali di tutte le otto
  famiglie e verificare leggibilità, contrasto, dimensioni touch, transizioni e
  comportamento con attitude/heading/position/altitude/VS invalidi o stale.
- Validare AGL, terreno, ostacoli, vento, polar e aeroporti prima di mostrare
  qualunque opzione come realmente raggiungibile.
- Integrare e verificare pulsante fisico e audio con priorità stall/TAWS; oggi il
  cue vocale resta testuale.
- Approvare procedure POH/AFM applicabili all'esemplare. I profili SIM non sono
  procedure operative e non autorizzano una checklist reale.
- Diagnosticare separatamente gli eseguibili della suite Windows che non si
  avviano e il test operativo sospeso; questo non viene mascherato dall'esito
  positivo delle suite Emergency.

### Voice Command e Radio Phraseology Assist — incremento software 2026-09-14

#### Implementato

- ADR-014 separa il `Voice Command Gate` deterministico dal `Radio
  Phraseology Assist` e mantiene il confine consultivo di ADR-012: nessuna API
  verso Pixhawk, ArduPilot, servi, radio, transponder o attuatori.
- Aggiunto un core C++20 indipendente da Qt con grammatica chiusa italiana e
  inglese, abilitazione esplicita, PTT obbligatorio, health del recognizer,
  soglia di confidenza configurabile e rifiuto fail-closed delle frasi fuori
  grammatica.
- L'acknowledgement conserva separatamente voce/touch e verifica del sensore.
  `FATTO` o `CONFERMO` non possono quindi trasformare una condizione non
  misurata in una condizione verificata.
- Aggiunto un compositore fraseologico deterministico. Metadati incompleti,
  package reali non approvati, contesto mancante, campi read-back assenti o
  placeholder residui bloccano l'output. Anche un package completo mantiene
  `pilot_review_required=true` e `transmit_allowed=false`.
- Aggiunta la pagina `RadioAssistView` adattiva 1024x600 con stazione,
  frequenza, origine/validità, contesto, trascrizione, confidenza, read-back,
  selezione IT/EN e PTT simulato. Il menu espone la pagina `RADIO`; la preview
  usa esclusivamente dati sintetici con banner permanente SIM/no-transmit.
- Aggiunti configurazione development disabilitata per default, specifica del
  futuro evento di rete e documento di design con sorgenti normative
  EASA/ENAC/ENAV. Vosk e whisper.cpp sono soltanto candidati documentati: non
  sono stati aggiunti come dipendenze o presentati come adapter installati.

#### Verificato solo software

- Configurazione CMake con Qt renderer obbligatorio: PASS.
- Build Windows MSVC/Qt completa di tutti i target, incluso QML AOT della
  nuova pagina e link di PFD/EMS: PASS.
- `efis_voice_assist_tests`, `efis_emergency_assist_tests` ed
  `efis_emergency_io_tests`: 3/3 PASS tramite CTest. Il nuovo test verifica PTT,
  enablement, recognizer health, confidenza, grammatica esatta, assenza di
  autorità, separazione acknowledgement/sensore e composizione fraseologica
  fail-closed.
- `qmllint` della nuova pagina termina con exit code 0 e senza warning. Il
  controllo congiunto dei file shell preesistenti termina con exit code 0 ma
  riporta warning storici di accesso non qualificato; non vengono dichiarati
  corretti da questo incremento.
- La suite generale ha eseguito 18 test con esito PASS. Un eseguibile ESP32 non
  è stato avviato dal controllo applicazioni Windows, il test runtime-store è
  andato in timeout a 90 secondi e il test profilo operativo è stato interrotto
  dopo quel timeout. La suite completa non è pertanto dichiarata PASS.
- La preview offscreen ha prodotto un PNG 1024x600 ed è terminata con exit
  code 0. Il backend Windows non ha reso i font e la cattura non vale come
  acceptance grafica o di leggibilità.

#### Verificato su hardware reale

- Nessuna verifica hardware è stata eseguita per Voice/Radio Assist. Non sono
  stati collegati microfono, interfono, PTT, adapter audio o recognizer e non è
  stato eseguito alcun deployment sulla Blackbox o sui display RPi5.

#### Aperto / non dichiarare DONE

- Identificare interfono, connettori e punto di prelievo audio; progettare
  isolamento elettrico e PTT fisico prima di collegare la cabina.
- Integrare un recognizer offline prima in simulazione, misurando false accept
  e false reject su italiano, inglese, accenti, radio e rumore motore. La soglia
  software 0,82 è soltanto un default di sviluppo non validato.
- Definire protocollo autenticato Voice Core -> PFD/Blackbox, audit e
  anti-replay; applicare limiti di CPU/memoria per non interferire con broker e
  recorder.
- Costruire il database fraseologico versionato e verificarlo con una figura
  competente. Frequenze, pista, QNH, NOTAM e procedure devono provenire da
  dati correnti con provenance e freshness; nessun testo SIM è operativo.
- Implementare adapter ALSA/PipeWire e output audio, quindi verificare perdita
  device, priorità STALL/TAWS, latenza e intelligibilità. L'audio grezzo resta
  non persistente per default.
- Compilare e catturare la pagina su RPi5 reale, eseguire acceptance touch e
  confermare che la perdita Voice/Blackbox lasci disponibili PFD, touch e
  procedure cartacee.

### Voice Core — adapter Vosk replay SIM 2026-09-14

#### Implementato

- Aggiunto `tools/voice/vosk_replay_adapter.py`: accetta soltanto file WAV
  locali PCM16 mono 16 kHz e richiede contemporaneamente `--simulate` e
  `--ptt-active`. Non esistono percorsi microfono, rete, radio, CAN, Pixhawk o
  attuatori.
- Aggiunta la grammatica bilingue `vosk-grammar-v1.json`, marcata
  `DECODER_HINT_ONLY`. Il gate C++ continua a rivalidare la frase e resta
  l'unico componente che può accettare un intento dell'interfaccia.
- Il replay emette JSONL `efis-next.voice-recognition-candidate.v1` con
  sessione, sequenza, modello, lingua, hash grammatica, transcript e
  confidenza conservativa. Ogni record mantiene `simulated=true`,
  `candidate_only=true`, `accepted=false`, `intent=NONE` e
  `transmission_or_actuation_allowed=false`.
- In assenza delle confidenze parola-per-parola il punteggio viene posto a
  zero. Formati WAV errati, grammatica/schema non validi, confidenze non finite
  o fuori intervallo e risultati JSON malformati falliscono senza produrre un
  candidato accettato.
- Aggiunto il runbook Blackbox replay-only con dipendenze isolate, inventario
  del modello e gate di carico/fault prima di qualsiasi servizio reale.

#### Verificato solo software

- `py_compile` dell'adapter e della relativa suite: PASS.
- Sette test Python deterministici con Vosk fittizio: PASS. Coprono emissione
  senza autorità, minimo delle confidenze, punteggi invalidi, WAV stereo,
  grammatica per lingua, obbligo SIM/PTT e punteggio zero senza dettaglio
  parola-per-parola.
- Il test Python è registrato in CTest come
  `efis_voice_replay_adapter_tests`. Configurazione CMake PASS e verifica
  congiunta `efis_voice_assist_tests` + `efis_voice_replay_adapter_tests`:
  2/2 PASS.
- Build incrementale completa del preset Windows Qt/MSVC: PASS.

#### Verificato su hardware reale

- Nessuna nuova verifica hardware. Vosk e i modelli non sono stati installati
  sulla Blackbox; nessun file audio reale, microfono, PTT o interfono è stato
  usato e non è stato eseguito alcun deployment.

#### Aperto / non dichiarare DONE

- Verificare disponibilità e compatibilità del runtime Vosk e dei modelli
  IT/EN su Debian 13 ARM64, inventariare SHA-256 e licenza e usare soltanto un
  ambiente isolato.
- Costruire un corpus replay autorizzato con silenzio, frasi valide, fuori
  grammatica, accenti e rumore; misurare false accept/false reject e non
  promuovere il default 0,82 senza evidenza.
- Misurare CPU, RAM, temperatura e jitter insieme a broker, recorder,
  MAVLink, CAN, DDP/EDP e meteo; provare crash e modello mancante/corrotto.
- Collegare candidato, gate C++ e audit soltanto dopo aver definito protocollo
  autenticato/anti-replay. Acquisizione live, PTT fisico e audio cabina restano
  esplicitamente fuori da questo incremento.

### Gateway osservatori MAVLink senza bridge IP — 2026-09-14

#### Implementato

- ADR-015 mantiene la separazione di ADR-010: la Blackbox non diventa router,
  bridge L2 o NAT fra FLIGHT-DATA e LAN di servizio. È ammessa soltanto una
  copia applicativa MAVLink TX-only verso osservatori espliciti.
- `UdpReceiver` conserva ora indirizzo e porta della sorgente. Il broker può
  quindi accettare soltanto il Pixhawk configurato (`10.77.10.3`) prima di
  ingerire, registrare o replicare un datagramma; le altre sorgenti vengono
  scartate e contate.
- Aggiunto `ReadOnlyUdpFanout`, privo di receiver e di qualsiasi metodo di
  ritorno. Replica byte-per-byte a un massimo di otto endpoint unicast, rifiuta
  duplicati, wildcard e broadcast e isola errori/metriche per destinatario.
- La configurazione di banco predispone il portatile Debian
  `192.168.1.39:14550` e il server `192.168.1.6:14550`. Il bind broker viene
  ristretto da `0.0.0.0` a `10.77.10.2`; il flusso Pixhawk resta
  `10.77.10.3 -> 10.77.10.2:14550`.
- Aggiunti policy JSON e validatore fail-closed. Tutti i flag di forwarding,
  bridge, NAT, ritorno, comando, scrittura parametri/missione e armamento
  devono essere esplicitamente falsi.
- La protezione nftables contiene ora una chain `forward` con policy `drop`;
  una configurazione sysctl separata forza a zero forwarding IPv4 e IPv6.
  Il servizio broker, il drop-in multidisplay e il file ambiente includono i
  nuovi endpoint.

#### Verificato solo software

- Configurazione CMake e build completa Windows Qt/MSVC 313/313: PASS.
  Aggiunto `/EHsc` a tutti i target MSVC: le eccezioni standard usate per
  validazione fail-closed e isolamento dei destinatari hanno ora semantica di
  unwinding esplicita e i warning C4530 sono stati eliminati.
- `efis_core_tests`, `efis_mavlink_observer_policy_tests`,
  `efis_ingest_tests`, `efis_ddp_udp_tests` e
  `efis_mavlink_observer_fanout_tests`: 5/5 PASS nel rilancio congiunto.
- `efis_blackbox_broker_observer_tests`: PASS. La prova avvia il processo
  reale, scarta un datagramma da `127.0.0.2`, replica quello dalla sorgente
  ammessa, verifica bytes identici, contatore reject, modalità `TX_ONLY` e
  zero failure del fan-out.
- `efis_edp_v1_tests` era PASS nella prima esecuzione congiunta; dopo l'ultima
  ricompilazione Windows non ha avviato una volta l'eseguibile (`BAD_COMMAND`),
  quindi il test è stato rilanciato isolatamente ed è PASS. Il transitorio non
  viene nascosto né attribuito al protocollo EDP.
- Cinque test policy Python verificano configurazione repository, divieto di
  IP forwarding e comandi, esclusione degli osservatori da FLIGHT-DATA e
  rifiuto duplicati. Il validatore del piano Ethernet originale resta PASS
  con due segmenti, 11 nodi, quattro flussi e due switch.
- Il test loopback C++ verifica copia identica verso due destinatari, metadata
  sorgente, metriche aggregate/per-target e rifiuto di duplicati, broadcast e
  multicast.

#### Verificato su hardware reale

- Il 2026-09-14 la chiave dedicata è stata autorizzata e il preflight è stato
  eseguito sulla Blackbox ARM64 `efisblackbox` (`192.168.1.47`). Il preflight
  ha individuato il broker precedente in ascolto non conforme su
  `0.0.0.0:14550`; Pixhawk `10.77.10.3`, portatile `192.168.1.39` e server
  `192.168.1.6` risultavano raggiungibili prima dell'aggiornamento.
- Il commit `55b24da` è stato compilato nativamente con GCC 14.2/CMake 3.31.6
  sul nodo reale. La build ARM64 109/109 e l'intera suite disponibile 24/24
  sono PASS; il validatore policy restituisce `status=PASS`, due osservatori e
  `return_path=false`. Il warning GCC preesistente in `ddp_v2.cpp` resta da
  analizzare separatamente e non è stato nascosto.
- Il binario ARM64 con SHA-256
  `648c7c91d1bc429ae33a909027d25e5ee956e7475319dcf5759395e75b37f8e0`
  è installato con backup verificato in
  `/opt/efis-blackbox/rollback/pre-55b24da-20260914-recovery1/`; il binario
  precedente conservato ha SHA-256
  `c621c272f2039123e7aee90232ae9ae3cc5488a11654873330ae15c79823c712`.
- Il servizio reale è `active/running`, `NRestarts=0`, `ExecMainStatus=0` e
  ascolta esclusivamente `10.77.10.2:14550`. Il Pixhawk risponde 3/3 su
  Ethernet; i log mostrano datagrammi MAVLink e DDP/EDP in crescita, copie
  TX-only verso entrambi gli osservatori e zero failure/reject al campione.
- Sul nodo reale `net.ipv4.ip_forward=0` e
  `net.ipv6.conf.all.forwarding=0`; nftables ha chain `forward` con policy
  `drop`. Una cattura `eth0` ha osservato sei datagrammi da
  `10.77.10.3:62510` a `10.77.10.2:14550`, 26 intercettati dal filtro e zero
  drop kernel. Una cattura su `wlan0` ha osservato otto pacchetti alternati verso
  `192.168.1.39:14550` e `192.168.1.6:14550`, con 52 pacchetti intercettati
  dal filtro e zero drop kernel. La cattura prova l'uscita dalla Blackbox, non
  la ricezione o interpretazione nei due consumer.
- Durante una cattura separata su `eth0` è stato inviato dalla LAN a
  `192.168.1.47:14550` un singolo payload esplicitamente non-MAVLink. La
  cattura verso `10.77.10.3` è terminata con zero pacchetti e il broker è
  rimasto `active/running`, `NRestarts=0`, con DDP/EDP e fan-out in crescita.
  Questo verifica l'assenza di inoltro IP generico per il caso provato.
- Il vecchio bridge USB sul portatile è stato individuato come sorgente di una
  seconda copia: prima dell'arresto una sonda ha ricevuto 204 datagrammi in
  quattro secondi, 99 da `127.0.0.1` e 105 dalla Blackbox. Il servizio
  `efis-mavlink-readonly-bridge.service` è stato disabilitato e arrestato; la
  ripetizione ha ricevuto 105/105 datagrammi solo da `192.168.1.47`. Il
  Pixhawk è stato fisicamente scollegato dall'USB del portatile e non sono più
  presenti `/dev/ttyACM*` su quel nodo.
- Una sonda sul server ha ricevuto 105 datagrammi, tutti dalla Blackbox. È una
  verifica end-to-end temporanea: sul server non è ancora installato un
  consumer persistente in ascolto su UDP 14550.
- Un parser indipendente sul portatile ha ricevuto in cinque secondi 130
  datagrammi contenenti 130 frame MAVLink2, zero frame malformati, cinque
  heartbeat, 26 message ID e identità `system 1/component 1`. QGroundControl
  5.0.8 è poi stato avviato come unico listener su `0.0.0.0:14550`; il log
  conferma il link MAVLink v2 e la creazione del veicolo.
- La prova applicativa negativa è stata eseguita con QGroundControl attivo. Il
  log mostra richieste MAVLink reali `MAV_CMD_SET_MESSAGE_INTERVAL` e relativi
  timeout. Nello stesso intervallo `tcpdump` su `wlan0` ha catturato 35
  datagrammi QGC da `192.168.1.39:14550` verso la porta sorgente Blackbox
  `192.168.1.47:58363`, mentre la cattura simultanea su `eth0` verso
  `10.77.10.3` ha rilevato zero pacchetti e zero drop kernel. La Blackbox ha
  continuato a trasmettere verso entrambi gli osservatori; il broker è rimasto
  `NRestarts=0`, `ExecMainStatus=0`, con zero failure del fan-out. È quindi
  verificato sul banco che le richieste QGC non raggiungono il Pixhawk.
- La rimozione dell'USB dal portatile ha anche dimostrato che in banco il
  Pixhawk era alimentato da quel cavo: Ethernet e contatori si sono arrestati.
  Per ripristinare la prova il Pixhawk è stato alimentato temporaneamente via
  USB dalla Blackbox, lasciando il trasporto dati su Ethernet. La Blackbox lo
  rileva come `1209:5740 Generic Pixhawk6X` su `/dev/ttyACM0` e
  `/dev/ttyACM1`, ma `fuser` non rileva consumer seriali. Una verifica di 30
  secondi ha dato ping Ethernet continuo e crescita dei contatori osservatore
  da 28.892 a 29.707 per ciascun target; dopo l'ultima riconnessione sono stati
  confermati 4/4 ping, `vcgencmd get_throttled=0x0`, broker attivo e contatori
  oltre 39.000 per target senza failure. L'USB Blackbox è soltanto
  alimentazione provvisoria da banco, non l'architettura finale.
- Durante il primo tentativo di deployment una espansione PowerShell locale ha
  trasformato il percorso di backup e ha portato temporaneamente `/usr/bin` a
  modo `0750`, impedendo nuove shell SSH e il Wi-Fi dopo reboot. Non è stato
  nascosto: il nodo è stato avviato da microSD, `/usr/bin` è stato ripristinato
  a `0755`, le tre directory root vuote create dal tentativo sono state rimosse
  con `rmdir` e il post-check ha confermato root su `/dev/nvme0n1p2`, Wi-Fi
  `.47`, broker/firewall attivi e Pixhawk 3/3. Il deployment è stato poi
  ripetuto senza variabili remote, con staging, backup e attivazione separati.

#### Aperto / non dichiarare DONE

- Installare sul server il consumer persistente previsto, con bind e firewall
  minimi; la ricezione attuale è stata verificata soltanto con sonda temporanea.
- Alimentare definitivamente Pixhawk tramite modulo regolato `POWER1`, quindi
  rimuovere l'USB Blackbox e ripetere presenza Ethernet, heartbeat, contatori e
  brownout test. L'attuale USB è ammessa solo per il banco.
- Definire una modalità operativa QGroundControl esplicitamente
  `MONITOR-ONLY`: la telemetria e il veicolo sono visibili, ma richieste di
  stream, parametri, missioni e comandi scadono per progetto. Non presentare
  questa limitazione intenzionale come collegamento bidirezionale completo.
- Provare perdita di WLAN, portatile e server senza effetti su ingestion,
  recorder, DDP/EDP o controllo di volo.
- Eseguire soak con perdita, jitter, CPU, RAM e temperatura. UDP osservatori
  non è autenticato: i consumer devono trattarlo come telemetria non autorevole
  e filtrare la sorgente Blackbox.
- Progettare separatamente l'eventuale manutenzione bidirezionale con
  interlock fisico a terra, disarmato verificato, allowlist, audit, timeout ed
  esclusione mutua. Questa funzione non è implementata e QGroundControl può
  solo osservare, non configurare il Pixhawk attraverso la Blackbox.
