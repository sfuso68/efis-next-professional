# FUSO AVIONICS NEXT — Stato dell'arte e piano operativo

Data di congelamento: 2026-09-22

Repository autorevole: `sfuso68/efis-next-professional`

Baseline integrata esaminata: `main` `61070b8d990c01ca58943a6370402623f4320ce7`

C12 integrato tramite PR #29: source
`a907a87ed58cb6899ccc43937cdfed35be79517a`, merge
`61070b8d990c01ca58943a6370402623f4320ce7`

Classificazione: **DEVELOPMENT / NON-CERT / NON APPROVATO AL VOLO**

## 1. Scopo, autorità e regole di lettura

Questo documento è il quadro direzionale completo del programma. Riunisce:

- situazione reale del software, dei nodi e dell'infrastruttura;
- avanzamento rispetto al Master Project Brief R0;
- problemi tecnici e organizzativi aperti;
- attività eseguibili senza hardware;
- attività che richiedono presenza dell'operatore, accesso ai terminali o banco;
- sequenza di sviluppo, gate e criteri di uscita.

Non sostituisce le fonti operative sottostanti:

1. `docs/status/EFIS_NEXT_MASTER_MATRIX.md` governa i requisiti e gli stati;
2. `docs/status/EFIS_NEXT_ACTIVE_RELEASES.md` governa le release osservate;
3. `docs/status/EFIS_NEXT_STATE.md` è la sintesi corrente;
4. `docs/project/AVIONIX_NEXT_EXECUTION_BACKLOG.md` governa le attività per gate;
5. ADR e bundle sotto `docs/status/evidence/` dimostrano decisioni e verifiche.

Le parole **implementato**, **verificato software**, **verificato ARM64**,
**verificato hardware** e **accettato dall'operatore** non sono sinonimi. Una
funzione non è `DONE` se il criterio della Master Matrix richiede una prova che
non è stata acquisita.

## 2. Quadro direzionale immediato

### 2.1 Avanzamento misurato

| Indicatore | Valore corrente | Interpretazione |
|---|---:|---|
| Requisiti tracciati | 71 | perimetro oggi censito, non totale definitivo del prodotto |
| Avanzamento ponderato | **46,8%** | copertura progettuale, non prontezza al volo |
| `DONE` | 4 | criterio di chiusura soddisfatto |
| `VERIFY-HW` | 11 | software o contratto presenti, prova fisica ancora necessaria |
| `PARTIAL` | 38 | lavoro o integrazione incompleti |
| `TODO` | 14 | funzione sostanziale non ancora realizzata |
| `BLOCKED` | 2 | dipendenza documentale/hardware non risolta |
| `LEGACY` | 2 | conservato come riferimento, fuori dalla nuova baseline |

### 2.2 Lettura corretta dello stato

Il progetto ha raggiunto una buona maturità nei contratti dati, nel core C++20,
nei test fail-closed, nella HMI simulata, nel packaging e nel rollback. È invece
ancora incompleto nelle prove integrate con sensori reali, alimentazione,
touch, termica, fault fisici, video, cablaggi e acceptance operatore.

Il collo di bottiglia non è più principalmente la scrittura di codice. È la
conversione del software già presente in evidenza ripetibile sul banco:

`sensore reale -> nodo sensori -> Blackbox -> PFD/EMS -> log -> replay`.

### 2.3 Semaforo sintetico

| Area | Stato | Valutazione |
|---|---|---|
| Governance, CI e controllo progetto | GIALLO | naming prodotto chiuso; fonti e restore NAS aperti |
| Core e contratti dati | VERDE SOFTWARE / GIALLO HW | core e ICD solidi; verticale reale incompleto |
| Release e rollback | VERDE per i nodi gestiti | contratto immutabile V1 verificato; prodotto sempre non-cert |
| PFD/MFD/EMS grafici | VERDE SOFTWARE / GIALLO HW | HMI avanzata; touch, leggibilità e fault reali da accettare |
| Synthetic Vision | GIALLO | pipeline e correzione C12 pronte; prestazioni/qualità RPi5 da provare |
| Pixhawk/M10/DLVR | GIALLO-ROSSO | telemetria osservata; calibrazione e failure modes incompleti |
| Nodo sensori ASSO5 | ROSSO | RPi4 identificato, schede Sequent e mapping non verificati |
| Alimentazione/harness/enclosure | ROSSO | storico undervoltage e nessuna qualifica elettrica finale |
| FPV/quattro telecamere | ROSSO | architettura definita, catena reale non qualificata |
| Emergenze/voce | GIALLO-ROSSO | ausili software presenti; fonti, audio reale e safety aperti |
| HIL e acceptance complessiva | ROSSO | banco e campagna integrata non ancora eseguiti |

## 3. Architettura consolidata

### 3.1 Principi vincolanti

- La Blackbox è il punto unico di ingestione, arbitraggio, normalizzazione e
  registrazione. I display non consumano direttamente i bus grezzi.
- PFD e MFD sono su RPi5. EMS e VIDEO-CORE sono due RPi4 distinti. Il nodo
  ASSO5-SENSOR è un terzo ruolo RPi4, separato dagli altri due.
- FLIGHT-DATA e VIDEO sono domini separati. Non è autorizzato un bridge/NAT
  generalizzato tra i domini.
- Pixhawk è una sorgente read-only. L'EFIS V1 non comanda autopilota, servi,
  radio, transponder, flap o carrello.
- Tutti i dati devono mantenere provenance, quality, validity, freshness e
  distinzione `REAL`/`SIM`/fallback.
- Emergency Assist resta consultivo. Ogni futura autorità attiva richiede un
  programma separato con interlock, HIL e safety case.

### 3.2 Topologia logica

```text
Pixhawk 6X / M10 / DLVR ---------+
                                  |
ASSO5-SENSOR RPi4 + Sequent ------+--> BLACKBOX RPi5
                                  |      - ingestione/arbitraggio
MGL / ESP32 aux ------------------+      - DDP/EDP publisher
                                         - recorder/blackbox
                                         - cache meteo/eventi
                                              |
                           +------------------+------------------+
                           |                  |                  |
                        PFD RPi5           MFD RPi5           EMS RPi4

VIDEO-CORE RPi4 + telecamere ---> rete VIDEO separata ---> viewer PFD/MFD

serverha <--> GitHub <--> NAS        (sviluppo, mappe, replica, restore)
portatile/QGC                         (manutenzione monitor-only)
```

### 3.3 Nodi e stato osservato

| Nodo | IP noto | Ruolo | Release/stato osservato | Stato operativo |
|---|---:|---|---|---|
| `efis-pfd-01` | `192.168.1.45` | PFD + recorder, RPi5 | `pfdrec-d52e377-primary-v1` | immutabile, rollback e reboot PASS; acceptance grafica aperta |
| `pi5` | `192.168.1.49` | MFD/NAV/SV, RPi5 | `mfd-3432402-primary-v1` | immutabile, rollback e reboot PASS; touch/SV/soak aperti |
| `efis` | `192.168.1.36` | EMS, RPi4 | `ems-7655799-primary-v1` | immutabile, EDP `LIVE - SIM`; sensori reali aperti |
| `efisblackbox` | `192.168.1.47` | Blackbox/broker, RPi5 | `blackbox-7655799-primary-v1` | immutabile, broker PASS; engine ancora SIM |
| `ems-sensor` | `192.168.1.89` | ASSO5-SENSOR, RPi4 8 GB | nessuna release distribuita | identità verificata; Sequent 0/4, Ethernet senza carrier |
| Pixhawk 6X | `10.77.10.3` storico | flight source read-only | ArduPlane/stream osservato | qualifica M10/DLVR e fault aperta |
| `serverha` | `192.168.1.6` | mappe, servizi, Git mirror/working copy | container e replica presenti | manifest servizi, CA, restore e lifecycle aperti |
| `PORTATILE` | `192.168.1.39` | QGC e manutenzione | monitor-only/firewall | Wi-Fi degradato; migrazione Ethernet e recovery aperti |
| NAS | `192.168.1.22` | replica Git/evidence | bare Professional replicato | restore drill e protezione fuori sito aperti |
| VIDEO-CORE | da assegnare | FPV, RPi4 | non qualificato | hardware, decoder, camere e rete VIDEO da definire |

Gli indirizzi sono riferimenti di banco, non una policy finale di addressing.
Vanno riconfermati prima di ogni sessione.

## 4. Stato per gate del planning R0

| Gate | Obiettivo | Stato reale | Evidenza disponibile | Uscita ancora necessaria |
|---|---|---|---|---|
| G0 — Verità del progetto | autorità, fonti, naming, repository e baseline univoci | **PARTIAL** | nome `FUSO AVIONICS NEXT` approvato, repo Professional autorevole, ADR-018, matrix, generatori, GitHub/serverha/NAS allineati | 5 fonti mancanti, restore NAS, protezione off-site e ritiro controllato copia Blackbox |
| G1 — Nodo sensori P0 | hardware identificato, acquisizione e soak | **BLOCKED-HW / IN PROGRESS** | RPi4 `.89`, tool e contratti fail-closed | 4/4 Sequent, DIP, PSU, mapping 18 canali, calibrazione, driver e soak 60 min |
| G2 — Contratto dati e replay | ICD congelato e verticale riproducibile | **ACCEPTED-SW / VERIFY-HW** | ICD, digest e 11/11 test su PFD/MFD/EMS ARM64 | trasporto upstream approvato e verticale reale con stesso dataset/replay |
| G3 — EFIS integrato al banco | HMI, SV, EMS e pannelli integrati | **PARTIAL** | golden, stati degradati, session manager, benchmark e packager | touch, pannelli, SV C9, frame-rate, termica, EMS reale e MGL passivo |
| G4 — Prototipo installabile | PSU, switch, harness, enclosure, schema filo-per-filo | **PROPOSED** | architettura rete e candidato switch documentati | qualifica switch, PSU, brownout/load, harness, protezioni, EMI e enclosure |
| G5 — Prove dinamiche | HIL, fault injection ed endurance | **PROPOSED** | simulatori e validator software | fixture HIL, fault fisici, endurance e criteri firmati |
| G6 — Baseline EFIS Next | release/configuration index/recovery accettati | **IN PROGRESS** | release immutabili e rollback sui nodi gestiti | chiusura G0-G5, configuration index e acceptance complessiva |
| G7 — CM / Next Plus | porting industriale con parità | **NON AVVIATO** | soli principi architetturali | baseline G6, hardware scelto e benchmark equivalente |

Conclusione critica: G2 è il gate più maturo come contratto; G1 e G4 sono i
principali blocchi fisici; G3 ha molta implementazione ma poca acceptance
hardware; G6 non può essere dichiarato raggiunto sommando test software.

## 5. Stato per workstream

| WS | Ambito | Stato | Risultato raggiunto | Lavoro decisivo rimanente |
|---|---|---|---|---|
| WS0 | governance e documentazione | PARTIAL | naming `FUSO AVIONICS NEXT`, brief, audit, ADR, matrix, CI e topologia Git | fonti, restore e brief R1 |
| WS1 | contratti dati | VERIFY-HW | DDP V2, EDP V1, ICD, CRC/SHA, replay ARM64 | verticale fisico e failure modes reali |
| WS2 | ASSO5 sensor node | BLOCKED-HW | identità RPi4 e framework REAL/SIM/health | inventario, adapter reali, mapping, calibrazione, soak |
| WS3 | HMI PFD/MFD/EMS | VERIFY-HW | design system, golden, responsive e stati degradati | pannelli/touch, sunlight/night, FPS, termica, fault |
| WS4 | SV, mappe e NAV | PARTIAL | DEM/imagery/navdata, renderer e C12 integrato | deployment gemello autorizzato, C9/C10/C11 su due RPi5, TAWS e NAV completa |
| WS5 | air data/AHRS | PARTIAL-HW | Pixhawk e dispositivi rilevati, modelli core | M10 outdoor, compass, DLVR zero/linearità, failover |
| WS6 | EMS Rotax/MGL | PARTIAL | HMI, simulatore, EDP, ore/log e release EMS | sorgenti CAN/MGL reali, limiti controllati e calibrazioni |
| WS7 | Pixhawk/autopilota | PARTIAL-HW | osservazione MAVLink e fan-out read-only | backup verificato, fault/endurance; nessuna autorità comando V1 |
| WS8 | Blackbox/server/laptop/NAS | PARTIAL | broker, recording, mappe, QGC, replica Git | endurance, restore, CA, lifecycle, auth e policy offline |
| WS9 | elettrico/enclosure | PROPOSED | segmentazione e candidati documentati | schema filo-per-filo e qualifica ambientale/elettrica |
| WS10 | qualità/test/safety | STRONG-SW / WEAK-HW | CI, test fail-closed, evidence bundle e rollback | HIL, fault fisici, acceptance operatore e safety case |
| WS11 | configuration management | IN PROGRESS | manifest immutabili, registro release, rollback/reboot | configuration index di sistema e prove restore complete |
| WS12 | Next Plus | TODO | correttamente rinviato | iniziare solo dopo G6 |

## 6. Stato tecnico per sottosistema

### 6.1 Core, provenance, health e arbitraggio

**Disponibile:** core C++20 indipendente dalla UI, snapshot tipizzati,
provenance, validity, quality, timestamp, health, freshness, arbitraggio e
failover; numerosi test unitari e sanitizer Linux.

**Aperto:** priorità e reversione con sorgenti reali simultanee, jitter reale,
recovery recorder e copertura hardware di tutti i SignalId. `CORE-003` e
`CORE-004` restano `VERIFY-HW`.

### 6.2 PFD e MFD

**Disponibile:** release immutabili, PFD, nastri, HSI/FMA, NAV/SV, fail-state,
profili V-speed atomici, recorder, setup e modello HMI condiviso.

**Aperto:** acceptance visuale/touch, dati reali e fault, transizione terrain,
soak worst-case, verifica sunlight/night e stabilità del pannello MFD. Il touch
USB del MFD risultava assente nell'ultima evidenza.

### 6.3 Synthetic Vision, terreno e navigazione

**Disponibile:** DEM/imagery Veneto, cache di 1.326 tile, pipeline server,
renderer Quick 3D, benchmark C9, sessione C10, assembler C11. C12 corregge il
LOD usando la distanza dall'impronta della tile con isteresi 18/24 km e preload
45 km.

**Problema noto:** le catture precedenti mostravano primo piano stirato o
sfocato, gap di copertura e circa 34-35 presentazioni/s, sotto la soglia C9 di
40 FPS. C12 è ora parte della baseline, ma resta verificato solo su workstation:
non è stato distribuito né accettato sui due RPi5.

**Criterio:** stesso commit sui due RPi5, rotta Treviso-Belluno, cinque
checkpoint DAY/NIGHT, `>=40 FPS`, p95 `<=25 ms`, termica accettabile, fault
missing-tile esplicito e firma operatore.

### 6.4 EMS, Rotax e MGL

**Disponibile:** EMS professionale RPi4, simulatore, stati
LIVE/SIM/STALE/WAITING/LOST, CAS, CHT/EGT, strumenti circolari, carburante,
impianto elettrico, log e ore motore, setup e release immutabile.

**Aperto:** CAN motore reale, MGL RS-232 passivo, calibrazione sonde,
fuel-flow, limiti controllati e touch. Il prodotto supporterà entrambe le
famiglie **Rotax 912 UL** e **Rotax 912 ULS** mediante profili distinti. Limiti,
allarmi e procedure non devono essere condivisi automaticamente: l'attivazione
di ciascun profilo richiede targhetta, seriale, manuale applicabile e revisione.

### 6.5 ASSO5-SENSOR e Sequent

**Disponibile:** nodo RPi4 `.89` identificato, Bookworm aarch64, 8 GB, I2C
esposto, contratti d'inventario/readiness e kernel health testati in software.

**Aperto e bloccante:** alimentatore, modello/revisione delle quattro schede,
DIP, ordine stack, 18 canali, conversioni, calibrazioni e trasporto upstream.
Nessun probing, cambio DIP o deployment va eseguito senza una sessione
controllata.

### 6.6 Pixhawk, GNSS e airspeed

**Disponibile:** Pixhawk 6X con MAVLink read-only, fan-out monitor-only,
isolamento della rete protetta, M10 e Matek ASPD-DLVR rilevati.

**Aperto:** fix 3D outdoor, accuratezza e log GNSS, orientamento/interferenze
del magnetometro, zero e linearità DLVR, mapping health, fault barometro/EKF,
failover e backup firmware/parametri con doppia copia verificata.

### 6.7 Blackbox e rete

**Disponibile:** broker unico, DDP/EDP, recorder, release immutabile,
rollback/reboot, fan-out MAVLink TX-only e firewall observer. La rete
FLIGHT-DATA resta separata dalla rete VIDEO.

**Aperto:** decoder CAN reale, storage endurance/power-loss, retention,
autenticazione, recovery prolungato e misura della rete con tutti i consumer.
Il precedente risultato Wi-Fi Ground Station è `DEGRADED/FAIL` per timing; la
soluzione preferita è Ethernet di servizio, non un bridge verso FLIGHT-DATA.

### 6.8 Server, imagery, meteo e NAS

**Disponibile:** serverha con mapapp/Caddy, navdata, DEM, OSM, hillshade,
imagery e corridor packs; GitHub autorevole, serverha mirror/working copy e NAS
come replica; timer serverha->NAS osservato `SUCCESS`.

**Aperto:** manifest di immagini/container, CA nel trust store, update atomico,
restore completo dal NAS, protezione off-site, quota/garbage collection,
freshness e licenze/provenance dei provider meteo e cartografici.

### 6.9 FPV e video

**Disponibile:** allocazione architetturale di VIDEO-CORE su RPi4 separato,
viewer su PFD/MFD e concetto a quattro camere.

**Aperto:** nodo fisico, decoder hardware, camere, gimbal, GStreamer/V4L2,
switch VIDEO Gigabit/PoE, banda, latenza, freeze detection, mosaico/PiP e
registrazione sincronizzata. Il MokerLink 100 Mbps candidato per altri usi non
è automaticamente idoneo alla rete VIDEO.

### 6.10 Emergenze, voce e fraseologia

**Disponibile:** Emergency Assist consultivo, UI di supporto al volo, eventi e
ack persistiti, grammatica IT/EN chiusa, gate PTT/confidence e Radio Assist
prototipale.

**Aperto:** POH e procedure approvate, audio reale, microfono/PTT/interfono,
voce naturale offline, misura false accept/reject, rumore cabina, database
fraseologia/frequenze versionato, acceptance RPi5 e safety case. `PAN-002` e
`PAN-003` restano bloccati; nessun comando attivo ai servi è autorizzato.

### 6.11 Alimentazione, switch, cablaggi ed enclosure

**Problema prioritario:** la Blackbox ha storico `get_throttled=0x50000`.
Campioni successivi `0x0` non cancellano l'evento storico. PSU, cavi,
connettori e margini non sono qualificati.

Waveshare 5P è insufficiente come switch finale della rete completa. MokerLink
è un candidato non verificato. Mancano schema filo-per-filo, protezioni,
brownout/load test, termica, EMI, vibrazioni ed enclosure.

## 7. Problemi e rischi ordinati

| Priorità | Problema/rischio | Impatto | Azione di riduzione |
|---|---|---|---|
| P0 | Sequent e PSU ASSO5 non identificati | blocca EMS reale e G2 verticale | inventario fotografico 4/4, DIP, stack, PSU e mapping |
| P0 | alimentazione non qualificata | reset, corruzione o perdita dati/display | inventario elettrico e brownout/load test controllati |
| P0 | acceptance pannelli assente | difetti touch/leggibilità/termica non rilevati | sessione HMI C8/A20 su PFD/MFD/EMS |
| P0 | fonti motore/POH insufficienti | limiti o checklist potenzialmente errati | targhette, seriali e manuali/revisioni applicabili |
| P1 | C12 non ancora provato sui RPi5 | qualità SV e prestazioni ignote | preparare release gemella, poi C9/C10/C11 su entrambi dopo autorizzazione |
| P1 | M10/DLVR non calibrati | IAS/heading/position non affidabili | test cielo aperto e banco pitot/statico |
| P1 | Blackbox single point incompleto | perdita distribuzione/log | fault storage/rete, watchdog, endurance e recovery |
| P1 | restore NAS non provato | replica esistente ma recuperabilità ignota | restore drill in directory isolata con hash |
| P2 | FPV non qualificato | latenza/saturazione/interferenza | rete separata e benchmark quattro feed |
| P2 | meteo/voce dipendenti da servizi | degrado offline o annunci ambigui | cache, freshness, fallback e marcatura disponibilità |
| P2 | debito di acceptance | nuove funzioni aumentano il rischio integrativo | bloccare espansioni non prioritarie fino a chiusure P0 |

## 8. Attività che richiedono interazione, terminale o hardware

Queste attività non devono essere marcate `DONE` finché l'evidenza non è
acquisita e archiviata.

### I-01 — C12 integrato; resta la verifica hardware

- completato: PR #29 unita dopo 4/4 check CI verdi al merge `61070b8`;
- operatore: autorizzare separatamente il deployment gemello su PFD e MFD;
- evidenza ancora richiesta: commit installato, C10/C11, benchmark C9 e firma operatore;
- limite: il merge non implica deployment né approvazione al volo.

### I-02 — Inventario ASSO5/Sequent

- presenza fisica richiesta sul nodo `.89`;
- nove fotografie previste, modelli/revisioni, ordine stack, DIP, alimentatore,
  connettori e cablaggi;
- nessun cambio DIP o probing attivo durante la sola ricognizione;
- output: bundle con hash e checklist 4/4.

### I-03 — Qualifica alimentazione

- identificare PSU/cavi di Blackbox, PFD, MFD, EMS e sensor node;
- eseguire solo con piano approvato cold/warm reboot, carico, brownout e log
  tensione/temperatura/throttling;
- output: `0x0`, zero reset/perdita dati e margine documentato.

### I-04 — Acceptance HMI sui tre pannelli

- installare lo stesso commit verificato;
- scenari LIVE/SIM/STALE/WAITING/LOST/FAIL;
- touch, leggibilità, overlay, sunlight/night, FPS, temperatura e soak;
- output: bundle C8/A20/P20 e firma operatore.

### I-05 — Benchmark Synthetic Vision PFD/MFD

- creare sessioni C10 dal commit realmente installato;
- acquisire log e screenshot con C11;
- verificare C9 sulla rotta Treviso-Belluno e il confine N45/N46;
- ripetere fault missing-tile senza modificare le soglie per far passare il test.

### I-06 — Pixhawk, M10 e DLVR

- GNSS cielo aperto, satelliti, accuratezza e log;
- compass orientation/interference/consistency;
- zero airspeed, linearità, perdita nodo e recovery;
- barometro/EKF unhealthy e failover read-only;
- output: log firmati e backup parametri/firmware con doppio hash.

### I-07 — Catena sensori e verticale G2

- dopo I-02 e approvazione upstream, deploy controllato dell'adapter;
- stesso dataset dalla sorgente al replay;
- fault open/short/timeout/reorder/loss e recovery;
- output: CRC/SHA coincidenti, provenance e timing verificati.

### I-08 — Blackbox endurance e storage fault

- soak >=60 minuti con tutti i consumer;
- filesystem full/inode/I/O e power-loss solo in ambiente isolato;
- verifica retention, quick-check, recovery e allarmi sui display.

### I-09 — Ground station Ethernet

- collegare `eno1` alla LAN di servizio, mai direttamente a FLIGHT-DATA;
- eseguire preflight, rollback, test simultaneo server/portatile e monitorare
  che non esista traffico di ritorno verso il flight controller.

### I-10 — Restore NAS

- usare una directory isolata su serverha;
- clonare dal bare NAS, verificare `main`, tag, oggetti e build minima;
- non sostituire working copy o runtime durante il drill.

### I-11 — Banco audio/voce

- identificare interfono, PTT, microfono, uscita audio e isolamento;
- corpus IT/EN, rumore cabina, latenza e false accept/reject;
- confrontare motore TTS naturale offline compatibile ARM64;
- mantenere sempre fallback touch e nessuna autorità sui comandi.

### I-12 — Banco FPV

- identificare VIDEO-CORE, quattro camere e switch VIDEO;
- misurare decode simultaneo, mosaico/PiP, latenza end-to-end, banda, termica,
  freeze/loss e registrazione sincronizzata.

## 9. Attività eseguibili senza interazione hardware

1. Preparare il piano di release PFD/MFD C12 per il benchmark senza distribuirlo.
2. Preparare il runbook C9/C10/C11 e i controlli di rollback.
3. Consolidare modello NAV unico, annunci di validità e test GPS/VOR/NDB.
4. Estendere test del core ai SignalId ancora non coperti.
5. Preparare decoder CAN/MGL con fixture sintetiche solo dopo documentazione.
6. Costruire manifest/versionamento di dataset, mappe, meteo e corridor pack.
7. Definire HIL matrix e generatori di fault senza collegare uscite fisiche.
8. Formalizzare configuration index G6 e distinta delle release correnti.
9. Preparare design elettrico e schema filo-per-filo come bozza revisionabile.
10. Definire contratto VIDEO stream/health e benchmark ripetibile.
11. Preparare corpus voice/radio e strumenti di scoring offline.
12. Aggiornare brief R1 dopo le decisioni aperte, senza cambiare gli stati
    hardware in assenza di nuove prove.

## 10. Planning operativo raccomandato

La sequenza è guidata dalle dipendenze, non dal numero di funzioni richieste.

### Fase 0 — Controllo configurazione corrente

**Obiettivo:** evitare che nuove attività partano da baseline diverse.

- registrare la PR #29 integrata e la baseline `61070b8`;
- preparare il configuration index C12 senza distribuire sui nodi;
- aggiornare `main`, registro release e configuration index;
- confermare replica serverha->NAS;
- eseguire restore drill NAS.

**Exit:** un solo commit di riferimento, backup recuperabile e nessuna PR
critica sospesa senza decisione.

### Fase 1 — Chiusura G1 e alimentazione

**Obiettivo:** rendere il nodo sensori realmente conoscibile e alimentabile.

- I-02 inventario Sequent;
- definizione mapping 18 canali;
- identificazione PSU/cavi;
- I-03 qualifica elettrica;
- scelta e acceptance dello switch FLIGHT-DATA.

**Exit:** 4/4 schede identificate, DIP/mapping approvati, PSU qualificata,
switch accettato e nessun evento undervoltage.

### Fase 2 — Verticale dati reale G2

**Obiettivo:** chiudere la prima catena end-to-end ripetibile.

- approvare trasporto upstream e SourceId;
- completare driver/calibrazioni minime;
- I-07 verticale live e replay;
- fault/recovery e soak 60 minuti.

**Exit:** stesso dataset e stessi esiti lungo tutta la catena, con provenance,
freshness, health e digest verificati.

### Fase 3 — Acceptance HMI e Synthetic Vision G3

**Obiettivo:** trasformare la maturità grafica software in acceptance fisica.

- I-04 sui tre pannelli;
- I-05 C9 su PFD e MFD;
- correggere soltanto difetti dimostrati dalle evidenze;
- ripetere soak con terrain e carico reale.

**Exit:** bundle firmati, touch leggibile, nessuna sovrapposizione, fail-state
corretti, `>=40 FPS`, p95 `<=25 ms` e termica entro limiti.

### Fase 4 — Air data ed EMS reali

**Obiettivo:** sostituire progressivamente SIM con sorgenti qualificate.

- I-06 M10/DLVR/Pixhawk;
- CAN motore e MGL passivo;
- calibrazione sonde e fuel-flow;
- limiti Rotax da fonte controllata;
- confronto indipendente delle sorgenti.

**Exit:** valori reali calibrati, failover/stale/lost provati e nessun limite
inventato.

### Fase 5 — Prototipo installabile G4

**Obiettivo:** rendere riproducibile la piattaforma fisica.

- schema filo-per-filo, protezioni e grounding;
- harness e connettori;
- enclosure, raffreddamento e manutenzione;
- rete VIDEO separata;
- review di installazione.

**Exit:** distinta base, schemi revisionati, test elettrici e termici PASS.

### Fase 6 — HIL, endurance e safety G5

**Obiettivo:** provare i guasti prima di qualunque uso dinamico.

- HIL con loss/reorder/corruption/stale;
- guasti sensori, rete, storage, alimentazione e display;
- campagne 8/24/72 ore secondo rischio;
- Emergency Assist, voce e meteo in modalità advisory.

**Exit:** matrice fault completa, recovery ripetibile, evidenza indipendente e
nessuna autorità involontaria.

### Fase 7 — Baseline G6

**Obiettivo:** congelare EFIS Next Professional come prototipo controllato.

- configuration index completo;
- release manifestate e rollback;
- manuali installazione/uso/manutenzione;
- acceptance bench finale;
- piano di prove incrementale separato.

**Exit:** tutti i gate precedenti soddisfatti. Non equivale automaticamente a
certificazione o approvazione al volo.

### Fase 8 — CM / Next Plus G7

Iniziare soltanto dopo G6, mantenendo replay e benchmark equivalenti sulla
piattaforma industriale scelta.

## 11. Priorità dei prossimi incrementi software

| Ordine | Incremento | Motivo | Dipendenza |
|---:|---|---|---|
| 1 | configuration/restore pack | riduce rischio di perdita o baseline ambigua | NAS/serverha |
| 2 | runbook HMI + SV per nodi — `PREPARED-SW / NOT-RUN` in `docs/deployment/HMI_SYNTHETIC_VISION_NODE_RUNBOOK.md` | prepara prove C12 senza improvvisazione | baseline `61070b8`; deployment e prove reali ancora da autorizzare |
| 3 | piano release gemella PFD/MFD | rende ripetibile il benchmark senza deployment implicito | runbook e rollback |
| 4 | mapping/calibration schema ASSO5 | abilita l'integrazione reale | inventario fisico |
| 5 | verticale G2 e replay | massimo valore tecnico immediato | G1 |
| 6 | air-data qualification tooling | evita dati apparentemente plausibili ma errati | banco M10/DLVR |
| 7 | blackbox endurance/recovery | riduce rischio single point | PSU/storage |
| 8 | NAV/TAWS fail-closed | completa il valore operativo del PFD/MFD | SV e dati reali |
| 9 | EMS reale e fonti controllate | porta il sistema oltre la simulazione | manuali/hardware |
| 10 | FPV e voce | valore elevato ma non devono ritardare G1-G5 | reti/hardware dedicati |

## 12. Decisioni ricevute e dati ancora richiesti

Decisioni ricevute il 2026-09-22:

1. Il nome prodotto è **FUSO AVIONICS NEXT**. `EFIS NEXT` resta alias tecnico
   fino a una futura migrazione controllata, senza rename automatico.
2. Il prodotto deve supportare **entrambi** i motori Rotax 912 UL e 912 ULS
   attraverso profili indipendenti.

Dati ancora richiesti; alcune dipendenze sono bloccanti:

1. **Motori:** targhetta, seriale e manuale/revisione applicabile per ogni
   esemplare che verrà configurato.
2. **Aereo:** fornire POH/AFM e revisione applicabile dell'ASSO 5 Jolly/Pioneer
   300 reale; la sola similarità non è sufficiente per limiti o emergenze.
3. **Sequent:** confermare modelli, revisioni, ordine stack e DIP delle quattro
   schede con fotografie.
4. **MGL:** fornire modello esatto, manuale, pinout e protocollo del dispositivo.
5. **Switch:** confermare se il MokerLink è già disponibile al banco e quale
   alimentatore verrà usato.
6. **Touch MFD:** confermare hardware USB e disponibilità per la sessione.
7. **FPV:** indicare priorità fra telecamere autocostruite e moduli industriali,
   risoluzione/FPS/visione notturna e vincoli di peso/alimentazione.
8. **Voce:** indicare lingua primaria, voce preferita, interfono/PTT e vincolo
   offline obbligatorio.
9. **Prodotto:** definire la prima configurazione da congelare: un solo
    esemplare ASSO 5 o già due profili Standard/Professional.

Restano inoltre non reperiti i cinque riferimenti richiesti dal Brief R0:

- `CLAUDE_CODE_ASSO5_SENSOR_NODE_HANDOFF_R0.md`;
- `AVIONIX_NEXT_ASSO5_EMS_Rotax912ULS_P4.docx`;
- `AVIONIX_NEXT_Linee_Prodotto_Documento_Riferimento_R0.docx`;
- `EFIS_NEXT_ALLINEAMENTO_CLAUDE.md`;
- `LSA_Rotax912ULS_Falken_IFM_Impianto_Elettrico_P0.pdf`.

Se esistono fuori dal repository vanno acquisiti, aperti, classificati e
registrati con hash; i loro contenuti non possono essere presunti.

## 13. Criteri di governo da mantenere

- una PR per obiettivo verificabile;
- niente merge senza CI e review del perimetro;
- niente deployment implicito dopo un merge;
- ogni deployment deve avere manifest, rollback e reboot acceptance;
- ogni test hardware deve congelare commit, configurazione, cablaggio e dati;
- nessun `DONE` da build o screenshot isolato;
- ogni stato hardware deve avere timestamp, nodo e digest dell'evidenza;
- nessuna funzione attiva di volo nella V1;
- `flight_approval=false` fino a un programma separato formalmente approvato.

## 14. Definizione del prossimo traguardo concreto

Il prossimo traguardo professionale non è una nuova pagina grafica. È una
dimostrazione completa, ripetibile e archiviata:

1. hardware sensori identificato;
2. alimentazione qualificata;
3. sorgente reale acquisita;
4. dato normalizzato dalla Blackbox;
5. visualizzazione coerente su PFD/EMS;
6. registrazione senza gap;
7. replay con stessi esiti;
8. fault e recovery verificati;
9. release e configurazione ricostruibili;
10. operatore che firma l'evidenza.

Il completamento di questa catena riduce più rischio di qualunque espansione
funzionale non accompagnata da prove.

## 15. Riferimenti principali

- `docs/status/EFIS_NEXT_STATE.md`
- `docs/status/EFIS_NEXT_MASTER_MATRIX.md`
- `docs/status/EFIS_NEXT_STATUS_GENERATED.md`
- `docs/status/EFIS_NEXT_ACTIVE_RELEASES.md`
- `docs/project/AVIONIX_NEXT_MASTER_PROJECT_BRIEF_CODEX_R0.md`
- `docs/project/AVIONIX_NEXT_EXECUTION_BACKLOG.md`
- `docs/project/AVIONIX_NEXT_DOCUMENT_INDEX.md`
- `docs/project/AVIONIX_NEXT_DECISION_REGISTER.md`
- `docs/status/CHECKPOINT-2026-09-21-SYNTHETIC-VISION-BENCHMARK.md`
- `docs/status/CHECKPOINT-2026-09-21-SYNTHETIC-VISION-SESSION.md`
- `docs/status/CHECKPOINT-2026-09-21-SYNTHETIC-VISION-CAPTURE-ASSEMBLER.md`
- PR #29 / C12, integrata in `main` al merge `61070b8d990c01ca58943a6370402623f4320ce7`.

---

Questo documento fotografa lo stato noto alla data indicata. Non autorizza
modifiche hardware, deployment, merge, prove dinamiche o uso in volo.
