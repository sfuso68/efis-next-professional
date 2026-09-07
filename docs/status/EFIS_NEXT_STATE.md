# EFIS NEXT — stato verificato

Aggiornamento: 2026-09-07

Branch di lavoro: `work/ddp-multidisplay-v1`

Base documentale letta prima delle modifiche: `CLAUDE_CODE_HANDOFF_2026-08-21.md`, `CHECKPOINT-2026-08-21.md`, ADR-001…ADR-008. ADR-009 registra la decisione di questo branch.

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
- Aggiunti unità systemd di predisposizione e guida di deployment. Il servizio
  non è stato installato né attivato sulla Blackbox reale in questo branch.

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
  i processi senza Qt. La sua prima esecuzione resta da verificare nella PR.

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

## Verificato solo software

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
  dichiarato superato; la verifica indipendente Linux della PR è il gate
  corrente.

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

### PFD RPi5 `efis-pfd-01` — 192.168.1.45

- Il forwarder reale è attivo e ha ricevuto via API autenticata il riepilogo Blackbox.
- Cache end-to-end verificata con manifesto `VALID`, conteggi e segmenti CAN più recenti.
- Il renderer attualmente installato non è stato sostituito; la nuova presentazione grafica dell'inventario non è quindi verificata visivamente sul display.

### RPi4 EMS — 192.168.1.48

- I sorgenti della proprietà e della pagina sono stati predisposti nella copia di build.
- Non esiste in questo intervento evidenza sufficiente di build completa, installazione o verifica visiva su RPi4; queste attività restano aperte.

## Aperto — non dichiarare DONE

- Installazione Blackbox sospesa al checkpoint
  `CHECKPOINT-2026-09-07-BLACKBOX-DDP.md`: l'accesso SSH dedicato non è ancora
  autorizzato. Riprendere soltanto al messaggio operativo `fatto blackbox`.

- Installare `efis-blackbox-broker` sulla Blackbox e il receiver sul RPi5,
  configurare gli endpoint reali e verificare su Ethernet perdita,
  riordino, timeout, reboot, latenza e jitter.
- Confermare la mappa fisica `can_a`/`can_b` prima di abilitare SocketCAN nel
  servizio; la configurazione `can_a` è soltanto un valore iniziale da banco.
- Definire cadenza operativa e autenticazione/provisioning. UDP unicast è
  deciso per lo sviluppo, ma una LAN non isolata non soddisfa il gate prodotto.

1. Riallineare in modo controllato le copie sorgente RPi4/RPi5 al commit del branch e correggere il drift CMake già presente; compilare e linkare entrambi i renderer.
2. Installare i binari risultanti, aprire la pagina Blackbox su entrambi i display e acquisire evidenza visiva leggibile del riepilogo e della lista CAN.
3. Aggiungere un test API automatico per manifesto valido, mancante, HMAC errato e JSON non valido.
4. Eseguire una prova reale di power-loss/reboot durante la scrittura `.bbx.part` e documentare il recupero; il comportamento è previsto dal formato ma non è stato provocato su hardware in questo intervento.
5. Definire ed eseguire una prova di restore/esportazione completa dei segmenti CAN. L'attuale `EXPORT_USB` esporta i pacchetti fuel/incident, non l'archivio CAN da 11+ GB.
6. Definire una politica di firma più forte per uso professionale: HMAC protegge l'integrità all'interno del sistema che custodisce la chiave, ma non equivale a una firma asimmetrica con verificabilità esterna.
7. Aggiungere TLS o una rete avionica isolata prima di considerare professionale il trasporto HTTP dell'hub; oggi autenticazione bearer e rete locale non costituiscono una certificazione avionica.
8. Collegare il parsing/normalizzazione dei messaggi CAN e MAVLink al modello dati arbitrato secondo ADR-003/ADR-005. L'inventario firmato prova l'esistenza e integrità dei segmenti, non la correttezza semantica di ogni misura.
9. Verificare carico CAN reale, latenze end-to-end, stasis/cross-check e comportamento degradato con Pixhawk 6X, M10 e Matek ASPD-DLVR collegati simultaneamente.
10. Ethernet cablata della Blackbox risulta ancora senza carrier nell'ultimo audit; Wi-Fi è il collegamento attivo. L'architettura Ethernet avionica resta da cablare e collaudare.

## Vincoli architetturali mantenuti

- Core C++20 indipendente da Qt e protocolli (ADR-001).
- Blackbox separata dai renderer e registrazione indipendente dal display (ADR-002).
- Acquisizione bus concentrata sulla Blackbox e percorso display read-only (ADR-003).
- Nessuna modifica alle protezioni stasis/cross-check o alle regole di arbitraggio (ADR-004/ADR-005).
- EMS resta nodo dedicato con componenti QML condivisi (ADR-006).
- Nessuna trasmissione introdotta sul CAN; il nodo resta listen-only (ADR-007).
