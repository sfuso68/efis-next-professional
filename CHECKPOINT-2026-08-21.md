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

Il backbone dati P1, che a inizio giornata non esisteva, è poi stato costruito
nel corso della stessa: campione con provenienza, salute dei nodi, parser e
adapter MAVLink in sola lettura, registro di volo rileggibile e trasporto
astratto. Restano da collegare fra loro pezzi che esistono già.

Stato complessivo: **il banco di sviluppo è coerente e riproducibile**, e il
backbone dati è passato da assente a percorribile dall'ingresso dei byte fino
alla rilettura del registro.

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
| Ramo di lavoro | `work/p1-backbone-dati` |
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
| Test | `ctest` 2 / 2 superati |
| Temperatura finale | 54.3 °C, nessun throttling |

Il backup `CMakeLists.txt.pre-claude-20260821` è stato creato prima della copia
del file modificato.

### 5.2 Windows — ambiente secondario

MSVC 19.51.36256.0 con Ninja, sia in Debug sia in Release: **2 / 2 test
superati** in entrambe le configurazioni. Il target Qt non è costruibile, vedi
sezione 6.2.

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

### 6.2 Qt Quick 3D assente su Windows — APERTO, richiede l'utente

Diagnosi prodotta dalla correzione precedente:

```
componenti mancanti nell'installazione Qt: Qt6Quick3D
```

`C:/Qt/6.11.1/msvc2022_64` contiene solo `Qt6Quick3DSpatialAudio`; manca il
modulo Quick3D richiesto da `Terrain3DSurface.qml` e `TerrainTile3D.qml`.

Rimedio: `C:/Qt/MaintenanceTool.exe` → *Add or remove components* → Qt 6.11.1 →
*Additional Libraries* → *Qt Quick 3D*. Richiede l'interfaccia grafica.

Non è bloccante: il RPi5 compila l'intero progetto ed è il target reale.

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

## 8. Backbone dati P1 — stato di avanzamento

All'inizio della giornata il backbone dati era lavoro interamente nuovo: la
ricerca di `mavlink`, `socketcan`, `heartbeat` ed `esp32` in `src/`, `include/` e
`tests/` non trovava alcuna implementazione, e l'unico riferimento era il valore
`SourceId::pixhawk_primary` dichiarato e mai usato. Il core era una pipeline di
sola simulazione.

Quello che segue è lo stato verificato leggendo il codice, non pianificato.

### 8.1 Elenco del paragrafo 20 dell'handoff

| # | Voce | Stato | Dove |
|---|---|---|---|
| 1 | Formalizzare Display/Sensor Data Protocol V2 | parziale | Envelope e regole restano un draft. La direzione delle dipendenze e i confini di trasporto sono però ora decisi in `ADR-003` |
| 2 | Adapter MAVLink in sola lettura | fatto | Parser e adapter, nessun percorso di trasmissione in nessuno dei due |
| 3 | Snapshot normalizzato verso il core | fatto | `MavlinkAdapter::snapshot()` |
| 4 | Heartbeat, stale e perdita nodo | fatto | `NodeHealthMonitor`, con prova di mutazione |
| 5 | Black box con segmenti e replay | quasi | Il registro di volo V2 si scrive e si rilegge, e `efis::io::ByteSource` rende il replay una sorgente di prima classe. Manca il collegamento vivo: `efis-recorder` registra ancora dal `SimulationProvider`, non dall'adapter |
| 6 | Dati monitor-only al banco prova UI | da fare | Zero riferimenti all'adapter in `src/renderers/` e `qml/` |

### 8.2 Elenco del paragrafo 6 dell'handoff, lavoro core EMS

| # | Voce | Stato | Nota |
|---|---|---|---|
| 1 | Schema unico del campione sensore | fatto | 14 campi |
| 2 | Heartbeat e stato dei nodi | fatto | |
| 3 | Timeout, dato stale, perdita nodo | fatto | Un battito scartato non rinvia il timeout |
| 4 | Adapter ESP32-S3 seriale o CAN | da fare | |
| 5 | Adapter SocketCAN su Raspberry | da fare | Le interfacce `can_a`, `can_b`, `vcan0` esistono già sul nodo black box |
| 6 | Provider reali al banco prova | da fare | |
| 7 | Priorità e failover primario-secondario | parziale | `SourceArbitrator` implementato e testato, ma non collegato |
| 8 | Registrare telemetria, guasti e cambi sorgente | parziale | Il formato li prevede e li rilegge; nessuno li scrive ancora dal vivo |

### 8.3 Tre moduli di sicurezza pronti ma non collegati

Oltre all'elenco dell'handoff sono stati implementati due controlli che coprono
i modi in cui un dato può essere presente, plausibile e falso. `ADR-004` li
motiva.

- `StasisMonitor` rileva il **valore fermo**: un nodo che trasmette
  regolarmente, con sequenza crescente e checksum valido, portando un valore che
  non cambia più perché il sensore dietro è morto. Nessuno dei controlli
  precedenti lo vedeva, perché tutti guardano il flusso e non il contenuto.
- `CrossCheck` confronta grandezze ottenute per vie indipendenti e finalmente
  produce quel `Validity::disagree` che esisteva nel modello dati dal primo
  giorno senza che niente lo generasse.

Va però detto con chiarezza: insieme al `SourceArbitrator` sono **tre moduli
implementati, testati e non ancora innestati nel percorso dei dati**. Zero
riferimenti a tutti e tre in `mavlink_adapter.cpp`. Finché restano scollegati
non proteggono nulla, e il rischio è che la loro esistenza venga scambiata per
copertura.

### 8.4 Modello dati: colmato

`Sample<T>` portava 7 campi contro i 14 richiesti dal paragrafo 17.2. Ora li
porta tutti: identificativo del segnale, nodo, canale, valore grezzo, unità,
marcatore di simulazione, revisione di calibrazione e tempo UTC, quest'ultimo
con un valore esplicito per "non affidabile".

Nel farlo è emerso un difetto reale. `Validity` ammette un solo stato per volta,
quindi un campione simulato che invecchiava diventava `stale` e perdeva ogni
traccia di essere simulato: il PFD mostrava STALE, indistinguibile da un dato
reale scaduto, contro il paragrafo 19. Il marcatore vive ora in
`SampleOrigin::simulated` e sopravvive a qualsiasi transizione.

### 8.5 Decisioni architetturali registrate

I due numeri liberi nella numerazione delle ADR sono stati usati:

- `ADR-003` fissa che l'ingestion dei bus avviene su un solo nodo, la black box,
  e che i display ricevono stato normalizzato e mai frame grezzi. Dichiara le
  direzioni di dipendenza ammesse, verificabili leggendo gli include.
- `ADR-004` motiva i due controlli della sezione 8.3.

### 8.6 Copertura di test

Da 1 asserzione a **292**, distribuite su due eseguibili registrati in `ctest`:

| Eseguibile | Asserzioni | Copre |
|---|---:|---|
| `efis_core_tests` | 264 | Campione, salute nodi, parser e adapter MAVLink, registro di volo, replay, display |
| `efis_safety_tests` | 28 | Valore fermo e confronto incrociato |

Comprendono frame di riferimento prodotti da pymavlink, una registrazione reale
di ArduPlane 4.7.0 e una prova di mutazione sulla proprietà di sicurezza del
monitor dei nodi.

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

Il backbone esiste ed è percorribile dai byte in ingresso fino alla rilettura del
registro. Quello che manca non è più costruzione: è **collegamento**. Tre pezzi
esistono, sono testati e non toccano il percorso dei dati.

1. Innestare `SourceArbitrator`, `StasisMonitor` e `CrossCheck` nell'adapter. È
   il passo che trasforma tre moduli corretti in tre protezioni attive, e finché
   non è fatto la copertura è apparente.
2. Portare `efis-recorder` a registrare dall'adapter invece che dal
   `SimulationProvider`, chiudendo il percorso vivo verso la black box.
3. Portare i dati monitor-only al banco prova già esistente nel Setup Center,
   dove il pilota possa vederli senza che entrino in alcuna funzione di volo.

Poi, in ordine: gli adapter ESP32-S3 e SocketCAN, che aprono P4, e solo dopo la
chiusura del PFD, che è P2.

### Aperto e non bloccante

- **Qt Quick 3D su Windows** (sezione 6.2). Richiede il MaintenanceTool e quindi
  l'utente. Il Raspberry Pi 5 compila l'intero progetto, perciò il lavoro
  prosegue comunque.
- **Audit in sola lettura** di server, black box e ground station (sezione 7).
  Solo il PFD è stato verificato.
- **Serializzazione wire** del Display Data Protocol V2, ancora da decidere.
  `ADR-003` ha fissato i confini, non il formato sul filo.

### Da non perdere di vista

Il `get_throttled` a `0x0` non chiude il punto dell'alimentazione: il registro si
azzera a ogni riavvio e la prova termica prolungata del paragrafo 8.4 non è stata
eseguita.

`ARSPD_USE` resta `0` e nessun parametro del Pixhawk è stato scritto. La scala
del paragrafo 25 non è stata risalita di un gradino: tutto il lavoro di questa
giornata resta osservazione in sola lettura.

---

## 11. Nota di metodo

Nessuna delle affermazioni di questo checkpoint deriva dalla lettura
dell'handoff. Dove il documento di consegna è stato confermato, la conferma
proviene da una misura indipendente; dove è risultato incompleto, la lacuna è
riportata esplicitamente. Le catture in `output/rpi5-captures/` continuano a non
essere considerate prova dell'implementazione corrente, come prescrive il
paragrafo 22.
