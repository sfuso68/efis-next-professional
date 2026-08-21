# EFIS NEXT — Checkpoint operativo

Data: 21 agosto 2026
Riferimento: `CLAUDE_CODE_HANDOFF_2026-08-21.md`
Checkpoint precedente: `CHECKPOINT-2026-08-18.md`

Questo checkpoint consolida il primo ciclo di lavoro richiesto al paragrafo 21
dell'handoff. Non è un riepilogo del documento di consegna: riporta soltanto ciò
che è stato **verificato eseguendo comandi** il 21 agosto 2026, distinguendo la
misura dall'interpretazione.

Le sezioni da 1 a 7 registrano lo stato trovato all'inizio della giornata e le
verifiche fatte su di esso, prima di scrivere codice. Le sezioni 8 e 10
descrivono il backbone dati costruito nel corso della stessa giornata, e vengono
aggiornate man mano che avanza. Nessun parametro del Pixhawk è stato letto o
scritto in nessun momento.

---

## 1. Sintesi

Il progetto è stato messo sotto controllo di versione per la prima volta ed è
stata rimossa una condizione che nascondeva un fallimento di build. Sono stati
verificati in modo indipendente l'ambiente Raspberry Pi 5, l'allineamento fra le
copie del sorgente e l'intera catena build/test. Sono emersi tre problemi che
l'handoff non riportava, tutti diagnosticati; due risolti, uno che richiede un
intervento dell'utente.

Il backbone dati P1, che a inizio giornata non esisteva, è stato costruito e
collegato nel corso della stessa. Oggi i byte del Pixhawk entrano, vengono
interpretati, arbitrati, sorvegliati, normalizzati, registrati nella black box e
mostrati al banco prova sul display reale. Nessun pezzo della catena è più un
modulo corretto lasciato fuori dal percorso.

P4 è cominciato. L'adapter del nodo sensori ESP32 e il trasporto SocketCAN
esistono, e il trasporto è stato provato sul bus vero del nodo black box. La
seconda catena indipendente però **non è ancora innestata** nella catena di
ingestione: esiste e non arbitra contro il Pixhawk.

Stato complessivo: **il banco di sviluppo è coerente e riproducibile**, il
backbone dati è completo dall'ingresso dei byte fino alla pagina che li mostra, e
il secondo bus è letto ma non ancora unito al primo.

---

## 2. Versionamento introdotto

Fino a oggi `efis-next` non aveva alcun controllo di versione: nessuna storia,
nessun rollback, nessun modo di sapere se le copie su macchine diverse
divergessero. La Definition of Done del paragrafo 21 dell'handoff richiede
"rollback possibile", condizione tecnicamente non soddisfacibile.

È stato creato un repository git **locale**, senza alcun remote: nulla lascia la
macchina.

| Elemento | Valore |
|---|---|
| Baseline | `0f499c8` — stato del progetto al 21 agosto 2026 |
| Correzione build | `403552b` — rilevamento Qt esplicito |
| Ramo principale | `main` |
| Ramo di lavoro | `work/claude-p1`, nato come `work/p1-backbone-dati` |
| File versionati | 294 |
| Dimensione repository | 26 MB |

### 2.1 Cosa è escluso e perché

Esclusi tramite `.gitignore`: artefatti di build, dati binari scaricati o
rigenerabili (DEM Copernicus in `assets/terrain/`, pacchetti terreno, ingest
server, piani imagery) e ogni forma di segreto, token o credenziale, come impone
il paragrafo 19 dell'handoff. I dati binari restano tracciabili tramite manifest
di checksum e sono ricostruibili con gli strumenti in `tools/`.

Versionate invece le catture RPi5 e i concept grafici: costituiscono il
riferimento visivo per verificare le regressioni della UI.

### 2.2 Controllo credenziali eseguito prima del commit

L'albero è stato scansionato per credenziali in chiaro, sia per nome file sia per
contenuto. **Nessuna credenziale trovata.** Gli unici riferimenti ad
autenticazione costruiscono l'header a partire da variabili d'ambiente
(`edge_weather.py`, `sync_imagery_cache.py`, `fetch_copernicus_dem.py`,
`fetch_sentinel2_texture.py`) e `Caddyfile.efis-imagery` non contiene alcun hash
bcrypt. Il comportamento del codice esistente su questo punto è corretto.

### 2.3 Normalizzazione fine riga

Git era configurato per convertire i file di testo in CRLF al checkout. Applicato
a `deploy/scripts/run-pfd.sh` e alle unit systemd, che vengono eseguiti su Debian
aarch64, avrebbe prodotto file non funzionanti. Aggiunto `.gitattributes` che
forza LF su tutto il testo e marca come binari immagini e database.

### 2.4 Proprietà della cartella

La directory di lavoro appartiene all'utente `CodexSandboxOffline`, non a
`stefa`, e git la rifiutava come *dubious ownership*. Aggiunta l'eccezione
`safe.directory` per quel solo percorso.

---

## 3. Allineamento fra le copie del sorgente

Né la copia Windows né quella sul Raspberry Pi 5 erano versionate, quindi non
esisteva alcuna garanzia che fossero allineate. Sul RPi5 sono inoltre presenti
file `*.pre-layout-v2`, residuo di modifiche fatte direttamente lì il 20 agosto.

Confronto eseguito con checksum MD5 su tutti i file `.cpp`, `.hpp`, `.h`, `.qml`,
`.frag` e su `CMakeLists.txt`.

| Esito | Valore |
|---|---|
| File confrontati | 72 per lato |
| File con contenuto diverso | 0 |
| File presenti solo su Windows | 0 |
| File presenti solo su RPi5 | 0 |

**Le due copie sono identiche.** La baseline committata è quindi fedele e non ha
perso il lavoro svolto sul Raspberry Pi.

Nota metodologica: il primo confronto era risultato falsamente negativo perché
`md5sum` su Windows marca i file in modalità binaria con un asterisco, rendendo
non confrontabili gli elenchi. Il dato sopra è quello del confronto normalizzato.

---

## 4. Ambiente Raspberry Pi 5 — verifica indipendente

Tutte le caratteristiche dichiarate al paragrafo 3.2 dell'handoff sono state
verificate e **risultano esatte**.

| Elemento | Dichiarato | Misurato |
|---|---|---|
| Sistema | Debian 13 trixie | Debian GNU/Linux 13 (trixie) |
| Architettura | aarch64 | aarch64, kernel 6.18.39+rpt-rpi-2712 |
| Qt | 6.8.2 | 6.8.2 |
| CMake | 3.31.6 | 3.31.6 |
| Ninja | 1.12.1 | 1.12.1 |
| GCC | 14.2.0 | 14.2.0 (Debian 14.2.0-19) |
| Display | MPI7002/MPI7003 | HDMI-A-1, Mediatrix MPI7002 |
| Risoluzione | 1024 × 600 ~60 Hz | 1024x600, 60.044 Hz, preferred e current |
| Touchscreen | USB 0484:5750 | Bus 001 Device 002, ID 0484:5750 MPI7003 |
| Sessione | Wayland | wayland su tty |
| Servizio kiosk | `efis-pfd.service` | active |

Aggiunte: 7.9 GiB di RAM, 43 GB liberi sul filesystem root.

### 4.1 Alimentazione e temperatura

L'handoff segnalava `vcgencmd get_throttled = 0x50000`, cioè eventi storici di
undervoltage e throttling.

Misure del 21 agosto 2026:

- a riposo: `get_throttled = 0x0`, temperatura 55.4 °C;
- al termine di una build completa: `get_throttled = 0x0`, temperatura 57.6 °C.

Il valore `0x0` indica che **dall'ultimo avvio non si sono verificati eventi di
undervoltage o throttling**. Questo non chiude il punto: il registro si azzera al
riavvio e la prova termica prolungata richiesta dal paragrafo 8.4 non è stata
eseguita. Il gate resta aperto.

---

## 5. Build e test

### 5.1 Raspberry Pi 5 — ambiente di riferimento

Build completa in directory isolata `build/claude-verify`, con
`-DEFIS_REQUIRE_QT_RENDERER=ON`, senza toccare la build di lavoro né il binario
servito da `efis-pfd.service`, lasciato attivo per tutta la durata.

| Esito | Valore |
|---|---|
| Configurazione | riuscita, con `-DEFIS_REQUIRE_QT_RENDERER=ON` |
| Target compilati | tutti, `efis-pfd-qt` compreso |
| Diagnostica GCC | **zero** fra warning ed errori, con `-Wall -Wextra -Wpedantic -Wconversion` |
| Test | `ctest` 8 / 8 superati |
| Temperatura finale | 60.4 °C, nessun throttling |

Il backup `CMakeLists.txt.pre-claude-20260821` è stato creato prima della copia
del file modificato.

### 5.2 Windows — ambiente secondario

MSVC 19.51.36256.0 con Ninja: **8 / 8 test superati**. Dal pomeriggio del 21
agosto il target Qt è costruibile anche qui, perché Qt Quick 3D è stato
installato: vedi sezione 6.2.

### 5.3 Un difetto visibile su una piattaforma sola

Per un certo periodo `efis_core_tests` compilato in Debug terminava su Windows
con `STATUS_BREAKPOINT`, senza stampare nulla, mentre lo stesso codice passava
su Linux. La causa: la cartella di prova veniva rimossa mentre il flusso di
lettura sul segmento era ancora aperto. Su Linux cancellare un file aperto
riesce; su Windows no, e `remove_all` solleva un'eccezione che nessuno
catturava, quindi `terminate` e `abort`.

È stato isolato con un riproduttore autonomo della sola sezione interessata,
compilato contro la stessa libreria e con le diagnostiche della CRT dirottate su
stderr: senza quel dirottamento un'asserzione della CRT muore in silenzio, ed è
il motivo per cui il crash non diceva niente. Il codice ora chiude il flusso
prima di cancellare, e il commento nel test nomina la differenza fra le due
piattaforme perché non venga reintrodotta.

La lezione è generale e vale oltre questo caso: **verificare su una piattaforma
sola non basta**, e la piattaforma che passa non è necessariamente quella che ha
ragione.

---

## 6. Problemi trovati che l'handoff non riportava

### 6.1 Fallimento di build mascherato — RISOLTO

`find_package(Qt6 ... QUIET)` faceva degradare la configurazione a un semplice
messaggio di stato "Qt 6 not found". La configurazione risultava riuscita, ma il
target `efis-pfd-qt` non veniva costruito e `build/dev/` continuava a contenere
l'eseguibile del **18 agosto 04:34**, precedente all'introduzione di
`Terrain3DSurface.qml` e `TerrainTile3D.qml`. Per giorni ogni modifica alla UI su
Windows è stata invisibile, senza alcun segnale all'operatore.

È il fallimento mascherato vietato dal paragrafo 22 dell'handoff.

Correzione (commit `403552b`, `CMakeLists.txt`):

- quando il rilevamento fallisce, ogni componente Qt viene cercato singolarmente
  e il messaggio dichiara **quale** manca;
- il messaggio avverte che i binari già presenti in `build/` appartengono a una
  configurazione precedente;
- il livello passa da `STATUS` a `WARNING`;
- nuova opzione `EFIS_REQUIRE_QT_RENDERER`, di default `OFF`, che rende la
  condizione `FATAL_ERROR` dove il PFD deve obbligatoriamente compilare.

Il comportamento a Qt completo è invariato: verificato sul RPi5, 106/106 target.

### 6.2 Qt Quick 3D assente su Windows — RISOLTO

Diagnosi prodotta dalla correzione precedente:

```
componenti mancanti nell'installazione Qt: Qt6Quick3D
```

`C:/Qt/6.11.1/msvc2022_64` contiene solo `Qt6Quick3DSpatialAudio`; manca il
modulo Quick3D richiesto da `Terrain3DSurface.qml` e `TerrainTile3D.qml`.

Il modulo è stato installato nel pomeriggio del 21 agosto
(`qt.qt6.6111.addons.qtquick3d.win64_msvc2022_64`), quindi
`-DEFIS_REQUIRE_QT_RENDERER=ON` ora configura senza avvisi anche su Windows e
`efis-pfd-qt.exe` si costruisce in locale.

Resta però una cosa che Windows non può dare: la **cattura dal display reale a
1024 × 600**, che è l'unica prova valida per una modifica di interfaccia. Windows
è diventato il ciclo veloce; il Raspberry Pi 5 resta il luogo in cui una modifica
alla UI si considera verificata.

### 6.3 Esecuzione dei binari bloccata su Windows — VINCOLO PERMANENTE

Smart App Control è in enforcement
(`HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy\VerifiedAndReputablePolicyState = 1`).
`ctest` fallisce con `BAD_COMMAND` / "Process not started" e l'esecuzione diretta
con "Un criterio di controllo dell'applicazione ha bloccato il file".

Osservato ripetutamente nel corso della giornata, il comportamento è più preciso
di quanto sembrasse alla prima occorrenza: il blocco colpisce l'eseguibile creato
a un **percorso nuovo**, mentre una ricompilazione che sovrascrive un file già
esistente viene eseguita senza problemi. Il primo caso si è presentato subito
dopo aver cancellato `build/dev`; le build incrementali successive hanno eseguito
i test regolarmente.

Non è una regressione del codice e **non è stato modificato**: è un criterio di
sicurezza di sistema.

Conseguenze operative:

- non cancellare la directory di build su Windows, perché la ricostruzione da
  zero produce binari che il criterio blocca;
- **la validazione di riferimento di build e test va comunque eseguita sul
  Raspberry Pi 5**, che è il target reale, ha il compilatore più severo e non ha
  questo vincolo. È raggiungibile via SSH a chiave senza password.

---

## 7. Rete e nodi

Tutti e quattro i nodi del paragrafo 3.1 dell'handoff risultano raggiungibili.

| Nodo | Indirizzo | Stato |
|---|---|---|
| PFD Raspberry Pi 5 | 192.168.1.45 | raggiungibile; SSH a chiave verificato |
| Server mappe `serverha` | 192.168.1.6 | raggiungibile; accesso non verificato |
| Black box | 192.168.1.47 | raggiungibile; accesso non verificato |
| Ground station | 192.168.1.39 | raggiungibile; accesso non verificato |

Gli indirizzi restano DHCP. Prenotazioni, DNS locale e discovery controllato
restano da introdurre.

L'audit in sola lettura dei nodi diversi dal PFD, richiesto al paragrafo 21.4
dell'handoff, **non è stato eseguito**.

---

## 8. Backbone dati P1 — completato

All'inizio della giornata il backbone dati era lavoro interamente nuovo: la
ricerca di `mavlink`, `socketcan`, `heartbeat` ed `esp32` in `src/`, `include/` e
`tests/` non trovava alcuna implementazione. Il core era una pipeline di sola
simulazione.

La catena oggi è questa, e ogni freccia è verificata da un test:

```
byte -> MavlinkAdapter -> SnapshotArbitrator -> FlightModel -> stato protetto
                                                     |
                                    +----------------+----------------+
                                    |                                 |
                        SegmentedRecorder (black box)        SensorBench (Setup Center)
```

### 8.1 Elenco del paragrafo 20 dell'handoff

| # | Voce | Stato | Dove |
|---|---|---|---|
| 1 | Formalizzare Display/Sensor Data Protocol V2 | parziale | Envelope e regole restano un draft. I confini di trasporto e le direzioni di dipendenza sono però decisi in `ADR-003`; manca la scelta della serializzazione sul filo |
| 2 | Adapter MAVLink in sola lettura | fatto | Nessun percorso di trasmissione, in nessun punto della catena |
| 3 | Snapshot normalizzato verso il core | fatto | `MavlinkPipeline::state()` |
| 4 | Heartbeat, stale e perdita nodo | fatto | `NodeHealthMonitor`, con prova di mutazione |
| 5 | Black box con segmenti e replay | fatto | `RecordingSession`. Il registro V2 si scrive e si rilegge, eventi compresi, e `efis-recorder --replay` registra dall'adapter invece che dal simulatore |
| 6 | Dati monitor-only al banco prova UI | fatto | `SensorBench` più il pannello nel Setup Center, verificato con cattura dal display reale |

### 8.2 Elenco del paragrafo 6 dell'handoff, lavoro core EMS

| # | Voce | Stato | Nota |
|---|---|---|---|
| 1 | Schema unico del campione sensore | fatto | 14 campi |
| 2 | Heartbeat e stato dei nodi | fatto | |
| 3 | Timeout, dato stale, perdita nodo | fatto | Un battito scartato non rinvia il timeout |
| 4 | Adapter ESP32-S3 seriale o CAN | da fare | È il prossimo passo, e apre P4 |
| 5 | Adapter SocketCAN su Raspberry | da fare | Le interfacce `can_a`, `can_b`, `vcan0` esistono già sul nodo black box |
| 6 | Provider reali al banco prova | fatto | Il banco è alimentato dalla registrazione reale del Pixhawk, non dal simulatore |
| 7 | Priorità e failover primario-secondario | fatto | `SnapshotArbitrator`, arbitrato **per grandezza**: la posizione può venire dal Pixhawk mentre l'assetto resta simulato, che è lo scenario di guasto parziale |
| 8 | Registrare telemetria, guasti e cambi sorgente | fatto | Eventi nodo e cambi sorgente finiscono nel registro e si rileggono |

### 8.3 Le protezioni sono ora sul percorso

Nella versione precedente di questo checkpoint la voce più importante era che
tre controlli esistevano, erano testati e non erano collegati a niente. Non è
più così, e la differenza è verificabile leggendo gli include:

| Controllo | Dove agisce | Che cosa intercetta |
|---|---|---|
| `SnapshotArbitrator` | `MavlinkPipeline` | La sorgente caduta, per singola grandezza |
| `StasisMonitor` | `FlightModel` | Il valore fermo: un nodo che trasmette regolarmente portando un numero che non cambia più perché il sensore dietro è morto |
| `CrossCheck` | `FlightModel` | Il sensore che sbaglia in modo coerente, contraddetto da una catena indipendente |

Le prove non verificano che i tre moduli funzionino, cosa che fanno già i loro
test: verificano che siano **raggiunti partendo dai byte**. La differenza è
sostanziale, e per un certo tempo la seconda non era vera.

### 8.4 Due difetti trovati collegando, non scrivendo

Meritano di restare scritti perché nessuno dei due si sarebbe visto senza far
passare dati veri lungo la catena intera.

**Il marcatore di simulazione si perdeva invecchiando.** `Validity` ammette un
solo stato per volta, quindi un campione simulato che scadeva diventava `stale` e
diventava indistinguibile da un dato reale scaduto. Il marcatore vive ora in
`SampleOrigin::simulated` e sopravvive a ogni transizione.

**L'arbitro cancellava la provenienza.** Quando nessuna sorgente era utilizzabile
e non ne era mai stata scelta una, restituiva un campione vuoto. Sulla
registrazione reale il sensore differenziale è dichiarato non sano, come è giusto
con `ARSPD_USE` a zero: l'adapter traduceva correttamente la velocità con
segnale, unità e valore, e dopo l'arbitrato diventava segnale sconosciuto, nessuna
unità, valore zero. Il registro perdeva l'identità della grandezza proprio nel
caso in cui serve saperla, e non è un caso limite: è lo stato normale di questo
banco.

### 8.5 Modello dati: colmato

`Sample<T>` portava 7 campi contro i 14 richiesti dal paragrafo 17.2. Ora li
porta tutti: identificativo del segnale, nodo, canale, valore grezzo, unità,
marcatore di simulazione, revisione di calibrazione e tempo UTC, quest'ultimo con
un valore esplicito per "non affidabile".

### 8.6 Decisioni architetturali registrate

I due numeri liberi nella numerazione delle ADR sono stati usati:

- `ADR-003` fissa che l'ingestion dei bus avviene su un solo nodo, la black box, e
  che i display ricevono stato normalizzato e mai frame grezzi. Dichiara le
  direzioni di dipendenza ammesse, verificabili leggendo gli include. È la ragione
  per cui l'arbitrato non sta dentro l'adapter.
- `ADR-004` motiva la sorveglianza del valore fermo e il confronto incrociato.

### 8.7 Copertura di test

Da 1 asserzione a **564**, distribuite su otto eseguibili registrati in `ctest`:

| Eseguibile | Asserzioni | Copre |
|---|---:|---|
| `efis_core_tests` | 264 | Campione, salute nodi, parser e adapter MAVLink, registro di volo, replay, display |
| `efis_safety_tests` | 92 | Valore fermo e confronto incrociato |
| `efis_arbitration_tests` | 56 | Arbitrato per grandezza, failover e reversione |
| `efis_esp32_tests` | 38 | Adapter del nodo sensori ESP32 |
| `efis_ingest_tests` | 36 | Che le protezioni siano raggiunte dai byte, e la registrazione verso la black box |
| `efis_bench_tests` | 33 | Il banco prova alimentato dalla registrazione reale |
| `efis_can_source_tests` | 27 | Trasporto CAN, formato candump, sorgente che non si apre |
| `efis_engine_tests` | 18 | Nodo motore |

---

## 8bis. P4 — EMS reale, in corso

### 8bis.1 Il protocollo non è stato inventato

Nel progetto non esisteva alcuna specifica del nodo sensori: l'handoff dichiara
al paragrafo 6 che manca la definizione delle sorgenti reali. È stata **ricavata
dal firmware vero**, `EFIS_Freenove_v4_8_.ino` versione 4.0.0, che rimanda a un
`PROTOCOLLO_CAN.md` non presente in nessun repository. Il codice che trasmette
era quindi l'unica descrizione autorevole, ed è stato letto invece che
immaginato. Il risultato è in `docs/protocols/ESP32_CAN_V1.md`, che dichiara la
propria provenienza: una specifica ricavata da un sorgente altrui non ha lo
stesso valore di una concordata, e sul bus non viaggia alcun numero di versione.

Il nodo non è un modulo motore, come l'elenco del paragrafo 6 lascerebbe
intendere: è **assetto, dati aria e posizione**. Sensori diversi da quelli del
Pixhawk, alimentazione diversa, bus diverso, firmware diverso. È la seconda
catena indipendente che dà qualcosa da confrontare al controllo incrociato.

### 8bis.2 Stato delle voci

| # | Voce | Stato | Nota |
|---|---|---|---|
| 1 | Adapter ESP32-S3 su CAN | fatto | Con le scale, la gerarchia di calibrazione e le tre trappole del protocollo documentate |
| 2 | Trasporto SocketCAN | fatto | Provato sul bus vero, non solo su registrazioni |
| 3 | Innesto nella catena di ingestione | **da fare** | `Esp32CanAdapter` ha zero riferimenti in `src/ingest/`: la seconda catena esiste e non arbitra contro il Pixhawk |
| 4 | Mappa dei canali reale | da fare | |
| 5 | Calibrazione dei sensori | da fare | |
| 6 | Limiti verificati da manuale motore | da fare | Primo punto che richiede una fonte esterna al codice |

### 8bis.3 Che cosa ha trovato il bus vero

Il trasporto era verificato su registrazioni. Provarlo su `vcan0` del nodo black
box, iniettando i frame con `cansend`, ha trovato **tre difetti che nessuna
registrazione avrebbe potuto mostrare**:

- la pressione differenziale era dichiarata senza unità, e il monitor la mostrava
  con un punto interrogativo al posto dei Pascal. È esattamente ciò che il campo
  `unit` esiste per impedire;
- con il nodo dichiarato perduto, assetto e quota diventavano non disponibili
  mentre **la pressione differenziale restava valida**: l'invalidazione al
  silenzio copriva i soli campioni dello snapshot di volo e lasciava fuori le
  grandezze che il banco prova mostra per prime;
- il monitor deduceva "mai ricevuto" dall'identificativo di segnale, e mostrava
  come mai arrivata una pressione statica appena letta.

Valori letti dal bus attraverso il nucleo, dopo le correzioni:

```
rollio                     4.12 deg   VALIDO
beccheggio                -1.32 deg   VALIDO
prua                      90.00 deg   VALIDO
velocita indicata         87.40 kt    VALIDO
quota                   3200.00 ft    VALIDO
pressione statica       1013.20 hPa   VALIDO
pressione differenziale   47.50 Pa    VALIDO
bus: 52 frame, 0 errori, 0 remoti, 0 persi dal kernel
adapter: 12 applicati, 40 ignorati, 0 troncati
```

I 1013,20 hPa dimostrano che l'offset di 1000 viene rimosso davvero. I 40 frame
ignorati sono traffico di altri identificativi già presente su quel bus, prodotto
da `bbxlog.service`: dimostrano che l'adapter prende i propri frame e lascia
stare il resto, cosa che un bus vuoto non avrebbe potuto mostrare.

### 8bis.4 Il limite, dichiarato

**Il nodo ESP32 fisico non è ancora collegato.** `vcan0` dimostra socket, filtro
e conversione; le prove sull'adapter dimostrano la conformità alla specifica
ricavata dal firmware. Nessuna delle due dimostra che il modulo dica ciò che il
firmware sembra dire. Quando sarà collegato servirà una registrazione vera, come
è stato per il Pixhawk.

---

## 9. Vincoli confermati e non toccati

- `ARSPD_USE` resta `0`. Nessun parametro del Pixhawk è stato letto o scritto.
- Nessun controllo pre-arm disabilitato.
- Nessun backup PX4 o ArduPlane toccato.
- Nessun servizio dei nodi riavviato o riconfigurato.
- Nessun segreto scritto nel repository o nei log.
- I due progetti EFIS legacy non sono stati aperti né modificati.
- Il criterio di sicurezza Smart App Control non è stato alterato.

---

## 10. Punto esatto di ripartenza

P1 è chiuso. P4 ha i due mattoni che gli servivano, l'adapter e il trasporto, e
si ferma davanti allo stesso genere di lavoro che aveva chiuso P1: il
collegamento.

1. **Innestare il nodo ESP32 nella catena di ingestione.** `MavlinkPipeline`
   compone una sola sorgente; qui ne servono due, ed è il caso per cui
   l'arbitrato è stato fatto per grandezza. Quando le due catene saranno unite,
   il confronto incrociato avrà finalmente due opinioni indipendenti da
   confrontare invece di una sola contraddetta dalla velocità al suolo.
2. Mappa dei canali reale e calibrazione.
3. Limiti letti dal manuale motore. È il primo punto che richiede una fonte
   esterna al codice, e non va stimato.

Poi P2, la chiusura del PFD, che a differenza di P4 richiede una cattura dal
display a ogni passo.

### Aperto e non bloccante

- **Audit in sola lettura** di server e ground station. Il PFD e il nodo black
  box sono stati usati e verificati; gli altri due no.
- **Serializzazione wire** del Display Data Protocol V2. `ADR-003` ha fissato i
  confini, non il formato sul filo.
- **Prestazioni del Setup Center**: con il pannello del banco aperto sopra il
  resto, il log riportava 9,4 fps. Non è il regime del PFD e non è stato
  indagato.
- **Il nodo EMS sul RPi4** è stato messo in servizio come unità di sistema su
  eglfs. Va tenuto presente che eglfs prende il DRM master in esclusiva, quindi
  non può condividere il pannello con l'impianto precedente.

### Da non perdere di vista

Il `get_throttled` a `0x0` non chiude il punto dell'alimentazione: il registro si
azzera a ogni riavvio e la prova termica prolungata del paragrafo 8.4 non è stata
eseguita.

`ARSPD_USE` resta `0` e nessun parametro del Pixhawk è stato letto o scritto. Il
nodo sensori ESP32 non è collegato. Nulla di quanto costruito è mai stato messo
in condizione di comandare qualcosa: la scala del paragrafo 25 resta al primo
gradino, osservazione in sola lettura.

---

## 11. Nota di metodo

Nessuna delle affermazioni di questo checkpoint deriva dalla lettura
dell'handoff. Dove il documento di consegna è stato confermato, la conferma
proviene da una misura indipendente; dove è risultato incompleto, la lacuna è
riportata esplicitamente. Le catture in `output/rpi5-captures/` continuano a non
essere considerate prova dell'implementazione corrente, come prescrive il
paragrafo 22.
