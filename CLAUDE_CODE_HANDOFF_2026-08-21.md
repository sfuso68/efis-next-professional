# EFIS NEXT — Handoff completo per Claude Code

**Data di consolidamento:** 21 agosto 2026  
**Progetto attivo:** `efis-next`  
**Scopo:** consegnare a Claude Code lo stato tecnico e operativo completo, evitando regressioni, duplicazioni e modifiche ai due EFIS legacy.

---

## 1. Istruzione fondamentale

Il lavoro deve proseguire **esclusivamente sul nuovo progetto `efis-next`**.

- I due progetti EFIS preesistenti sono funzionanti e devono restare separati, invariati e disponibili.
- Non copiare meccanicamente l'architettura del vecchio software.
- Recuperare dai sistemi precedenti soltanto concetti, dati, algoritmi o componenti dimostratisi validi, dopo revisione.
- EFIS NEXT deve essere una nuova piattaforma professionale, modulare e verificabile.
- Una sola codebase deve supportare i profili **Standard** e **Professional**.
- Non presentare il sistema come avionica certificata. Tutto ciò che non ha completato una procedura formale di validazione deve essere marcato `DEVELOPMENT`, `EXPERIMENTAL` o `NON-CERT`.

Claude Code deve iniziare leggendo questo documento, quindi `README.md`, `CHECKPOINT-2026-08-18.md`, le ADR e le specifiche richiamate nelle sezioni successive.

---

## 2. Visione del prodotto

EFIS NEXT è una piattaforma avionica distribuita per VDS/LSA e aviazione generale sperimentale, progettata per fornire:

- PFD ad alta leggibilità con grafica moderna e adattabile;
- MFD/navigazione, radionavigazione e pianificazione;
- synthetic vision e terreno 3D con TAWS;
- EMS evoluto e strumenti motore;
- integrazione Pixhawk 6X/ArduPilot tramite MAVLink;
- sensore aria Matek ASPD-DLVR tramite DroneCAN;
- GNSS M10 collegato al Pixhawk;
- telemetria distribuita fra più nodi;
- black box indipendente;
- meteo, METAR/TAF e radar meteo;
- FPV e gestione futura di quattro telecamere;
- strumenti accessori su display piccoli, anche circolari, 4/5/7 pollici;
- setup unificato per aeromobile, motore, sensori, display, rete e funzioni;
- funzionamento degradato e offline senza dipendere costantemente da Internet.

La filosofia non è replicare una singola suite commerciale. La UI può studiare Garmin, Dynon, G3X, SkyView, avionica business e automotive/HMI professionali, ma deve produrre un'identità grafica originale e non copiare risorse protette.

---

## 3. Architettura operativa prevista

### 3.1 Nodi attuali

| Nodo | Hostname/utente | IP osservato | Ruolo |
|---|---|---:|---|
| Raspberry Pi 5 PFD | `efis@efis-pfd-01` | `192.168.1.45` | Display primario, Qt/QML, synthetic vision, cache locale |
| Server centrale mini-PC | `sfuso@serverha` | `192.168.1.6` | Mappe, imagery, cataloghi e distribuzione dati |
| Black box | `pi@efisblackbox` | `192.168.1.47` | Registrazione, edge weather e futuro broker dati |
| Portatile Ground Station | `stefano@PORTATILE` / hostname osservato `Port-hp` | `192.168.1.39` | QGroundControl, MAVProxy, configurazione Pixhawk |

Gli indirizzi sono DHCP/LAN osservati e non devono essere considerati immutabili. Introdurre in seguito prenotazioni DHCP, DNS locale e discovery controllato.

### 3.2 PFD Raspberry Pi 5

Configurazione verificata:

- Debian GNU/Linux 13 `trixie`;
- architettura `aarch64`;
- Qt 6.8.2;
- CMake 3.31.6;
- Ninja 1.12.1;
- GCC/G++ 14.2.0;
- display touch QDtech/Mediatrix MPI7002/MPI7003;
- risoluzione nativa attiva 1024 × 600 a circa 60 Hz;
- touchscreen USB `0484:5750` rilevato;
- Wayland/wlroots;
- servizio SSH attivo;
- servizio utente `efis-pfd.service` usato per il kiosk.

È stato osservato `vcgencmd get_throttled = 0x50000`: indica che nella storia del dispositivo si sono verificati eventi di undervoltage/throttling. Anche se non necessariamente attivi nel momento della misura, alimentazione, cavo e dissipazione devono essere verificati prima di prove lunghe o di volo.

### 3.3 Black box Raspberry Pi

Configurazione verificata:

- Debian 13 `trixie`, `aarch64`;
- 4 GiB RAM, 2 GiB swap/zram;
- NVMe Crucial P310 da circa 500 GB come filesystem root;
- microSD presente ma il sistema opera da NVMe;
- Wi-Fi attivo;
- Ethernet senza link al momento della rilevazione;
- interfacce `can_a`, `can_b` e `vcan0` presenti.

Ruolo deciso: non soltanto registratore, ma **edge avionics node** per:

- black box append-only;
- raccolta e normalizzazione telemetria;
- cache e distribuzione meteo;
- broker delle informazioni aggiuntive;
- health monitoring degli altri nodi;
- buffering durante perdita del server centrale;
- sincronizzazione differita quando la rete ritorna.

### 3.4 Server centrale

Il server `serverha` ospita `/srv/stack` e un servizio Docker `mapapp`. È esposto tramite Caddy all'indirizzo locale:

`https://mappe.casa.lan`

Il certificato root locale Caddy è stato installato sul PFD. L'accesso è protetto da Basic Auth. **Non inserire password, token o hash nel repository.** Usare secret locali, environment file con permessi restrittivi o un secret store.

Endpoint `/api/status` verificato. Al momento della verifica dichiarava disponibili:

- navdata OpenAIP;
- DEM SRTM1;
- OSM vector;
- tile OSM/Geofabrik via Planetiler;
- hillshade;
- aeroway;
- servizi OpenAIP;
- circuiti di traffico;
- piramide raster.

Il volume Docker `hangar_flightplans` esiste già ma Compose avverte che non è stato creato dallo stack corrente. Correggere il Compose dichiarandolo `external: true` solo dopo aver confermato nome e ownership; non ricrearlo o cancellarlo.

---

## 4. Stato del repository `efis-next`

### 4.1 Principi architetturali già adottati

- C++20 per il core.
- Core senza dipendenza Qt.
- Qt Quick/QML come adapter di presentazione, non come dominio.
- Monolite modulare nella prima fase, con confini chiari fra sottosistemi.
- Campioni sensore con provenance, timestamp monotono, sequenza, qualità e validità.
- Snapshot di volo immutabili per i consumer.
- Simulazione deterministica.
- Health model e degradazione espliciti.
- Sorgenti arbitrate invece di collegamenti diretti sensore-widget.
- Dati simulati sempre riconoscibili.
- Configurazioni atomiche con backup, audit e rollback.

ADR già presenti:

- `docs/adr/ADR-001-language-and-build.md`
- `docs/adr/ADR-002-modular-monolith.md`
- `docs/adr/ADR-005-source-arbitration.md`

### 4.2 Build

Build standard:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Windows con Qt MSVC:

```sh
cmake --preset dev -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64
cmake --build --preset dev
```

Linux/Wayland kiosk:

```sh
EFIS_KIOSK=1 QT_QPA_PLATFORM=wayland ./build/dev/efis-pfd-qt
```

Lo stato storico indica compilazione ARM completata e test automatici superati. Claude deve comunque rieseguire build e test prima di modificare il codice e riportare esattamente eventuali fallimenti correnti.

### 4.3 Componenti UI già presenti

File QML principali:

- `AdaptiveShell.qml`
- `AdaptivePfd.qml`
- `AdaptiveNavigationView.qml`
- `AdaptiveMissionView.qml`
- `AdaptiveEmsView.qml`
- `AdaptiveCanvasView.qml`
- `AdaptiveFpvView.qml`
- `ProfessionalVisionSurface.qml`
- `ProfessionalTape.qml`
- `ProfessionalCircularGauge.qml`
- `ProfessionalSystemsDeck.qml`
- `ProfessionalFma.qml`
- `HeadingTape.qml`
- `Hsi.qml`
- `Terrain3DSurface.qml`
- `TerrainTile3D.qml`
- `GpuMapSurface.qml`
- `WeatherRadarOverlay.qml`
- `MaintenanceOverlay.qml`

La UI include già prototipi/catture per PFD, Canvas, EMS, navigazione, meteo, setup, terreno e black box. Le catture in `output/rpi5-captures/` costituiscono una cronologia visuale, non tutte sono target finali.

### 4.4 Stato grafico e richieste ancora aperte

Il livello grafico è migliorato molto ma il proprietario considera il risultato ancora non definitivo. Requisiti espliciti:

- leggibilità e qualità percepita da prodotto professionale;
- layout proporzionato e senza spazi sprecati;
- nastri laterali trasparenti e senza cornici pesanti;
- valori IAS/ALT separati dalla scala senza sovrapposizioni;
- bande velocità progressive e aderenti alla scala;
- IAS che cambia colore coerentemente con le bande e i limiti;
- riferimenti Vx/Vy evidenti a destra del nastro velocità;
- V-speeds complete e configurabili dal profilo aeromobile, inclusa Vne e le velocità pertinenti all'LSA;
- bussola superiore più definita, trasparente e graduata;
- HSI/bussola inferiore circolare, mai ellittica;
- arco inferiore con geometria corretta e informazioni di navigazione interne, proporzionate e non interferenti;
- synthetic vision compresso in modo da non interferire con gli strumenti;
- pista prospettica mostrata solo con aeroporto/pista validi e spostata correttamente nella profondità;
- cielo senza banding visibile;
- strumenti EMS circolari più grandi;
- CHT/EGT correttamente allineati e collegati al simulatore motore;
- tensione/corrente più compatte, disposte verticalmente;
- menu verticale, non orizzontale;
- tocco sull'HSI inferiore apre la pagina di navigazione VOR/NDB/GPS;
- tutte le pagine devono adattarsi a 1024×600 e a risoluzioni superiori.

Problema più recente segnalato sul PFD: il nastro altitudine di destra aveva sovrapposizioni e gli strumenti circolari non risultavano realmente ingranditi del 30%. Non considerare questa correzione conclusa senza nuova cattura dal vero RPi5 e confronto pixel/layout.

Specifiche utili:

- `docs/design/EFIS_UI_DESIGN_SYSTEM.md`
- `docs/design/PFD_FOCUS_1024_SPEC.md`
- `docs/design/PFD_PROFESSIONAL_V1.md`
- `docs/design/ADAPTIVE_MISSION_DECK_CONCEPT.md`
- `docs/design/NAV_CONTROL_CENTER_V1.md`

---

## 5. Setup Center, cataloghi e profili

È stato implementato un Unified Setup Center accessibile con pressione prolungata di uno/due secondi sul logo EFIS NEXT.

Funzioni implementate/prototipate e verificate sul RPi5:

- catalogo normalizzato di aeromobili e motori;
- selezione e staging del profilo aeromobile;
- revisione di pesi, carburante, motore e V-speeds;
- verifica obbligatoria con riferimento POH/AFM;
- attivazione atomica del profilo;
- backup, audit e rollback;
- selezione/staging profilo motore;
- limiti RPM, olio, CHT, EGT e carburante;
- mappatura sensori EMS per sorgente, nodo e canale;
- rilevamento canali mancanti o duplicati;
- banco prova canale con raw, engineering value, frequenza e sample age;
- gain/offset temporanei;
- iniezione e cancellazione timeout;
- modalità `BENCH_SIM_ACTIVE` distinta dai provider reali.

File/dati rilevanti:

- `config/active/aircraft-profile.json`
- `config/aircraft/lsa-sim.json`
- `data/catalog/efis-catalog.db`
- `data/catalog/efis-catalog-index.json`
- `tools/catalog/catalog_tool.py`
- `docs/design/EFIS_CATALOG_V1.md`

Vincoli:

- nessun limite aeromobile/motore indicativo deve diventare attivo senza verifica su POH/AFM/manuale motore;
- niente assegnazione automatica hardware definitiva;
- setup operativo soltanto a terra e con condizioni di sicurezza verificabili;
- ogni modifica critica deve lasciare audit e possibilità di rollback.

---

## 6. EMS e simulatore motore

È presente una simulazione motore e un protocollo documentato in:

`docs/protocols/EMS_SIMULATION_V1.md`

Sono presenti catture di EMS avanzato e del simulatore in `output/rpi5-captures/`, fra cui:

- `ems-advanced-operational-v1.png`
- `ems-advanced-operational-v2.png`
- `ems-simulator-v2.png`
- `pfd-cht-egt-engine-sim-v1.png`
- `canvas-engine-deck-v3.png`

Stato funzionale:

- CHT ed EGT sono stati collegati alla simulazione sulle viste PFD/Canvas;
- gli altri parametri principali sono rappresentati con indicatori circolari;
- la UI motore richiede ancora revisione dimensionale e visuale;
- manca il provider hardware normalizzato completo ESP32-S3/CAN/RPi;
- manca la definizione finale delle sorgenti reali per ogni sensore motore;
- manca una procedura completa di calibrazione e collaudo del cablaggio reale.

Prossimo lavoro core EMS:

1. definire schema unico del campione sensore;
2. heartbeat e health dei nodi;
3. timeout/stale/lost node;
4. adapter ESP32-S3 seriale e/o CAN;
5. adapter SocketCAN Raspberry;
6. collegamento provider reali al banco prova;
7. priorità/failover sensore primario-secondario;
8. logging di telemetria, guasti e cambi sorgente nella black box.

---

## 7. Navigazione e radionavigazione

Sono presenti:

- database `data/navigation/navigation.db`;
- indici navigazione e radionavigazione;
- strumenti di importazione in `tools/navigation/`;
- route LIPH–LIDB;
- selettore aeroporti e nearest airport nel Setup Center;
- prototipo Navigation Control Center;
- prototipo pagina radio NAV;
- configurazioni scenari Treviso, Belluno e Dolomiti.

File principali:

- `docs/design/NAV_CONTROL_CENTER_V1.md`
- `docs/design/TERRAIN_NAV_DATABASE_V1.md`
- `config/routes/liph-lidb.json`
- `config/scenarios/treviso-belluno-route.json`
- `config/scenarios/treviso-liph.json`
- `data/navigation/navigation.db`

Da completare:

- modello unico flight plan/leg/waypoint;
- gestione CDI/OBS/BRG/DTK/TRK e validità sorgenti;
- sorgenti VOR/NDB/GPS selezionabili e annunciabili;
- integrazione dati Pixhawk senza delegargli la responsabilità della UI;
- nearest, direct-to, diversione e alternati;
- validazione database e ciclo aggiornamenti;
- distinzione chiara fra dati ufficiali, comunitari e sperimentali;
- test con perdita GPS e dati stale;
- apertura della pagina NAV tramite tocco sull'HSI inferiore.

---

## 8. Terreno 3D, imagery e ostacoli

### 8.1 Dati Copernicus

Completato il download/audit di 12 celle Veneto del dataset:

`COP-DEM_GLO-30-DGED/2024_1`

Pacchetto attivo dichiarato:

`cop-dem-glo30-veneto-2024_1`

Caratteristiche:

- datum verticale EGM2008;
- stato `DEVELOPMENT / NON-CERT`;
- checksum e 17 bordi condivisi verificati;
- differenza massima osservata ai bordi: 0 m;
- quota campione LIPH verificata;
- corretto fallback del renderer per evitare sostituzione silenziosa con Mapzen quando il pacchetto richiesto è incompleto;
- prestazioni Wayland osservate circa 32–33 fps.

Il token CDSE ha funzionato dopo averlo generato correttamente. Non salvare mai il token nel repository. Il placeholder `IL_TOKEN_REALE` non è un token valido.

### 8.2 Texture Sentinel-2

È stata scaricata correttamente una texture:

`data/terrain/gpu/N46E012-sentinel2.png`

con bbox `[12, 46, 13, 47]`, dimensione richiesta 2048 e output di circa 10 MB. Sono presenti anche asset server-ingest per N45E012/N46E012.

### 8.3 Rendering

Componenti:

- `Terrain3DSurface.qml`
- `TerrainTile3D.qml`
- shader `qml/shaders/terrain_taws.frag`
- strumenti in `tools/terrain/` per DEM, heightfield, package, texture e validazione.

Sono state prodotte molte iterazioni per Dolomiti e tratta Treviso–Belluno, incluse:

- multi-tile;
- LOD;
- crossfade;
- texture Sentinel-2;
- TAWS dinamico;
- camera prospettica;
- pianura ad alta risoluzione.

La qualità è migliorata ma non è ancora considerata definitiva, soprattutto in pianura.

### 8.4 Gate ancora necessari

- attraversamento dinamico reale fra celle senza seam, pop o stallo;
- ulteriori punti quota indipendenti;
- prova termica prolungata sul RPi5;
- controllo memoria GPU/CPU e frametime, non soltanto FPS medio;
- gestione cache e priorità tile in base a rotta/posizione;
- fallback offline deterministico;
- ostacoli reali con provenance, altezza, datum, età e validità;
- database aeroporti/piste coerente con synthetic vision;
- allineamento pista prospettica con posizione, orientamento e soglia reali;
- decluttering per fase di volo;
- TAWS con envelope e soglie configurabili, poi validazione indipendente;
- impedire che dati mancanti siano rappresentati come terreno sicuro.

---

## 9. Server imagery e distribuzione Nord Italia

Obiettivo concordato: copertura dell'intero Nord Italia e trasferimento intelligente verso EFIS.

Modalità richieste:

1. selezione per area geografica;
2. selezione per piano di volo/corridoio.

Architettura più professionale prevista:

- baseline regionale a bassa/media risoluzione sempre disponibile;
- corridor pack ad alta risoluzione per la rotta attiva;
- destination/alternate pack prioritari;
- cache opportunistica per deviazioni;
- manifest versionati, checksum e provenance;
- download resumable;
- quota disco e garbage collection;
- nessuna eliminazione del pacchetto attivo prima che il sostituto sia verificato;
- stato cache visibile al pilota ma non invasivo.

Sincronizzazione tratta Treviso–Belluno verificata sul PFD:

- dataset `north-italy-sentinel2-development`;
- richiesti 1326 tile;
- scaricati 0 perché già presenti;
- riutilizzati 1326;
- fallimenti 0;
- dimensione cache riportata 19.573.858 byte.

File principali:

- `docs/design/IMAGERY_DISTRIBUTION_V1.md`
- `docs/design/IMAGERY_SELECTION_AND_TRANSFER_V2.md`
- `docs/deployment/IMAGERY_SERVER.md`
- `config/imagery/north-italy-development.json`
- `config/imagery/server-catalog.json`
- `tools/imagery/`
- `server/imagery_server.py`
- `server/serverha-mapapp/`

Credenziali imagery: vengono fornite mediante variabili `EFIS_IMAGERY_USER` e `EFIS_IMAGERY_PASSWORD`. Non registrarle nei log e fare `unset` della password dopo l'uso interattivo.

---

## 10. Meteo, METAR, TAF e radar

Sono presenti:

- overlay radar meteo QML;
- simulazione meteo;
- rappresentazioni METAR/TAF;
- edge weather service sul nodo black box;
- viste/catture PFD e NAV;
- possibilità prevista di connessione tramite hotspot.

File rilevanti:

- `docs/design/WEATHER_RADAR_CONNECTIVITY_V1.md`
- `qml/WeatherRadarOverlay.qml`
- `server/edge-node/edge_weather.py`
- test edge weather.

Da completare in modo professionale:

- registry provider con licenze e attribution;
- timestamp observation/product/retrieval separati;
- georeferenziazione radar verificata;
- cache, expiry e modalità stale esplicita;
- METAR raw + parsed conservando entrambi;
- TAF con periodi FM/TEMPO/BECMG corretti;
- failure mode senza rete/hotspot;
- distribuzione server centrale → blackbox → display;
- firma/checksum dei pacchetti;
- test di clock skew;
- messaggi chiari: `LIVE`, `CACHED`, `STALE`, `SIM`;
- nessun dato simulato confondibile con dati reali.

---

## 11. FPV, video e telecamere

Requisito non negoziabile: mantenere e sviluppare FPV e telemetria sovrapposta.

Il RPi4 dispone di decoder video hardware e l'idea è gestire fino a quattro telecamere. Il progetto deve valutare sia telecamere commerciali sia moduli assemblabili internamente, inclusi eventuali gimbal.

Documenti e configurazioni esistenti:

- `docs/design/EFIS_CAM_V1.md`
- `docs/hardware/EFIS_CAM_V1_BOM.md`
- `docs/protocols/VIDEO_NODE_PROTOCOL_V1.md`
- `docs/verification/EFIS_CAM_V1_TEST_PLAN.md`
- `config/video/efis-cam-lab.json`
- `AdaptiveFpvView.qml`

Da completare:

- matrice requisiti camera: sensore, HDR/WDR, lux, rolling/global shutter, lente, FOV, latenza, codec, bitrate, temperatura e alimentazione;
- pipeline GStreamer/V4L2 hardware accelerated verificata su RPi4/RPi5;
- quattro ingressi con priorità, PiP e full-screen;
- perdita segnale e freeze detection;
- timestamp video coerente con telemetria/black box;
- overlay indipendente dal flusso compresso;
- registrazione selettiva;
- protezione EMI, alimentazione e cablaggio aeronautico;
- gimbal: requisiti prima della scelta meccanica;
- test vibrazioni, rolling shutter e latenza end-to-end;
- nessuna dipendenza dalla rete esterna per FPV primario.

---

## 12. Pixhawk 6X e ArduPilot — stato reale verificato

### 12.1 Hardware e firmware

Controller rilevato:

- Pixhawk 6X / FMUv6X;
- USB vendor/product `3185:0035`;
- seriale hardware osservato `3F001D000751333439393536`;
- porte persistenti:
  - `/dev/serial/by-id/usb-ArduPilot_Pixhawk6X_3F001D000751333439393536-if00`
  - `/dev/serial/by-id/usb-ArduPilot_Pixhawk6X_3F001D000751333439393536-if02`

Firmware installato e verificato:

- **ArduPlane 4.7.0**;
- commit `1511f271`;
- ChibiOS;
- IOMCU;
- RCOut 1–16;
- tre IMU ad alta risoluzione rilevate;
- due barometri rilevati (`292369` e `1074177`).

Il firmware ufficiale usato aveva SHA-256:

`e49d4abf3d4aed5e9f936765fb7c51de0ac6aa8c3d06565fc35e84d7e41c948e`

### 12.2 Backup

Sul portatile:

- backup PX4 originale:
  `/home/stefano/EFIS-ArduPilot/backups/px4-original-1.14.3-20260820-222453`
- backup ArduPlane factory:
  `/home/stefano/EFIS-ArduPilot/backups/arduplane-4.7.0-factory-20260820-224934`

Il backup PX4 comprende parametri, tlog, audit e hash. Non cancellare, sovrascrivere o spostare questi backup senza una copia ulteriore verificata.

### 12.3 Pre-arm

Avvisi osservati e attualmente attesi perché il banco non è completo:

- safety hardware;
- accelerometri non calibrati;
- bussole non calibrate;
- GPS bad fix;
- batteria/power monitor unhealthy;
- RC assente.

Non disabilitare i controlli pre-arm per eliminare gli avvisi. Risolvere ogni causa con hardware e calibrazione reali.

---

## 13. GNSS M10 — stato reale

Il modulo GNSS è fisicamente collegato alla porta **GPS2** perché il connettore disponibile è a 6 pin.

Mappatura ArduPilot:

- GPS1 fisico non usato;
- `SERIAL3_PROTOCOL` modificato da `5` a `-1` per interrompere la ricerca sulla porta vuota;
- GPS2 fisico usa `SERIAL4_PROTOCOL=5`;
- il singolo ricevitore viene quindi esposto come GPS instance 1.

Stato osservato:

- comunicazione presente;
- `fix_type=1` / `GPS 1: Bad fix`;
- zero satelliti nella posizione di test vicino alla finestra;
- nessun LED visibile sul modulo;
- bussola esterna rilevata, oltre a una bussola interna;
- `COMPASS_DEV_ID=658961`, external=1;
- `COMPASS_DEV_ID2=331777`.

Conclusione corretta: **GNSS comunicante, fix pending**. Non concludere che sia guasto e non resettare il Pixhawk soltanto perché il LED non è visibile. Coordinate/quote ricevute senza fix sono invalide e devono essere ignorate.

Da fare:

- test con visibilità cielo reale;
- attendere cold start adeguato;
- verificare satelliti, HDOP, fix 3D e accuratezza;
- validare orientamento bussola esterna;
- calibrare bussola lontano da masse ferromagnetiche;
- verificare interference/motor current quando il sistema sarà installato.

---

## 14. Matek ASPD-DLVR L431 — stato reale

Hardware identificato dalle fotografie:

- MatekSys Digital Air Speed Sensor `ASPD-DLVR L431`;
- elemento All Sensors `DLVR-L10D`;
- collegamento DroneCAN al Pixhawk CAN1.

Parametri applicati e verificati:

```text
CAN_P1_DRIVER=1
CAN_D1_PROTOCOL=1
ARSPD_TYPE=8
ARSPD_USE=0
ARSPD_SKIP_CAL=0
ARSPD_DEVID=32003
```

Scelte importanti:

- `ARSPD_USE=0` è intenzionale: il sensore è in **monitor-only**, non ancora usato dal controllo di volo;
- CAN osservato a 1 Mbit/s;
- offset differenziale osservato circa 17,18 Pa;
- nessun errore DroneCAN rilevato.

I tubi inizialmente erano invertiti e sono stati scambiati. Con la configurazione attuale il test con ventilatore ha prodotto:

- velocità massima circa 8,33 m/s, circa 30 km/h;
- ritorno vicino a zero;
- campione finale a riposo: media circa 0,447 m/s, minimo 0,068, massimo 0,892.

Conclusione: orientamento attuale dei tubi corretto. **Non scambiarli nuovamente senza una prova strumentata che dimostri il contrario.**

Nota: ArduPlane 4.7.0 ha rifiutato la calibrazione manuale `MAV_CMD_PREFLIGHT_CALIBRATION` per questo backend DroneCAN come unsupported. Con `ARSPD_SKIP_CAL=0` resta attiva la calibrazione automatica allo startup. Non è stato considerato un guasto.

Prima di porre `ARSPD_USE=1`:

1. installazione definitiva del pitot e delle linee;
2. leak test;
3. protezione acqua/condensa;
4. zero stabile con aeromobile fermo;
5. confronto IAS con riferimento indipendente;
6. inserimento V-speeds reali dell'aereo;
7. taxi test;
8. validazione in volo con procedura prudente;
9. audit dei log;
10. solo dopo approvazione esplicita, abilitazione al flight control.

Non usare aria compressa e non soffiare direttamente a bocca sul sensore.

---

## 15. Portatile Ground Station

Sistema:

- Debian 13 x86_64;
- 16 GiB RAM;
- circa 423 GB liberi al momento dell'installazione;
- utente `stefano` nei gruppi `dialout` e `plugdev`;
- ModemManager rimosso per evitare interferenze con la seriale Pixhawk;
- suspend, hibernate e hybrid sleep mascherati;
- sospensione GNOME su AC/batteria disattivata;
- chiusura coperchio impostata per non sospendere.

### 15.1 MAVProxy

Installazione isolata:

`/home/stefano/.local/share/efis-ardupilot/venv`

Versioni:

- MAVProxy 1.8.74;
- pymavlink 2.4.49;
- pyserial 3.5;
- `setuptools==80.9.0` fissato per compatibilità `pkg_resources`;
- pacchetto `future` installato.

Collegamenti:

- `~/.local/bin/mavproxy.py`
- `~/.local/bin/mavlogdump.py`

Directory operative:

`~/EFIS-ArduPilot/{logs,backups,audits,tools,firmware,qgroundcontrol}`

### 15.2 QGroundControl

Installato QGroundControl **5.0.8 stabile** ufficiale.

File:

`/home/stefano/EFIS-ArduPilot/qgroundcontrol/QGroundControl-5.0.8-x86_64.AppImage`

SHA-256:

`06969c67ef58ea063def0a8271447a1cc385438c4a7df36813315b4475146737`

Poiché Debian 13 non aveva tutte le librerie FUSE, l'AppImage è stata estratta in spazio utente e viene lanciata da:

`/home/stefano/EFIS-ArduPilot/qgroundcontrol/squashfs-root/AppRun`

Collegamento nel menu:

`~/.local/share/applications/org.mavlink.qgroundcontrol.desktop`

Comando disponibile:

`~/.local/bin/qgroundcontrol`

Stato finale: QGC installato; Pixhawk visto sulle porte USB; nessun MAVProxy/QGC occupava la porta nel controllo conclusivo. L'avvio visuale e la lettura completa dei parametri devono essere confermati dall'utente sul desktop.

Regola: non eseguire contemporaneamente QGroundControl e MAVProxy sulla stessa porta seriale, salvo configurazione intenzionale di routing MAVLink.

---

## 16. Tool hardware aggiunti al progetto

In `tools/hardware/` sono presenti:

- `audit_pixhawk.py`
- `audit_gnss.py`
- `audit_airspeed.py`
- `set_mavlink_parameter.py`

Attenzione: `set_mavlink_parameter.py` ha incontrato un `TypeError` pymavlink durante un tentativo. Non è stata effettuata alcuna scrittura in quel tentativo. Prima di riutilizzarlo:

- riprodurre il problema con un mock o SITL;
- correggere la firma/typing per `param_set_send`;
- aggiungere read-back obbligatorio;
- aggiungere timeout e rollback;
- non provarlo direttamente su parametri critici del Pixhawk reale.

Per le modifiche effettuate finora è stato preferito MAVProxy con batch controllati e verifica del valore letto indietro.

Uploader ufficiale salvato sul portatile:

`~/EFIS-ArduPilot/tools/uploader.py`

---

## 17. Black box e data backbone — progetto da completare

Il blackbox node deve diventare un nodo autonomo e non un semplice disco di log.

### 17.1 Responsabilità proposte

- ingestion MAVLink, CAN, ESP32-S3 e servizi di rete;
- normalizzazione messaggi;
- append-only flight log;
- event log separato;
- health e heartbeat;
- ring buffer ad alta frequenza;
- segmentazione per volo;
- indice e metadata;
- replica verso server centrale;
- cache meteo e nav data;
- distribuzione locale ai display;
- clock discipline;
- export diagnostico e replay.

### 17.2 Modello dati minimo

Ogni campione deve includere almeno:

- schema/version;
- signal ID;
- source/provider ID;
- node ID e channel;
- valore raw;
- valore ingegneristico;
- unità;
- timestamp monotono locale;
- UTC quando affidabile;
- sequence counter;
- validity;
- quality;
- simulated flag;
- calibration/profile revision.

### 17.3 Affidabilità

- scrittura append-only e fsync parametrico;
- protezione da perdita alimentazione;
- rotazione controllata;
- retention per spazio, non per cancellazione indiscriminata;
- checksum per segmento;
- manifest di volo;
- nessun singolo database monolitico che possa corrompere tutto;
- replay deterministico nel simulatore;
- dati sensibili e credenziali esclusi dai log.

---

## 18. Standard e Professional

Una sola codebase, differenze gestite tramite capability/profile, non fork.

### Standard

- PFD essenziale;
- EMS base;
- GPS e navigazione base;
- logging;
- display 1024×600 o accessori piccoli;
- synthetic vision semplificata/opzionale;
- meno nodi e meno ridondanza.

### Professional

- PFD/MFD completi;
- synthetic vision avanzata;
- terrain/obstacle/TAWS;
- meteo e cache distribuita;
- FPV multi-camera;
- Pixhawk/ArduPilot e sensori CAN;
- blackbox edge;
- failover sorgenti;
- diagnostica e maintenance avanzate;
- supporto display multipli.

Non usare `if professional` sparsi nella UI. Definire un capability model centrale, validato, che permetta anche hardware differenti senza duplicare il codice.

---

## 19. Sicurezza e limiti operativi

Queste regole devono rimanere obbligatorie:

- nessun valore simulato può presentarsi come reale;
- nessun dato stale può mantenere l'aspetto di dato valido;
- niente auto-arm e niente disabilitazione indiscriminata dei pre-arm check;
- nessun servo/motore deve essere comandato durante test software non specificamente autorizzati;
- rimuovere elica durante attività da banco;
- `ARSPD_USE` resta 0 finché la validazione reale non è completata;
- setup critico disponibile soltanto a terra;
- tutte le configurazioni critiche devono avere read-back, audit e rollback;
- evitare token e password nella shell history e nel repository;
- non modificare firmware/parametri Pixhawk senza backup e consenso esplicito;
- ogni funzione di supporto al volo deve definire failure mode e degradazione;
- nessuna dichiarazione di certificazione senza processo dedicato.

---

## 20. Priorità operative consigliate

### P0 — Proteggere lo stato raggiunto

1. Inventario e hash del repository locale e dei nodi.
2. Eseguire build/test senza modifiche.
3. Creare backup aggiornato dei parametri Pixhawk dopo GPS/CAN/airspeed.
4. Acquisire screenshot QGC dello stato sensori e pre-arm.
5. Registrare versioni OS, pacchetti, firmware e servizi.
6. Non cambiare `ARSPD_USE=0`.

### P1 — Rendere operativo il backbone dati

1. Formalizzare Display/Sensor Data Protocol V2.
2. Implementare adapter MAVLink in sola lettura.
3. Pubblicare snapshot normalizzato verso EFIS core.
4. Implementare heartbeat/stale/lost.
5. Integrare blackbox recorder con segmenti e replay.
6. Collegare GNSS, attitude, baro e airspeed monitor-only al banco prova UI.

### P2 — Chiudere il PFD primario

1. Correggere definitivamente nastro ALT e sovrapposizioni.
2. Aumentare realmente del 30% i gauge EMS con test a 1024×600.
3. Rifinire IAS, Vx/Vy, Vne e bande.
4. Validare HSI superiore/inferiore.
5. Verificare touch target e apertura NAV.
6. Produrre screenshot RPi5 e confronto con reference target.
7. Testare FPS, frametime, memoria e temperatura per almeno due ore.

### P3 — Terreno e navigazione

1. Tile streaming lungo LIPH–LIDB.
2. Pianura ad alta definizione e crossfade.
3. Ostacoli reali con provenance.
4. Piste/aeroporti e projection gating.
5. TAWS dinamico con test scenario.
6. Route, direct-to, alternate e database update.

### P4 — EMS reale

1. Adapter ESP32-S3/CAN.
2. Channel map reale.
3. Calibrazione sensori.
4. Limiti verificati da manuale motore.
5. Alarm manager con latch/acknowledge/escalation.
6. Registrazione blackbox.

### P5 — Meteo, FPV e distribuzione

1. Edge weather blackbox.
2. METAR/TAF/radar con freshness.
3. cache Nord Italia e corridor packs.
4. pipeline video hardware.
5. quattro telecamere e overlay.
6. gimbal soltanto dopo requisiti e prototipo da banco.

---

## 21. Primo ciclo di lavoro richiesto a Claude Code

Claude Code dovrebbe procedere in questo ordine:

1. Leggere integralmente questo handoff e i documenti citati.
2. Ispezionare la struttura reale senza assumere che ogni cattura corrisponda al codice corrente.
3. Eseguire test/build locali e sul RPi5, documentando gli esiti.
4. Eseguire un audit in sola lettura dei quattro nodi.
5. Creare un nuovo checkpoint con:
   - commit/hash o snapshot identificabile;
   - test pass/fail;
   - servizi attivi;
   - parametri Pixhawk rilevanti;
   - screenshot PFD/QGC;
   - problemi riproducibili.
6. Implementare prima l'adapter MAVLink read-only e il modello di health, senza cambiare parametri di volo.
7. Collegare i dati reali in modalità monitor-only al core e alla black box.
8. Solo dopo stabilizzare UI e terreno.

### Definition of Done per ogni modifica

- requisito e rischio dichiarati;
- test automatico o procedura ripetibile;
- build Windows se applicabile;
- build RPi5;
- screenshot per modifiche UI;
- nessuna regressione a 1024×600;
- stato simulato/reale evidente;
- log senza segreti;
- documentazione aggiornata;
- rollback possibile.

---

## 22. Cose da non fare

- Non modificare i progetti legacy.
- Non rifare da zero componenti già presenti senza prima analizzarli.
- Non trattare le immagini di output come prova di implementazione corrente.
- Non attivare il Matek nel flight control.
- Non calibrare sensori con dati simulati.
- Non memorizzare token Copernicus o password imagery.
- Non dipendere da Internet per le funzioni primarie.
- Non usare un singolo rendering gigante non adattabile.
- Non mettere logica avionica critica dentro QML.
- Non mascherare errori con valori plausibili di fallback.
- Non disabilitare gli allarmi per ottenere una schermata “verde”.
- Non lanciare QGC e MAVProxy sulla stessa seriale senza routing intenzionale.
- Non cancellare backup PX4/ArduPlane, cache funzionanti o volumi server esistenti.

---

## 23. Riferimenti da leggere per primi

Ordine consigliato:

1. `README.md`
2. `CHECKPOINT-2026-08-18.md`
3. `docs/adr/ADR-001-language-and-build.md`
4. `docs/adr/ADR-002-modular-monolith.md`
5. `docs/adr/ADR-005-source-arbitration.md`
6. `docs/protocols/DISPLAY_DATA_PROTOCOL_V2.md`
7. `docs/design/EFIS_UI_DESIGN_SYSTEM.md`
8. `docs/design/PFD_FOCUS_1024_SPEC.md`
9. `docs/design/TERRAIN_NAV_DATABASE_V1.md`
10. `docs/design/IMAGERY_SELECTION_AND_TRANSFER_V2.md`
11. `docs/design/WEATHER_RADAR_CONNECTIVITY_V1.md`
12. `docs/design/EFIS_CAM_V1.md`
13. `server/edge-node/README.md`
14. `docs/deployment/RPI5_PFD.md`
15. `docs/deployment/IMAGERY_SERVER.md`

---

## 24. Stato finale sintetico

### Fatto e verificato

- nuovo progetto separato dai legacy;
- toolchain Windows e RPi5;
- Qt/QML PFD funzionante sul display 1024×600;
- Setup Center e cataloghi;
- EMS simulation e molte viste;
- database navigazione e prototipi NAV;
- Copernicus DEM Veneto acquisito e validato a livello development;
- texture Sentinel-2 e pipeline terrain;
- server imagery e cache rotta Treviso–Belluno;
- nodo blackbox con NVMe e CAN disponibile;
- Pixhawk 6X flashato con ArduPlane 4.7.0 e backup eseguiti;
- M10 comunicante su GPS2, fix non ancora ottenuto;
- Matek DroneCAN rilevato e testato con ventilatore, monitor-only;
- MAVProxy e QGroundControl installati sul portatile;
- sospensione portatile disabilitata.

### Parzialmente completato

- qualità finale PFD e proporzioni strumenti;
- terreno 3D ad alta definizione e pianura;
- TAWS e ostacoli;
- navigation control center;
- meteo/METAR/TAF/radar;
- blackbox edge services;
- imagery Nord Italia;
- EMS reale;
- FPV e multi-camera.

### Non ancora completato

- adapter MAVLink EFIS production-grade;
- bus dati distribuito normalizzato;
- calibrazioni complete Pixhawk;
- fix GNSS 3D;
- power module e battery monitor;
- RC/servo/actuator integration;
- calibrazione magnetometro/accelerometri;
- test pitot installato e abilitazione airspeed al controllo;
- prove termiche e durata;
- flight test;
- processo di safety/certificazione.

---

## 25. Nota conclusiva per Claude Code

La priorità non è aggiungere rapidamente nuove schermate, ma trasformare il prototipo già ricco in un sistema coerente, misurabile e sicuro. Conservare le capacità sviluppate — PFD, EMS, terreno, navigazione, meteo, FPV, setup e blackbox — ma farle convergere su un unico modello dati, con provenienza, salute, degradazione, logging e test.

Prima di ogni intervento hardware, distinguere sempre:

1. osservazione in sola lettura;
2. configurazione reversibile;
3. calibrazione;
4. attivazione nel controllo di volo;
5. prova a terra;
6. prova in volo.

Il sistema attuale è un banco di sviluppo avanzato, non ancora un prodotto pronto al volo. La direzione concordata è corretta: una piattaforma EFIS NEXT professionale, distribuita, adattabile e indipendente dai due sistemi legacy.
