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
| PRJ-001 | Master Brief e indice documentale AVIONIX NEXT | Governance | PARTIAL | Brief R0 importato integralmente dal repository storico; indice di autorità, nove conflitti e backlog G0–G7 aggiunti su branch `docs/avionix-master-baseline-r0` | cinque riferimenti interni non reperiti; naming AVIONIX/EFIS non congelato | recuperare i documenti e approvare naming/brief R1 | fonti indicizzate con hash, conflitti chiusi o accettati e brief revisionato |
| PRJ-002 | Gate G0 — verità del progetto | Tutti | PARTIAL | audit GitHub/NAS e verifica read-only di PFD, MFD, EMS, Blackbox, server, portatile, NAS e Pixhawk eseguiti il 2026-09-16; `origin/main` e NAS `professional/main` allineati a `d9231f4` | nodo ASSO5/Sequent non identificato; bare NAS duplicato; branch SV storico divergente e senza genealogia con Professional | decidere repository NAS duplicato, identificare ASSO5 e completare indice hardware | nessun lavoro non localizzato, inventario approvato e conflitti documentali risolti |
| CORE-001 | Core C++20 indipendente da Qt | Tutti | DONE | build/test multipiattaforma | mantenere confini | preservare nelle nuove feature | nessuna dipendenza UI nel dominio |
| CORE-002 | Snapshot, provenance, timestamp, quality, validity | Black box/Core | DONE | test core/arbitration | regressioni dati plausibili ma falsi | estendere nuovi segnali | copertura test per ogni SignalId |
| CORE-003 | Arbitraggio per grandezza e failover | Black box/Core | DONE | `SnapshotArbitrator`, test | tuning priorità reali | integrare sorgenti P2/P4 | failover/reversione verificati su HW |
| CORE-004 | Health model nodo/sorgente/dato | Black box/Core | PARTIAL-HW | heartbeat, stale/lost e recovery event implementati; 16/16 test MSVC e 16/16 ARM; broker `e916548` attivo e post-check HW senza errori | recovery event non ancora osservato dopo il deployment; health Pixhawk/DroneCAN incompleto | ripetere fault fisico e leggere evento recorder, poi consolidare stati | sequenza STALE/LOST/ONLINE registrata e fault injection completa |
| PFD-001 | PFD primario 1024x600 | RPi5 | PARTIAL | release e902973 attiva; build 313/313 e 18/18 CTest ARM64 PASS; due reboot reali con DDP/kernel zero gap; nella prova pulita 112 s nessun callback stall, fuel file invariato e radar simulato su tmpfs; 33–37 presentazioni/s, 54,3 °C, no throttle; sul branch `safety/pfd-validity-gates-v1` SVG e Qt/QML applicano gate fail-closed ad assetto, prua, SVS, guidance e selezioni, senza AP/trend inventati; build Qt/QML e test core Windows PASS | i gate nuovi sono verificati solo software e non distribuiti né verificati sul display reale; restano 958 ms iniziali e un evento generico da 590 ms; un evento da 12607 ms coincidente con SSH non è riprodotto; mancano touch acceptance e verifica visuale pagine/terrain | dopo review/merge, distribuire su banco e verificare perdita/ripristino delle sorgenti reali prima di promuovere l'evidenza hardware | acceptance visuale + fault injection dati su RPi5, avvio senza stall né nuovi gap |
| PFD-002 | Nastri IAS/ALT professionali | RPi5 | PARTIAL | release MFD `3432402`: cattura EGLFS reale 1024x600 mostra fascia ALT/VS separata e valori distinti; unita' KT/FT visibili | correzione non ancora accettata dall'operatore sul display; scale e leggibilita' non qualificate in tutte le condizioni | acceptance visiva operatore e scenari limiti/invalid/stale | nessuna sovrapposizione, scale corrette |
| PFD-003 | HSI/FMA/heading e annunci | RPi5 | PARTIAL | componenti presenti | validità sorgenti NAV | collegare modello nav canonico | annunci coerenti in failover |
| MFD-001 | Secondo display MFD/SV | RPi5 `pi5` | PARTIAL | release `3432402` attiva in EGLFS/Canvas; Setup reale identifica `RPi5 • MFD-01` e `MULTIFUNCTION DISPLAY`; stato broker remoto deriva dal link DDP senza dichiarare registrazione; PFD/Setup/Emergency catturati; baseline `f27508a` ha superato soak 60/60, 56,0–60,9 °C, zero restart/throttle e DDP senza errori | touch USB assente; nuova release non ha ancora soak 60 min; EMS MFD non riceve EDP per scelta architetturale; terrain vicino resta sfocato/stirato e non accettato | collegare touch, definire stato EMS sul MFD, correggere LOD/proiezione terrain e ripetere soak worst-case sulla release corrente | boot Next, DDP/terrain/setup stabili e acceptance su display reale |
| INST-001 | Framework strumenti analogici | RPi5/display secondari | DONE-SW | 29 SVG, six-pack, 10/10 test | verifica display post-router | test su RPi5 reale | catture HW + fail-state verificati |
| INST-002 | PanelStore atomico con backup/rollback | Core | DONE | test persistenza | nessuno noto | mantenere schema versionato | attivazione/rollback testati su target |
| PIX-001 | Pixhawk 6X come primary flight source | Pixhawk/Black box | PARTIAL | stream reale; attitude/alt/VS `pixhawk_primary`, non simulati | IAS resta ESP32; GNSS non disponibile; P2 non congelata | completare Matek/M10 e health | mapping segnali+health+timeout HW |
| PIX-002 | MAVLink read-only verso FlightSourceRouter | Black box | PARTIAL-HW | bridge O_RDONLY locale; backup finale 1084/1084; MAVLink2 unicast verificato; distacco/riconnessione fisici hanno fermato/ripreso MAVLink mentre broker, recorder, DDP e PFD restavano operativi; broker corretto `e916548` compilato ARM e installato | recovery event post-fix non ancora osservato su hardware; endurance e failover prolungato aperti; pre-arm richiede calibrazioni | ripetere fault fisico dopo il fix, leggere recovery event e fare soak >=60 min | mapping, stale/lost/online registrati e latenza verificati su HW |
| PIX-003 | Comandi autopilota | Pixhawk | TODO | fuori scope P2 | safety elevata | fase separata dopo read-only | specifica, interlock, test HIL |
| PAN-001 | Emergency Assist consultivo | PFD/Core/Black box | PARTIAL | release PFD `60b3526` attiva; build ARM 319 step e 20/20 CTest PASS; otto famiglie catturate 1024x600 su Wayland RPi5; nuova UI Flight Support implementata su branch `work/emergency-flight-support-ui-v1`: vista centrale assetto/SkyView, heading/pitch, IAS/ALT/VS, gerarchia FLY-NAVIGATE-COMMUNICATE/MANAGE e gate espliciti attitude/heading/position/VS; build Qt/MSVC e QML AOT PASS, `qmllint` pulito e 3/3 suite Emergency PASS; cattura offscreen preliminare 1024x600 generata prima dell'ultimo affinamento dei gate, mentre l'eseguibile finale non firmato è stato bloccato dal controllo applicazioni Windows; 24 transizioni UI + 2 eventi fault SIM persistiti con ack; arresto hub mantiene offset 51011 e recovery avanza a 51325/26 record; replay 26 record valido senza gap/regressioni; 9 test trasporto coprono anche SQLite full, mancato ack, recovery e uscita brusca con transazione incompleta; harness eseguito il 2026-09-14 su `efisblackbox`: ext4 loopback pieno 507/non persistito, health 503, recovery 201, due record conservati dopo SIGKILL, quick_check PASS, servizi production active e zero restart; evidenza in `docs/status/evidence/emergency-storage-fault-blackbox-20260914/`; input conditioner e audio arbiter testati, nessuna API comando | la nuova UI non ha ancora build/cattura/touch acceptance su RPi5; la cattura Windows preliminare non qualifica la versione finale, i font o la resa EGLFS/Wayland; il loopback non prova power-loss, inode/I/O fault o volume production; adapter pulsante/audio, replay sincronizzato bus/UI, health end-to-end FLIGHT-DATA, AGL/terrain/vento validati e procedure approvate restano aperti | distribuire la UI su RPi5 e provare touch/degrado dati; verificare health production dal PFD, poi inode/I/O/power-loss controllato, audio e correlazione bus senza superare ADR-012 | scenari/replay verificati, supporto assetto leggibile con fallback fail-closed, advice tracciabile, ack Blackbox resistente ai fault e nessuna autorità di volo |
| PAN-002 | Motore checklist emergenza versionato | Core/Setup | BLOCKED | package ID/revisione/fonte/hash e workflow step/skip/repeat implementati; package incompleto rifiutato; solo test vector SIM | manca POH controllato dell'esemplare, revisione/applicabilità, seriale motore e manuale elica | acquisire scansioni proprietario e produrre diff campo-per-campo | ogni istruzione collegata a fonte/revisione e test scenario |
| PAN-003 | Stabilize Assist con servi rollio/beccheggio | Pixhawk/Aereo | BLOCKED | due servi presenti nel registro; uno solo identificato parzialmente | pinout/drive/coppia/meccanica/AP disconnect/HIL non verificati; P2 read-only aperta | identificare entrambi i servi e qualificarli al banco | interlock, override, fault injection e prove progressive documentati |
| PAN-004 | Emergency Glide automatico / Autoland | Futuro | TODO | esplicitamente fuori scope V1 | soli roll/pitch insufficienti; architettura e qualifica non definite | rivalutare solo dopo PAN-003 e attuazione completa | programma HIL/flight-test e safety case approvati |
| VOI-001 | Voice Command Gate bilingue e acknowledgement | Black box/Core/PFD | PARTIAL | core C++20 con grammatica chiusa IT/EN e PTT/health/confidence fail-closed; adapter Vosk replay-only valida WAV e grammatica, usa confidenza minima, emette candidati SIM senza autorità; 7/7 test Python e test C++ dedicati PASS su Windows | Vosk/modello non installati sulla Blackbox; nessun audio reale, microfono, PTT fisico, protocollo rete o audit collegato; soglia non validata | eseguire corpus replay IT/EN su Blackbox misurando risorse e false accept/reject, poi definire protocollo autenticato mantenendo touch fallback | false accept/reject misurati su audio reale, fault device/rete verificati e nessuna autorità di comando |
| VOI-002 | Radio Phraseology Assist IT/EN | PFD/Black box/Server | PARTIAL | compositore deterministico blocca package/metadati/contesto incompleti; pagina Radio Assist 1024x600 e preview SIM compilate QML AOT | database operativo non acquisito/approvato; frequenze, AIP/NOTAM e fraseologia corrente non collegati; nessuna acceptance operatore | costruire registry versionato da fonti EASA/ENAC/ENAV e collegare dati con provenance/freshness | revisione competente, pacchetti firmati, scenari IT/EN e dati stale/missing verificati |
| VOI-003 | Catena audio, PTT e riproduzione cue | Black box/PFD/Cabina | TODO | `EmergencyAudioArbiter` e policy priorità esistenti; ADR-014 definisce l'allocazione candidata | interfaccia interfono, isolamento, ALSA/PipeWire, livelli, intelligibilità e priorità STALL/TAWS non provati | identificare interfono/connettori/PTT e costruire banco audio isolato | test rumore cabina, device-loss, latenza, intelligibilità, reboot e carico simultaneo |
| GNSS-001 | M10 comunicante | Pixhawk GPS2 | PARTIAL-HW | ricezione presente | fix outdoor non chiuso | test cielo aperto | fix 3D, sat, accuracy, log |
| GNSS-002 | Magnetometro esterno M10 | Pixhawk | PARTIAL-HW | device rilevato | orientamento/interferenze | calibrazione installata | compass consistency test |
| AIR-001 | Matek ASPD-DLVR su DroneCAN | Pixhawk CAN1 | PARTIAL-HW | nodo rilevato, monitor-only | calibrazione/offset | banco pitot/statico | zero/calibrazione/linearità verificati |
| AIR-002 | Airspeed nel core arbitrato | Core | PARTIAL | predisposizione sorgenti | non usare GS come IAS | integrare DroneCAN/Pixhawk/legacy | failover e stale testati |
| AIR-003 | AoA | Professional | TODO | requisito prodotto | sensore/algoritmo da scegliere | specifica AoA | validazione banco+volo |
| ESP-001 | ESP32-S3 legacy adapter | Black box/CAN | PARTIAL | adapter + test | ruolo da ridurre | usarlo come aux/fallback | dati reali arbitrati senza conflitti |
| ESP-002 | ESP32 come I/O ausiliario | Aereo | TODO | concetto definito | hardware/protocollo | definire discrete/OAT/flap/trim | specifica + nodo testato |
| EMS-001 | Simulazione motore | RPi5/Core | DONE | UI e test | non confondere con reale | mantenere SIM marcato | sempre riconoscibile come SIM |
| EMS-002 | Nodo EMS software | RPi4 dedicato/ESP32/Black box | PARTIAL | ADR-006/013 fissano EMS su RPi4 separato dal VIDEO-CORE; EDP V1/49003 installato sul banco; Blackbox→EMS LIVE, arresto broker produce STALE→LOST→fallback e recovery LIVE; feed marcato SIM; workflow Setup compilato nel target EMS; nodo reale attivo da oltre una settimana, gigabit/full duplex/EEE off, zero errori Ethernet e throttling 0 | `NRestarts=2` storico non attribuibile perché il journal è ruotato; sorgente Blackbox ancora simulata; sensori reali incompleti; workflow non verificato sul touch RPi4 | investigare restart alla prossima finestra; verificare Setup touch e collegare decoder/arbitraggio CAN reale | canali calibrati, provenienza, health e workflow touch verificati |
| EMS-003 | DroneCAN engine/fuel/power | CAN | PARTIAL | ADR-007, decoder/test | carico bus/timing | prova bus reale multi-nodo | bus load + recovery + loss test |
| EMS-004 | Limiti motore da manuale | Setup/EMS | TODO | 912 UL 80 hp riscontrato sul sito Rotax; libreria OM-912 identificata; audit rileva limiti legacy non affidabili | manca seriale e revisione OM-912 applicabile; profilo contiene valori indicativi/anomali | acquisire targhetta e OM-912, poi sostituire limiti con riferimenti di pagina | POH/manuale citato e approvato |
| CFG-001 | Catalogo operativo completo modello/variante/esemplare | Setup/Core | PARTIAL | dizionario V2 con 447 parametri/28 domini e tre profili separati; workflow condiviso PFD/EMS; import fail-closed e prefill sempre SIM; test software PASS; Setup EGLFS reale `.49` usa ora identita'/piattaforma/ruolo/profilo da configurazione e mostra MFD-01 correttamente | touch assente; l'esemplare reale conserva 309 blocker e mancano fonti controllate/release | provare workflow touch e acquisire documenti applicabili | profilo reale con 447/447 campi risolti o motivati, fonti a pagina, release firmata e caricamento runtime verificato |
| BBX-001 | Black box append-only | Black box | DONE/PARTIAL | recorder e ingest; hub Emergency fail-closed su storage error con quick-check iniziale, health degradata e 9 test software inclusi SQLite full/recovery e transazione interrotta; harness ext4 loopback separato da produzione disponibile | retention e fault filesystem/NVMe reali da chiudere; harness non ancora eseguito sul nodo | eseguire fault injection isolata, poi pianificare power-loss ed endurance | power-loss recovery + retention |
| BBX-002 | Ingestion unico dei bus | Black box | DONE-ARCH | ADR-003 | deployment finale | consolidare distribuzione | nessun display consuma frame grezzi |
| BBX-003 | Broker stato normalizzato ai display | Black box | PARTIAL | DDP V2 al PFD; EDP V1/49003 installato verso EMS; publisher 20 Hz senza failure e prova STALE/LOST/recovery riuscita; feed esplicitamente SIM | decoder CAN motore non collegato a EDP; reboot/Pixhawk/endurance aperti | integrare sorgente CAN reale e prova reboot/endurance | protocollo versionato + replay + perdita/reordine HW multi-display |
| BBX-004 | Edge weather/cache | Black box | PARTIAL | servizio presente | freshness/offline | hardening | live/cached/stale verificati |
| NET-001 | Fan-out MAVLink monitor-only verso Ground Station e server | Black box/Portatile/Server | PARTIAL | ADR-015; fan-out TX-only, forwarding zero; confronto v2 Ethernet/Wi-Fi integro; schema v3 e verificatore separano integrity/liveness/uniformity; report reali classificati Ethernet PASS completo, Wi-Fi link PASS/quality DEGRADED/overall FAIL; broker/QGC stabili | uniformità Wi-Fi FAIL per intervallo 1.014,687 ms; nessuna cattura v3 o simultanea; persistenza power save post-reboot, authentication, fault/endurance e POWER1 aperti | portare Ground Station su Ethernet; prova simultanea; mantenere quality degradata visibile | Pixhawk Ethernet indipendente dalla USB, due osservatori stabili, zero ritorno verso FC e nessun impatto su broker/recorder/DDP/EDP |
| SRV-001 | Server mappe/imagery | Server casa | PARTIAL | mapapp/Caddy/status; reboot 2026-09-14: 12 container Up, mapapp direct HTTP 200 con RestartCount=0, Caddy endpoint protetto 401 | lifecycle pacchetti; CA Caddy non presente nel trust store locale del server | installare/verificare CA locale senza `-k`; versioning+manifest | update atomico + rollback e HTTPS validato dal trust store |
| SRV-002 | Cataloghi/navdata centralizzati | Server casa | PARTIAL | DB/cataloghi presenti | validità/fonti | pipeline update | checksum/provenance/versione |
| SRV-003 | Distribuzione corridor pack | Server->PFD | PARTIAL | 1326 tile cache verificati | resume/quota/GC | chiudere lifecycle | perdita rete senza perdita pack attivo |
| NAV-001 | Database aeroporti/navaid/airspace/reporting | RPi5/Server | PARTIAL/DONE-DATA | DB e importer | ciclo aggiornamento | validazione fonti | dataset versionato e controllato |
| NAV-002 | Modello unico FlightPlan/leg/WP | Core | PARTIAL | route/prototipi | sorgenti NAV multiple | consolidare modello canonico | direct-to/nearest/route test |
| NAV-003 | CDI/OBS/BRG/DTK/TRK | RPi5/Core | PARTIAL | prototipi | annunci validità | integrare con model | scenari GPS/VOR/NDB e loss test |
| SV-001 | Synthetic Vision | RPi5 | PARTIAL | EFIS Next `f27508a` e' il solo renderer operativo su `pi5` `.49`; PFD/Canvas EGLFS reali confermano caricamento asset GPU/texture e manifesto `OFFLINE_READY` da 1326 tile, circa 34–35 presentazioni/s stabilizzate | catture reali mostrano texture stirata e sfocata nel primo piano; 27 gap di copertura; qualita' non accettata | correggere campionamento/LOD/proiezione del terreno e ripetere la rotta simulata | prestazioni e qualità target |
| SV-002 | DEM Copernicus/SRTM | RPi5/Server | PARTIAL | pacchetto Veneto verificato | copertura/provenance | estendere Nord Italia | seam/quote/checksum verificati |
| SV-003 | TAWS | RPi5/Core | PARTIAL/EXPERIMENTAL | shader/envelope prototipale | safety critica | definire envelope | test indipendenti, no terrain-safe su missing |
| WX-001 | METAR/TAF | Black box/RPi5 | PARTIAL | parser/edge service | provider/licenze/freshness | registry e timestamp separati | raw+parsed+stale test |
| WX-002 | Radar meteo | RPi5/Server | PARTIAL | overlay presente | georef/provider | verifica georeferenziazione | LIVE/CACHED/STALE/SIM corretti |
| FPV-001 | Vista FPV | RPi4 VIDEO-CORE dedicato + viewer RPi5 | PARTIAL | ADR-013 assegna acquisizione/decode/mosaico FPV a un RPi4 distinto dall'EMS; i viewer PFD/MFD RPi5 consumano soltanto il flusso selezionato e health | pipeline reale e protocollo VIDEO-CORE→viewer non implementati | definire processo GStreamer/V4L2 HW e contratto stream/health senza bridge fra VIDEO e FLIGHT-DATA | latenza e freeze detection verificate senza impatto EMS/PFD |
| FPV-002 | Quattro telecamere | RPi4 VIDEO-CORE dedicato | TODO | requisito e allocazione RPi4 definiti; isolamento fisico EMS/FPV imposto dal validatore del piano | hardware RPi4 video e switch VIDEO non ancora identificati; banda/decoder/power aperti | selezionare RPi4 e switch VIDEO, poi provare quattro ingressi e mosaico | 4 feed, PiP, failover e fault isolation verificati |
| FPV-003 | Registrazione video sincronizzata | Black box | TODO | requisito | storage/timestamp | specifica | sync telemetria verificata |
| GS-001 | QGroundControl/bridge manutenzione | Portatile | PARTIAL | QGC PID 2660/zero restart; Wi-Fi link PASS/quality DEGRADED; preflight Ethernet v2 a due fasi implementato con guardie di segmentazione, 8 test PASS | `eno1` reale visto NO-CARRIER nell'ultima prova; accesso SSH corrente non autorizzato; nessun profilo/switch eseguito; persistenza power save, confronto parametri e shutdown aperti | collegare `eno1` al router/switch LAN di servizio (mai al Waveshare FLIGHT-DATA), ripristinare SSH, ottenere physical PASS, migrare `.39` con rollback e verificare MAVLink v3 | recovery ripetibile, qualità link misurata e nessuna scrittura involontaria |
| GS-002 | Backup firmware/parametri Pixhawk | Portatile | DONE | backup PX4/ArduPlane | proteggere copie | replica verificata | doppia copia + hash |
| GS-003 | Tooling sviluppo Windows | Portatile | PARTIAL | build MSVC/Qt | Smart App Control | documentare workaround | build/test riproducibili |
| LEG-R4-001 | EFIS RPi4 storico | RPi4 | LEGACY | operativo | non regredire | congelare | nessuna modifica salvo fix critici |
| LEG-R5-001 | `sv3d-fondamenta` | RPi5 | LEGACY | servizi disabilitati/rimossi dal percorso systemd e directory `/home/pi/efis` e `/home/pi/efis2` spostate nella quarantena recuperabile `/home/pi/legacy-disabled-20260912`; EFIS Next ha superato due reboot senza dipendenze eseguibili dal legacy | quarantena occupa circa 5 GiB; `fuso-can` conserva soltanto un riferimento documentale al vecchio percorso | mantenere la quarantena fino all'acceptance, poi autorizzare cancellazione definitiva e aggiornare il riferimento documentale CAN | nessuna dipendenza nascosta e rollback Next indipendente |
| PROD-001 | Profili Standard/Professional | Tutti | TODO/DESIGN | concetto definito | hardware feature matrix | congelare SKU/features | matrice prodotto approvata |
| PROD-002 | Alimentazione avionica | Aereo | TODO | requisito | safety/hardware | progettare PSU/protezioni | brownout/EMI/load test |
| PROD-003 | Enclosure e termica | Aereo | TODO | prototipi HW | vibrazioni/temperatura | progetto meccanico | soak/vibration/thermal test |
| PROD-004 | Harness/connettori | Aereo | PARTIAL | Waveshare 5P identificato; schema FLIGHT-DATA V1; video separato | 5 porte insufficienti per finale; cavi/EMI aperti | cablare banco e selezionare switch video/finale | harness documentato e testato |
| PROD-005 | Update/rollback software | Tutti | PARTIAL | release immutabile 93bbe44 attiva su `.45`; release `3432402` attiva su `.49` dopo switch atomico, con `1d02b84` come rollback immediato e `f27508a` ancora conservata; profilo MFD direct-DRM versionato e legacy in quarantena recuperabile | deployment non uniforme; gate Wayland `.45` e rollback automatico non provocati; release `.49` corrente richiede reboot, touch e soak lungo | validare reboot/rollback MFD e gate Wayland PFD, poi standardizzare tutti i profili | rollback simulato su tutti i nodi e nessuna promozione con gate HW aperto |
| VER-001 | Test automatici core | Dev/RPi5 | DONE/PARTIAL | suite multiple + 16/16 CTest PASS su release ARM64 12b4697 (`.45`) e ab0429e (`.49`) + EFIS Core CI Linux PASS su PR #6 | estendere piattaforme e fault test | mantenere CI green | green required per PR |
| VER-002 | HIL | Banco | TODO | concetto | hardware fixture | costruire banco HIL | fault injection ripetibile |
| VER-003 | Test termico prolungato RPi5 | RPi5 | PARTIAL | `.45` ha superato 60 min; baseline MFD `.49` `f27508a` ha superato 60/60 campioni, 56,0–60,9 °C, zero restart/throttle, DDP 19,9 Hz e zero reject/gap/socket error | soak precedente non copre release corrente `3432402`; manca carico terrain/video combinato e qualificazione definitiva PSU/cavo | ripetere >=60 min su release corrente con terrain/video worst-case | nessun throttle/undervoltage |
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
