# EFIS NEXT — Master Requirements & Status Matrix

**Ruolo:** registro operativo del progetto / controllo avanzamento  
**Fonte sintetica collegata:** `docs/status/EFIS_NEXT_STATE.md`  
**Baseline tecnica:** `work/codex-multisource-p1`

**Stato progetto:** DEVELOPMENT / NON-CERT  

## Regole

Stati ammessi: `DONE`, `PARTIAL`, `TODO`, `BLOCKED`, `VERIFY-HW`, `LEGACY`.

Una voce può passare a `DONE` solo se esiste una evidenza verificabile: test automatico, log, cattura da hardware reale, misura, checklist o documento di acceptance. Il solo fatto che il codice compili non basta.

Ogni modifica a un sottosistema deve aggiornare nello stesso commit/PR almeno una riga di questa matrice oppure motivare esplicitamente perché lo stato non cambia.

## Matrice principale

| ID | Area / requisito | Nodo | Stato | Evidenza attuale | Dipendenze / rischio | Prossimo passo | Criterio di chiusura |
|---|---|---|---|---|---|---|---|
| CORE-001 | Core C++20 indipendente da Qt | Tutti | DONE | build/test multipiattaforma | mantenere confini | preservare nelle nuove feature | nessuna dipendenza UI nel dominio |
| CORE-002 | Snapshot, provenance, timestamp, quality, validity | Black box/Core | DONE | test core/arbitration | regressioni dati plausibili ma falsi | estendere nuovi segnali | copertura test per ogni SignalId |
| CORE-003 | Arbitraggio per grandezza e failover | Black box/Core | DONE | `SnapshotArbitrator`, test | tuning priorità reali | integrare sorgenti P2/P4 | failover/reversione verificati su HW |
| CORE-004 | Health model nodo/sorgente/dato | Black box/Core | PARTIAL | heartbeat, stale, lost node | health Pixhawk/DroneCAN incompleto | consolidare stati OK/DEGRADED/STALE/FAILED | fault injection completa |
| PFD-001 | PFD primario 1024x600 | RPi5 | PARTIAL | release immutabile 12b4697 attiva su RPi5; DDP LIVE senza reject/gap; imagery OFFLINE_READY; ~23-24 FPS come baseline precedente | qualità grafica non definitiva; manca acceptance visuale/60 min | revisione layout su vetro e soak | acceptance visuale + 60 min runtime |
| PFD-002 | Nastri IAS/ALT professionali | RPi5 | PARTIAL | componenti presenti | overlap/leggibilità | rifinitura e catture | nessuna sovrapposizione, scale corrette |
| PFD-003 | HSI/FMA/heading e annunci | RPi5 | PARTIAL | componenti presenti | validità sorgenti NAV | collegare modello nav canonico | annunci coerenti in failover |
| MFD-001 | Secondo display MFD/SV | RPi5 `pi5` | PARTIAL | FLIGHT-DATA `10.77.10.13`, broker a due display e DDP LIVE con 579 snapshot, zero reject/gap/errori; release 3458552 e imagery OFFLINE_READY verificati in prova breve | ~14 FPS, power flag `0x50000`, terrain senza acceptance visiva | correggere PSU, profilare renderer e ripetere test con cattura/soak | boot Next, DDP/terrain stabili e acceptance su display reale |
| INST-001 | Framework strumenti analogici | RPi5/display secondari | DONE-SW | 29 SVG, six-pack, 10/10 test | verifica display post-router | test su RPi5 reale | catture HW + fail-state verificati |
| INST-002 | PanelStore atomico con backup/rollback | Core | DONE | test persistenza | nessuno noto | mantenere schema versionato | attivazione/rollback testati su target |
| PIX-001 | Pixhawk 6X come primary flight source | Pixhawk/Black box | PARTIAL | stream reale; attitude/alt/VS `pixhawk_primary`, non simulati | IAS resta ESP32; GNSS non disponibile; P2 non congelata | completare Matek/M10 e health | mapping segnali+health+timeout HW |
| PIX-002 | MAVLink read-only verso FlightSourceRouter | Black box | PARTIAL | bridge O_RDONLY HW + recorder con provenance Pixhawk; backup pre-Ethernet completo 1050/1050 e `NET_ENABLE` 0→1 verificato | riavvio e parametri NET restanti non ancora applicati; priorità/fault test da consolidare | power-cycle, configurare UDP client unicast e confrontare parametri | mapping, stale e failover verificati su HW |
| PIX-003 | Comandi autopilota | Pixhawk | TODO | fuori scope P2 | safety elevata | fase separata dopo read-only | specifica, interlock, test HIL |
| GNSS-001 | M10 comunicante | Pixhawk GPS2 | PARTIAL-HW | ricezione presente | fix outdoor non chiuso | test cielo aperto | fix 3D, sat, accuracy, log |
| GNSS-002 | Magnetometro esterno M10 | Pixhawk | PARTIAL-HW | device rilevato | orientamento/interferenze | calibrazione installata | compass consistency test |
| AIR-001 | Matek ASPD-DLVR su DroneCAN | Pixhawk CAN1 | PARTIAL-HW | nodo rilevato, monitor-only | calibrazione/offset | banco pitot/statico | zero/calibrazione/linearità verificati |
| AIR-002 | Airspeed nel core arbitrato | Core | PARTIAL | predisposizione sorgenti | non usare GS come IAS | integrare DroneCAN/Pixhawk/legacy | failover e stale testati |
| AIR-003 | AoA | Professional | TODO | requisito prodotto | sensore/algoritmo da scegliere | specifica AoA | validazione banco+volo |
| ESP-001 | ESP32-S3 legacy adapter | Black box/CAN | PARTIAL | adapter + test | ruolo da ridurre | usarlo come aux/fallback | dati reali arbitrati senza conflitti |
| ESP-002 | ESP32 come I/O ausiliario | Aereo | TODO | concetto definito | hardware/protocollo | definire discrete/OAT/flap/trim | specifica + nodo testato |
| EMS-001 | Simulazione motore | RPi5/Core | DONE | UI e test | non confondere con reale | mantenere SIM marcato | sempre riconoscibile come SIM |
| EMS-002 | Nodo EMS software | RPi4/ESP32/Black box | PARTIAL | EDP V1 installato sul banco; Blackbox→EMS LIVE, arresto broker produce STALE→LOST→fallback e recovery LIVE; feed marcato SIM | sorgente Blackbox ancora simulata; sensori reali incompleti | collegare decoder/arbitraggio CAN reale e ripetere fault/endurance | canali calibrati, provenienza e health verificati |
| EMS-003 | DroneCAN engine/fuel/power | CAN | PARTIAL | ADR-007, decoder/test | carico bus/timing | prova bus reale multi-nodo | bus load + recovery + loss test |
| EMS-004 | Limiti motore da manuale | Setup/EMS | TODO | campi presenti | serve fonte esterna | verificare per motore installato | POH/manuale citato e approvato |
| BBX-001 | Black box append-only | Black box | DONE/PARTIAL | recorder e ingest | retention/robustezza da chiudere | endurance test | power-loss recovery + retention |
| BBX-002 | Ingestion unico dei bus | Black box | DONE-ARCH | ADR-003 | deployment finale | consolidare distribuzione | nessun display consuma frame grezzi |
| BBX-003 | Broker stato normalizzato ai display | Black box | PARTIAL | DDP V2 al PFD; EDP V1/49003 installato verso EMS; publisher 20 Hz senza failure e prova STALE/LOST/recovery riuscita; feed esplicitamente SIM | decoder CAN motore non collegato a EDP; reboot/Pixhawk/endurance aperti | integrare sorgente CAN reale e prova reboot/endurance | protocollo versionato + replay + perdita/reordine HW multi-display |
| BBX-004 | Edge weather/cache | Black box | PARTIAL | servizio presente | freshness/offline | hardening | live/cached/stale verificati |
| SRV-001 | Server mappe/imagery | Server casa | PARTIAL | mapapp/Caddy/status | lifecycle pacchetti | versioning+manifest | update atomico + rollback |
| SRV-002 | Cataloghi/navdata centralizzati | Server casa | PARTIAL | DB/cataloghi presenti | validità/fonti | pipeline update | checksum/provenance/versione |
| SRV-003 | Distribuzione corridor pack | Server->PFD | PARTIAL | 1326 tile cache verificati | resume/quota/GC | chiudere lifecycle | perdita rete senza perdita pack attivo |
| NAV-001 | Database aeroporti/navaid/airspace/reporting | RPi5/Server | PARTIAL/DONE-DATA | DB e importer | ciclo aggiornamento | validazione fonti | dataset versionato e controllato |
| NAV-002 | Modello unico FlightPlan/leg/WP | Core | PARTIAL | route/prototipi | sorgenti NAV multiple | consolidare modello canonico | direct-to/nearest/route test |
| NAV-003 | CDI/OBS/BRG/DTK/TRK | RPi5/Core | PARTIAL | prototipi | annunci validità | integrare con model | scenari GPS/VOR/NDB e loss test |
| SV-001 | Synthetic Vision | RPi5 | PARTIAL | GLES storico attivo su `pi5` `.49`; Qt3D Next ARM64 usa root dati portabili, asset GPU/texture copiati con hash uguali e manifesto da 1326 tile caricato | due linee tecniche; ~14 FPS e nessuna acceptance visiva terrain sul secondo nodo | profilare UI/render loop e acquisire cattura hardware | prestazioni e qualità target |
| SV-002 | DEM Copernicus/SRTM | RPi5/Server | PARTIAL | pacchetto Veneto verificato | copertura/provenance | estendere Nord Italia | seam/quote/checksum verificati |
| SV-003 | TAWS | RPi5/Core | PARTIAL/EXPERIMENTAL | shader/envelope prototipale | safety critica | definire envelope | test indipendenti, no terrain-safe su missing |
| WX-001 | METAR/TAF | Black box/RPi5 | PARTIAL | parser/edge service | provider/licenze/freshness | registry e timestamp separati | raw+parsed+stale test |
| WX-002 | Radar meteo | RPi5/Server | PARTIAL | overlay presente | georef/provider | verifica georeferenziazione | LIVE/CACHED/STALE/SIM corretti |
| FPV-001 | Vista FPV | RPi5/RPi4 | PARTIAL | QML/design | pipeline reale | GStreamer/V4L2 HW | latenza e freeze detection |
| FPV-002 | Quattro telecamere | Video node | TODO | requisito definito | banda/decoder/power | architettura ingressi | 4 feed, PiP, failover |
| FPV-003 | Registrazione video sincronizzata | Black box | TODO | requisito | storage/timestamp | specifica | sync telemetria verificata |
| GS-001 | QGroundControl/bridge manutenzione | Portatile | PARTIAL | bridge O_RDONLY installato + runbook separazione QGC | `Linger=no`, recovery e confronto parametri aperti | test USB fault + recovery da backup | recovery ripetibile e nessuna scrittura involontaria |
| GS-002 | Backup firmware/parametri Pixhawk | Portatile | DONE | backup PX4/ArduPlane | proteggere copie | replica verificata | doppia copia + hash |
| GS-003 | Tooling sviluppo Windows | Portatile | PARTIAL | build MSVC/Qt | Smart App Control | documentare workaround | build/test riproducibili |
| LEG-R4-001 | EFIS RPi4 storico | RPi4 | LEGACY | operativo | non regredire | congelare | nessuna modifica salvo fix critici |
| LEG-R5-001 | `sv3d-fondamenta` | RPi5 | LEGACY/REFERENCE | attivo su `pi5` WLAN `192.168.1.49`; rollback verificato dopo due prove Qt/EGLFS | divergenza storica; contesa DRM | mantenere fino ad acceptance EFIS NEXT sul nodo | nessuna dipendenza nascosta |
| PROD-001 | Profili Standard/Professional | Tutti | TODO/DESIGN | concetto definito | hardware feature matrix | congelare SKU/features | matrice prodotto approvata |
| PROD-002 | Alimentazione avionica | Aereo | TODO | requisito | safety/hardware | progettare PSU/protezioni | brownout/EMI/load test |
| PROD-003 | Enclosure e termica | Aereo | TODO | prototipi HW | vibrazioni/temperatura | progetto meccanico | soak/vibration/thermal test |
| PROD-004 | Harness/connettori | Aereo | PARTIAL | Waveshare 5P identificato; schema FLIGHT-DATA V1; video separato | 5 porte insufficienti per finale; cavi/EMI aperti | cablare banco e selezionare switch video/finale | harness documentato e testato |
| PROD-005 | Update/rollback software | Tutti | PARTIAL | release immutabili su `.45`; release separata e rollback renderer reale verificato su `.49` | deployment non uniforme tra nodi | standardizzare attivazione atomica e rollback | rollback simulato su tutti i nodi |
| VER-001 | Test automatici core | Dev/RPi5 | DONE/PARTIAL | suite multiple + 16/16 CTest PASS su release ARM64 12b4697 (`.45`) e ab0429e (`.49`) + EFIS Core CI Linux PASS su PR #6 | estendere piattaforme e fault test | mantenere CI green | green required per PR |
| VER-002 | HIL | Banco | TODO | concetto | hardware fixture | costruire banco HIL | fault injection ripetibile |
| VER-003 | Test termico prolungato RPi5 | RPi5 | TODO | `.45` senza throttle nella prova breve; `.49` ripartito da `0x0` dopo reboot ma tornato a `0x50000` durante build, 54,9 °C nel test display | PSU/cavo del secondo nodo ancora insufficiente sotto carico; misura FPS non conclusiva | correggere alimentazione, reboot e soak test | nessun throttle/undervoltage |
| VER-004 | Test vibrazioni/EMI | Sistema | TODO | nessuna chiusura | hardware finale | piano di prova | limiti definiti e report |
| VER-005 | Acceptance bench/flight | Sistema | TODO | non ancora | precedenti gate | checklist incrementale | tutte le funzioni safety-critical verificate |

## Cruscotto di controllo

Le percentuali NON si aggiornano a mano. Devono essere derivate dalla matrice con pesi espliciti. Fino all'automazione, usare solo conteggi per stato.

Comando/strumento futuro: `tools/project_status/generate_status.py`.

Output atteso:

- totale requisiti;
- DONE;
- PARTIAL;
- VERIFY-HW;
- TODO;
- BLOCKED;
- per nodo;
- per area;
- lista dei gate bloccanti.

## Regola per Claude Code / Codex

Prima di iniziare una milestone:
1. leggere `EFIS_NEXT_STATE.md` e questa matrice;
2. citare gli ID che verranno modificati;
3. lavorare su branch dedicato;
4. eseguire i test pertinenti;
5. aggiornare stato/evidenza/prossimo passo degli stessi ID;
6. non impostare `DONE` senza criterio di chiusura soddisfatto.

## Gate professionali principali

1. **GATE DATA:** Pixhawk + DroneCAN + ESP32 arbitrati e health completi.
2. **GATE DISPLAY:** PFD/strumenti verificati sul display reale senza regressioni.
3. **GATE AIRDATA:** GNSS, baro, airspeed e failover verificati su hardware.
4. **GATE EMS:** nodo motore completo, calibrato e con limiti verificati.
5. **GATE DISTRIBUTED:** black box broker + DDP V2 + display consumer.
6. **GATE SV/TAWS:** dati terrain, performance e failure modes verificati.
7. **GATE PRODUCT:** PSU, enclosure, harness, watchdog, update/rollback.
8. **GATE HIL:** fault injection end-to-end ripetibile.
9. **GATE FLIGHT:** piano di prove progressivo e acceptance documentata.

Questo file è un registro operativo, non una dichiarazione di certificazione o idoneità al volo.
