# EFIS NEXT — Checkpoint operativo

Data: 21 agosto 2026
Riferimento: `CLAUDE_CODE_HANDOFF_2026-08-21.md`
Checkpoint precedente: `CHECKPOINT-2026-08-18.md`

Questo checkpoint consolida il primo ciclo di lavoro richiesto al paragrafo 21
dell'handoff. Non è un riepilogo del documento di consegna: riporta soltanto ciò
che è stato **verificato eseguendo comandi** il 21 agosto 2026, distinguendo la
misura dall'interpretazione. Nessuna riga di codice avionico è stata modificata.

---

## 1. Sintesi

Il progetto è stato messo sotto controllo di versione per la prima volta ed è
stata rimossa una condizione che nascondeva un fallimento di build. Sono stati
verificati in modo indipendente l'ambiente Raspberry Pi 5, l'allineamento fra le
copie del sorgente e l'intera catena build/test. Sono emersi tre problemi che
l'handoff non riportava, tutti diagnosticati; due risolti, uno che richiede un
intervento dell'utente.

Stato complessivo: **il banco di sviluppo è coerente e riproducibile**. Il punto
di ripartenza indicato dall'handoff, il backbone dati P1, risulta confermato come
non ancora iniziato.

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
| Configurazione | riuscita |
| Target compilati | 106 / 106, `efis-pfd-qt` compreso |
| Test | `ctest` 1 / 1 superato |
| Temperatura finale | 57.6 °C, nessun throttling |

Il backup `CMakeLists.txt.pre-claude-20260821` è stato creato prima della copia
del file modificato.

### 5.2 Windows — ambiente secondario

Configurazione, build e test da zero con MSVC 19.51.36256.0 e Ninja: **7 / 7
target del core**, 1 / 1 test superato. Il target Qt non è costruibile, vedi
sezione 6.

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
(`HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy\VerifiedAndReputablePolicyState = 1`)
e blocca l'esecuzione degli eseguibili non firmati creati durante la sessione.
`ctest` fallisce con `BAD_COMMAND` / "Process not started" e l'esecuzione diretta
con "Un criterio di controllo dell'applicazione ha bloccato il file".

Non è una regressione del codice e **non è stato modificato**: è un criterio di
sicurezza di sistema.

Conseguenza operativa, da considerare permanente: su Windows si compila il core
C++, ma **la validazione di build e test va eseguita sul Raspberry Pi 5**, che è
anche il target reale. Il RPi5 è raggiungibile via SSH a chiave senza password.

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

Sono stati completati i primi quattro passi. Quello che segue e' lo stato
verificato, non pianificato.

### 8.1 Elenco del paragrafo 20 dell'handoff

| # | Voce | Stato | Dove |
|---|---|---|---|
| 1 | Formalizzare Display/Sensor Data Protocol V2 | parziale | `docs/protocols/DISPLAY_DATA_PROTOCOL_V2.md` resta un draft: envelope e regole sono definiti, la serializzazione sul filo attende una ADR dedicata |
| 2 | Adapter MAVLink in sola lettura | fatto | `da02d98`, `85bad4e` |
| 3 | Snapshot normalizzato verso il core | fatto | `MavlinkAdapter::snapshot()` |
| 4 | Heartbeat, stale e perdita nodo | fatto | `9ca517d` |
| 5 | Blackbox recorder con segmenti e replay | da fare | `SegmentedRecorder` esiste e ha i suoi test, ma nulla lo collega all'adapter |
| 6 | GNSS, assetto, baro e airspeed al banco prova UI | da fare | nessun riferimento all'adapter in `src/renderers/` o `qml/` |

### 8.2 Elenco del paragrafo 6 dell'handoff, lavoro core EMS

| # | Voce | Stato | Nota |
|---|---|---|---|
| 1 | Schema unico del campione sensore | fatto | 14 campi, `7c8c329` |
| 2 | Heartbeat e stato dei nodi | fatto | `NodeHealthMonitor` |
| 3 | Timeout, dato stale, perdita nodo | fatto | con prova di mutazione |
| 4 | Adapter ESP32-S3 seriale o CAN | da fare | |
| 5 | Adapter SocketCAN su Raspberry | da fare | le interfacce `can_a`, `can_b`, `vcan0` esistono gia' sul nodo black box |
| 6 | Provider reali collegati al banco prova | da fare | |
| 7 | Priorita' e failover primario-secondario | parziale | `SourceArbitrator` e' implementato e coperto dai test, ma l'adapter non lo usa ancora: zero riferimenti in `mavlink_adapter.cpp` |
| 8 | Registrazione di telemetria, guasti e cambi sorgente | da fare | dipende dalla voce 5 del paragrafo precedente |

### 8.3 Modello dati: colmato

`Sample<T>` portava 7 campi contro i 14 richiesti dal paragrafo 17.2. Ora li
porta tutti: identificativo del segnale, nodo, canale, valore grezzo, unita',
marcatore di simulazione, revisione di calibrazione e tempo UTC, quest'ultimo
con un valore esplicito per "non affidabile".

Nel farlo e' emerso un difetto reale. `Validity` ammette un solo stato per volta,
quindi un campione simulato che invecchiava diventava `stale` e perdeva ogni
traccia di essere simulato: il PFD mostrava STALE, indistinguibile da un dato
reale scaduto, contro il paragrafo 19. Il marcatore vive ora in
`SampleOrigin::simulated` e sopravvive a qualsiasi transizione.

### 8.4 Copertura di test

Da 1 a **212 asserzioni**, sempre in un solo eseguibile registrato in `ctest`.
Comprendono i frame di riferimento prodotti da pymavlink, una registrazione
reale di ArduPlane 4.7.0 e una prova di mutazione sulla proprieta' di sicurezza
del monitor dei nodi.

Resta valida l'osservazione che un solo test registrato non basta: andranno
separati per sottosistema quando il numero crescera' ancora.

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

I passi da 1 a 4 del paragrafo 21.6 sono chiusi. Il punto di ripartenza si
sposta ai due che restano, entrambi di collegamento e nessuno dei quali richiede
di toccare il Pixhawk.

1. Collegare l'adapter al `SegmentedRecorder`, cosi' che telemetria, transizioni
   di stato dei nodi e cambi sorgente finiscano nella black box con segmenti e
   replay deterministico.
2. Portare i dati monitor-only al banco prova gia' esistente nel Setup Center,
   dove il pilota possa vederli senza che entrino in alcuna funzione di volo.
3. Far usare all'adapter il `SourceArbitrator`, che oggi e' implementato e
   testato ma non collegato: e' il presupposto del failover fra sensore primario
   e secondario.

Poi, in ordine, gli adapter ESP32-S3 e SocketCAN, che aprono P4, e solo dopo la
chiusura del PFD, che e' P2.

### Aperto e non bloccante

- **Qt Quick 3D su Windows** (sezione 6.2). Richiede il MaintenanceTool e quindi
  l'utente. Il Raspberry Pi 5 compila l'intero progetto, percio' il lavoro
  prosegue comunque.
- **Audit in sola lettura** di server, black box e ground station (sezione 7).
  Solo il PFD e' stato verificato.
- **Serializzazione wire** del Display Data Protocol V2, che attende una ADR: i
  numeri 003 e 004 sono liberi.

### Da non perdere di vista

Il `get_throttled` a `0x0` non chiude il punto dell'alimentazione: il registro si
azzera a ogni riavvio e la prova termica prolungata del paragrafo 8.4 non e'
stata eseguita. `ARSPD_USE` resta `0` e la scala del paragrafo 25 non e' stata
risalita di un gradino: tutto quanto fatto oggi resta osservazione in sola
lettura.

---

## 11. Nota di metodo

Nessuna delle affermazioni di questo checkpoint deriva dalla lettura
dell'handoff. Dove il documento di consegna è stato confermato, la conferma
proviene da una misura indipendente; dove è risultato incompleto, la lacuna è
riportata esplicitamente. Le catture in `output/rpi5-captures/` continuano a non
essere considerate prova dell'implementazione corrente, come prescrive il
paragrafo 22.
