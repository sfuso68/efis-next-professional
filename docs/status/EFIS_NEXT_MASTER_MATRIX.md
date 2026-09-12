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
| CORE-004 | Health model nodo/sorgente/dato | Black box/Core | PARTIAL-HW | heartbeat, stale/lost e recovery event implementati; 16/16 test MSVC e 16/16 ARM; broker `e916548` attivo e post-check HW senza errori | recovery event non ancora osservato dopo il deployment; health Pixhawk/DroneCAN incompleto | ripetere fault fisico e leggere evento recorder, poi consolidare stati | sequenza STALE/LOST/ONLINE registrata e fault injection completa |
| PFD-001 | PFD primario 1024x600 | RPi5 | PARTIAL | release immutabile 23cd411 attiva; 313/313 build e 18/18 test ARM64; workflow profilo raggiungibile; DDP LIVE 20 Hz con zero reject/gap/errori; telemetria corretta e QML shutdown senza errori; loader 1d25e62 misurato con successo su secondo RPi5 | trasferimento 1d25e62 a `.45` attende autorizzazione esplicita; nuova interazione non ancora provata sul vetro; mancano touch acceptance e 60 min | installare 1d25e62 con rollback, verificare pagine/workflow touch e soak | acceptance visuale + 60 min runtime |
| PFD-002 | Nastri IAS/ALT professionali | RPi5 | PARTIAL | componenti presenti | overlap/leggibilità | rifinitura e catture | nessuna sovrapposizione, scale corrette |
| PFD-003 | HSI/FMA/heading e annunci | RPi5 | PARTIAL | componenti presenti | validità sorgenti NAV | collegare modello nav canonico | annunci coerenti in failover |
| MFD-001 | Secondo display MFD/SV | RPi5 `pi5` | PARTIAL | ADR-013 fissa MFD e applicazioni generali su RPi5; release 1d25e62 compilata 313/313 e 18/18 test PASS e selezionata nello staging atomico; PFD EGLFS passa da 13,6 a 34,5 snapshot/s e NAV arriva a 49,6 snapshot/s e 50,6 presentazioni/s; catture exit 0, zero errori QML/DDP, `throttled=0x0` | renderer legacy resta attivo al boot; touch/terrain e soak alimentazione senza acceptance | misurare le altre pagine e completare interazione/soak prima del boot definitivo | boot Next, DDP/terrain/setup stabili e acceptance su display reale |
| INST-001 | Framework strumenti analogici | RPi5/display secondari | DONE-SW | 29 SVG, six-pack, 10/10 test | verifica display post-router | test su RPi5 reale | catture HW + fail-state verificati |
| INST-002 | PanelStore atomico con backup/rollback | Core | DONE | test persistenza | nessuno noto | mantenere schema versionato | attivazione/rollback testati su target |
| PIX-001 | Pixhawk 6X come primary flight source | Pixhawk/Black box | PARTIAL | stream reale; attitude/alt/VS `pixhawk_primary`, non simulati | IAS resta ESP32; GNSS non disponibile; P2 non congelata | completare Matek/M10 e health | mapping segnali+health+timeout HW |
| PIX-002 | MAVLink read-only verso FlightSourceRouter | Black box | PARTIAL-HW | bridge O_RDONLY locale; backup finale 1084/1084; MAVLink2 unicast verificato; distacco/riconnessione fisici hanno fermato/ripreso MAVLink mentre broker, recorder, DDP e PFD restavano operativi; broker corretto `e916548` compilato ARM e installato | recovery event post-fix non ancora osservato su hardware; endurance e failover prolungato aperti; pre-arm richiede calibrazioni | ripetere fault fisico dopo il fix, leggere recovery event e fare soak >=60 min | mapping, stale/lost/online registrati e latenza verificati su HW |
| PIX-003 | Comandi autopilota | Pixhawk | TODO | fuori scope P2 | safety elevata | fase separata dopo read-only | specifica, interlock, test HIL |
| PAN-001 | Emergency Assist consultivo | PFD/Core/Black box | PARTIAL | core e UI 1024x600 implementati; detector multi-segnale gated, planata/ranking fail-closed, otto famiglie, eventi locali e suite test core; build/QML/offscreen PASS | manca hardware PFD, input fisico, audio, AGL/terrain/vento validi e sink Blackbox centrale | acceptance RPi5 e protocollo eventi con ack di persistenza | scenari/replay verificati, advice tracciabile e nessuna autorità di volo |
| PAN-002 | Motore checklist emergenza versionato | Core/Setup | BLOCKED | package ID/revisione/fonte/hash e workflow step/skip/repeat implementati; package incompleto rifiutato; solo test vector SIM | manca POH controllato dell'esemplare, revisione/applicabilità, seriale motore e manuale elica | acquisire scansioni proprietario e produrre diff campo-per-campo | ogni istruzione collegata a fonte/revisione e test scenario |
| PAN-003 | Stabilize Assist con servi rollio/beccheggio | Pixhawk/Aereo | BLOCKED | due servi presenti nel registro; uno solo identificato parzialmente | pinout/drive/coppia/meccanica/AP disconnect/HIL non verificati; P2 read-only aperta | identificare entrambi i servi e qualificarli al banco | interlock, override, fault injection e prove progressive documentati |
| PAN-004 | Emergency Glide automatico / Autoland | Futuro | TODO | esplicitamente fuori scope V1 | soli roll/pitch insufficienti; architettura e qualifica non definite | rivalutare solo dopo PAN-003 e attuazione completa | programma HIL/flight-test e safety case approvati |
| GNSS-001 | M10 comunicante | Pixhawk GPS2 | PARTIAL-HW | ricezione presente | fix outdoor non chiuso | test cielo aperto | fix 3D, sat, accuracy, log |
| GNSS-002 | Magnetometro esterno M10 | Pixhawk | PARTIAL-HW | device rilevato | orientamento/interferenze | calibrazione installata | compass consistency test |
| AIR-001 | Matek ASPD-DLVR su DroneCAN | Pixhawk CAN1 | PARTIAL-HW | nodo rilevato, monitor-only | calibrazione/offset | banco pitot/statico | zero/calibrazione/linearità verificati |
| AIR-002 | Airspeed nel core arbitrato | Core | PARTIAL | predisposizione sorgenti | non usare GS come IAS | integrare DroneCAN/Pixhawk/legacy | failover e stale testati |
| AIR-003 | AoA | Professional | TODO | requisito prodotto | sensore/algoritmo da scegliere | specifica AoA | validazione banco+volo |
| ESP-001 | ESP32-S3 legacy adapter | Black box/CAN | PARTIAL | adapter + test | ruolo da ridurre | usarlo come aux/fallback | dati reali arbitrati senza conflitti |
| ESP-002 | ESP32 come I/O ausiliario | Aereo | TODO | concetto definito | hardware/protocollo | definire discrete/OAT/flap/trim | specifica + nodo testato |
| EMS-001 | Simulazione motore | RPi5/Core | DONE | UI e test | non confondere con reale | mantenere SIM marcato | sempre riconoscibile come SIM |
| EMS-002 | Nodo EMS software | RPi4 dedicato/ESP32/Black box | PARTIAL | ADR-006/013 fissano EMS su RPi4 separato dal VIDEO-CORE; EDP V1/49003 installato sul banco; Blackbox→EMS LIVE, arresto broker produce STALE→LOST→fallback e recovery LIVE; feed marcato SIM; workflow Setup compilato nel target EMS | sorgente Blackbox ancora simulata; sensori reali incompleti; workflow non verificato sul touch RPi4 | verificare pagine Setup sul pannello e collegare decoder/arbitraggio CAN reale | canali calibrati, provenienza, health e workflow touch verificati |
| EMS-003 | DroneCAN engine/fuel/power | CAN | PARTIAL | ADR-007, decoder/test | carico bus/timing | prova bus reale multi-nodo | bus load + recovery + loss test |
| EMS-004 | Limiti motore da manuale | Setup/EMS | TODO | 912 UL 80 hp riscontrato sul sito Rotax; libreria OM-912 identificata; audit rileva limiti legacy non affidabili | manca seriale e revisione OM-912 applicabile; profilo contiene valori indicativi/anomali | acquisire targhetta e OM-912, poi sostituire limiti con riferimenti di pagina | POH/manuale citato e approvato |
| CFG-001 | Catalogo operativo completo modello/variante/esemplare | Setup/Core | PARTIAL | dizionario V2 con 447 parametri/28 domini e tre profili separati; workflow condiviso PFD/EMS; import fail-closed e prefill sempre SIM; 12/12 test Python, test modello Qt e 17/17 ARM64 PASS; release corretta 74a85f9 attiva sul PFD senza errori QML | l'esemplare reale conserva 309 blocker; mancano fonti controllate, review, firma/release/rollback e verifica touch delle pagine su PFD/EMS | provare workflow touch sul PFD, poi installare su EMS e acquisire documenti applicabili | profilo reale con 447/447 campi risolti o motivati, fonti a pagina, release firmata e caricamento runtime verificato |
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
| FPV-001 | Vista FPV | RPi4 VIDEO-CORE dedicato + viewer RPi5 | PARTIAL | ADR-013 assegna acquisizione/decode/mosaico FPV a un RPi4 distinto dall'EMS; i viewer PFD/MFD RPi5 consumano soltanto il flusso selezionato e health | pipeline reale e protocollo VIDEO-CORE→viewer non implementati | definire processo GStreamer/V4L2 HW e contratto stream/health senza bridge fra VIDEO e FLIGHT-DATA | latenza e freeze detection verificate senza impatto EMS/PFD |
| FPV-002 | Quattro telecamere | RPi4 VIDEO-CORE dedicato | TODO | requisito e allocazione RPi4 definiti; isolamento fisico EMS/FPV imposto dal validatore del piano | hardware RPi4 video e switch VIDEO non ancora identificati; banda/decoder/power aperti | selezionare RPi4 e switch VIDEO, poi provare quattro ingressi e mosaico | 4 feed, PiP, failover e fault isolation verificati |
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
| PROD-005 | Update/rollback software | Tutti | PARTIAL | release immutabile 23cd411 attiva su `.45`, con 9c5c0f1/74a85f9 conservate; 1d25e62 staged e misurata su `.49`, legacy attivo e rollback renderer verificato; ADR-013 definisce i profili RPi4/RPi5 | deployment ancora non uniforme; egress 1d25e62 verso `.45` attende autorizzazione esplicita; `.49` richiede touch e soak | uniformare `.45` su 1d25e62 e standardizzare attivazione/rollback per profilo | rollback simulato su tutti i nodi e nessuna promozione con gate HW aperto |
| VER-001 | Test automatici core | Dev/RPi5 | DONE/PARTIAL | suite multiple + 16/16 CTest PASS su release ARM64 12b4697 (`.45`) e ab0429e (`.49`) + EFIS Core CI Linux PASS su PR #6 | estendere piattaforme e fault test | mantenere CI green | green required per PR |
| VER-002 | HIL | Banco | TODO | concetto | hardware fixture | costruire banco HIL | fault injection ripetibile |
| VER-003 | Test termico prolungato RPi5 | RPi5 | TODO | `.45` senza throttle nella prova breve; `.49` ha avuto uno storico `0x50000` durante una build, ma le due prove grafiche successive sono terminate a `0x0`, 53,2–53,8 °C | PSU/cavo del secondo nodo non qualificati con soak; le prove brevi non chiudono il gate | eseguire reboot e soak test sotto carico combinato | nessun throttle/undervoltage |
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
