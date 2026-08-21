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

## 8. Stato del backbone dati P1

Il paragrafo 21 dell'handoff indica il backbone dati come primo lavoro da
svolgere. La verifica conferma che si tratta di lavoro interamente nuovo.

Ricerca di `mavlink`, `socketcan`, `heartbeat` ed `esp32` in `src/`, `include/` e
`tests/`: **nessuna implementazione**. L'unico riferimento è il valore
`SourceId::pixhawk_primary` in `include/efis/core/types.hpp`, dichiarato e mai
usato. Il core è oggi una pipeline di sola simulazione. L'unico codice MAVLink
esistente sono i quattro script Python di audit in `tools/hardware/`.

### 8.1 Distanza dal modello dati richiesto

`include/efis/core/sample.hpp` definisce 7 campi: `value`, `source`,
`sampled_at`, `received_at`, `quality`, `validity`, `sequence`.

Il paragrafo 17.2 dell'handoff ne richiede 14. Mancano: identificativo del
segnale, identificativo del nodo, canale, valore grezzo, unità di misura, flag di
simulazione, revisione di calibrazione e timestamp UTC.

### 8.2 Fondamenta già utilizzabili

`SourceArbitrator` in `include/efis/core/source_arbitrator.hpp` implementa già
selezione, failover e reversione con tempo di permanenza configurabile, ed emette
un evento a ogni cambio sorgente. È la base su cui innestare il modello di salute
dei nodi, senza riscritture.

`docs/protocols/DISPLAY_DATA_PROTOCOL_V2.md` esiste come draft implementabile con
envelope, regole di rifiuto e gestione dei duplicati già definite. La
serializzazione wire è esplicitamente rimandata a una ADR separata.

### 8.3 Copertura di test

Un solo eseguibile di test, un solo test registrato in `ctest`. Insufficiente
rispetto alla Definition of Done del paragrafo 21. Va ampliata contestualmente al
lavoro su P1, non dopo.

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

Invariato rispetto al `CHECKPOINT-2026-08-18.md` e coerente con il paragrafo 21.6
dell'handoff: **modello dati e adapter MAVLink in sola lettura**, prima di
qualsiasi intervento su UI e terreno.

Ordine previsto:

1. Estendere il campione sensore ai 14 campi del paragrafo 17.2, mantenendo
   `Sample<T>` compatibile con il codice esistente.
2. Definire heartbeat e stato dei nodi.
3. Implementare timeout, dato stale e perdita nodo, con test deterministici.
4. Adapter MAVLink in sola lettura, senza alcuna scrittura di parametri.
5. Collegare i dati reali in modalità monitor-only al core e alla black box.
6. Solo dopo, stabilizzare UI e terreno.

### Da fare prima, non bloccanti

- Installare Qt Quick 3D su Windows (sezione 6.2).
- Eseguire l'audit in sola lettura di server, black box e ground station
  (sezione 7).
- Ampliare la copertura di test contestualmente a P1 (sezione 8.3).

---

## 11. Nota di metodo

Nessuna delle affermazioni di questo checkpoint deriva dalla lettura
dell'handoff. Dove il documento di consegna è stato confermato, la conferma
proviene da una misura indipendente; dove è risultato incompleto, la lacuna è
riportata esplicitamente. Le catture in `output/rpi5-captures/` continuano a non
essere considerate prova dell'implementazione corrente, come prescrive il
paragrafo 22.
