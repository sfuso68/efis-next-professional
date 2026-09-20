# AVIONIX NEXT — stato di avanzamento rispetto al piano

Data di riferimento: 2026-09-20  
Repository autorevole: `sfuso68/efis-next-professional`  
Baseline integrata osservata: `origin/main` `67aee2e`  
Branch di lavoro osservato: `work/release-supervision-contract-v1` `8dc7895`  
Pull request aperta: PR #15, pronta al merge ma non ancora integrata in `main`  
Classificazione: DEVELOPMENT / NON-CERT / NON APPROVATO AL VOLO

## 1. Sintesi direzionale

AVIONIX NEXT dispone ormai di una base software e documentale consistente:
architettura distribuita definita, contratti dati congelati, HMI SkyView/EMS
verificata mediante test e golden multi-risoluzione, broker Blackbox operativo,
catena MAVLink read-only e strumenti di controllo progetto, evidence e release.

Il programma non è però ancora una piattaforma avionica integrata e validata.
La distanza principale dal piano non è oggi nella quantità di codice, ma in
quattro elementi critici:

1. identificazione e qualificazione dell'hardware reale, in particolare nodo
   sensori ASSO5, quattro schede Sequent, alimentazioni e cablaggi;
2. deployment ripetibile e immutabile su tutti i nodi;
3. prove end-to-end con sorgenti reali, fault injection, pannelli e touch;
4. chiusura delle funzioni di prodotto ancora parziali: synthetic vision,
   EMS reale, FPV/video, meteo, voce, emergenze e avionica accessoria.

La Master Matrix misura **44,7% di avanzamento ponderato** su **71 requisiti**:

| Stato requisito | Quantità | Significato operativo |
|---|---:|---|
| DONE | 3 | chiuso secondo il criterio disponibile |
| VERIFY-HW | 7 | software/documentazione presenti, prova reale ancora necessaria |
| PARTIAL | 43 | implementazione o integrazione incompleta |
| TODO | 14 | lavoro sostanziale non ancora implementato |
| BLOCKED | 2 | dipendenza esterna o hardware non risolta |
| LEGACY | 2 | mantenuto come riferimento, non parte della nuova baseline |

Il 44,7% non deve essere letto come “quasi metà prodotto pronto al volo”. È un
indice di copertura del programma: molte voci sono `PARTIAL` e solo sette
richiedono formalmente hardware, mentre numerose integrazioni parziali dovranno
comunque essere nuovamente provate sul banco e sui pannelli finali.

## 2. Stato dei gate del Master Project Brief

Le percentuali non vengono inventate per gate: il giudizio usa le evidenze e i
criteri di uscita definiti nel backlog. `PASS-SW` indica che il risultato è
ripetibile in software; non equivale ad acceptance hardware.

| Gate | Obiettivo pianificato | Stato attuale | Evidenza raggiunta | Cosa manca per chiuderlo |
|---|---|---|---|---|
| G0 — Project truth | autorità, fonti, naming, repository e baseline univoci | PARTIAL | repository Professional autorevole, brief, ADR, indice, matrix e generatori presenti | naming AVIONIX/EFIS, cinque fonti mancanti, decisione bare NAS duplicato e formalizzazione finale GitHub/NAS |
| G1 — Sensor node P0 | nodo ASSO5 identificato, quattro Sequent inventariate, acquisizione e soak | BLOCKED-HW / IN PROGRESS | RPi4 `.89` identificato; capture e contratti fail-closed pronti; kernel REAL/SIM implementato | Sequent 0/4, DIP, PSU, mapping 18 canali, calibrazioni, driver/publisher reali e soak 60 min |
| G2 — Data/replay | ICD congelato e verticale sensore→Blackbox→display riproducibile | PARTIAL; ICD ACCEPTED | ICD V1, test workstation e replay ARM64 su PFD/MFD/EMS PASS con digest coerenti | verticale reale G1→Blackbox→RPi5, trasporto upstream approvato, replay dello stesso dataset fisico e fault live |
| G3 — Integrated bench EFIS | HMI, SV, EMS e pannelli integrati al banco | PARTIAL / PASS-SW | contratti e golden HMI, stati EMS, responsive matrix, packager evidence e harness acceptance pronti | touch e pannelli reali, LOD/proiezione terreno, frame-rate/termica, MGL passivo e sorgenti motore reali |
| G4 — Installable electrical prototype | alimentazione, switch, harness, enclosure e schema filo-per-filo | PROPOSED / PREPARED-SW | architettura reti e candidato MokerLink documentati; acceptance switch preparata | identità/prova MokerLink, PSU qualificate, brownout/load test, cablaggi, protezioni, enclosure e review installativa |
| G5 — Dynamic tests | HIL, fault injection ed endurance integrati | PROPOSED | simulatori e diversi validator software disponibili | banco HIL completo, scenari di guasto reali, prove dinamiche, endurance e criteri di accettazione firmati |
| G6 — EFIS Next baseline | release controllata, configuration index, recovery e acceptance completa | IN PROGRESS, non raggiunto | contratto V1; MFD, EMS e Blackbox con manifest, rollback drill e reboot acceptance PASS | PFD/recorder, CI/evidence complessivi e chiusura G0–G5 |
| G7 — CM / Next Plus | porting su hardware industriale con parità verificata | NON AVVIATO | linee architetturali soltanto | baseline G6, scelta hardware, profilo prodotto, benchmark e replay equivalenti |

### Valutazione critica del percorso

- **Il gate più avanzato è G2 sul piano del contratto**, non sul piano del
  verticale reale: l'ICD è solido, ma il nodo sensori non alimenta ancora la
  catena accettata.
- **G3 ha elevata maturità grafica/software**, ma resta lontano dall'uscita di
  gate finché non vengono effettuate prove sui display reali e corretti i
  difetti del synthetic vision.
- **G1 e G4 sono il collo di bottiglia fisico.** Continuare ad ampliare funzioni
  applicative senza chiuderli aumenta il debito di integrazione.
- **La PR #15 è un abilitatore G6**, non una release: prepara il confezionamento
  e il preflight, ma non modifica né valida i nodi attivi.

## 3. Stato per workstream

| WS | Pianificazione | Stato reale dettagliato | Livello di verifica | Priorità |
|---|---|---|---|---|
| WS0 Governance | unica verità di progetto e controllo configurazione | brief, audit, ADR, matrix, stato e generatori presenti; repository Professional riconosciuto | DOCUMENTED / PARTIAL | P0 |
| WS1 Data contract | DDP/EDP e semantica comune | ICD congelato, vettori/digest/validator e ARM64 PASS; verticale fisico assente | VERIFIED-SW + ARM64 | P0 |
| WS2 ASSO5 sensor node | acquisizione CHT/EGT/pressioni/livelli e health | nodo noto; framework REAL/SIM e health presenti; nessuna scheda reale inventariata | IMPLEMENTED-SW / BLOCKED-HW | P0 |
| WS3 EFIS HMI | PFD, MFD, EMS coerenti e adattivi | design, golden, stati degradati e responsive matrix testati; acceptance pannelli non eseguita | VERIFIED-SW / VERIFY-HW | P0 |
| WS4 Synthetic vision e NAV | terreno, ostacoli, aeroporti, rotta e meteo | pipeline dati e rendering esistono; terreno vicino/LOD e qualità pianura non accettabili | PARTIAL | P1 |
| WS5 Air data e AHRS | Pixhawk/M10/DLVR con qualità e fallback | MAVLink read-only osservato; M10 e DLVR non qualificati/calibrati; sorgenti non arbitrate in modo completo | PARTIAL-HW | P0 |
| WS6 EMS Rotax/MGL | EMS completo, calibrazioni, log, ore, fuel/electrical | UI e simulatore avanzati; release immutabile RPi4, persistenza e EDP SIM verificati; CAN/MGL e sensori reali non chiusi | PARTIAL / DEPLOY VERIFIED-HW | P0 |
| WS7 Pixhawk/autopilot | integrazione osservativa senza autorità di volo | collegamento e fan-out read-only presenti; nessun comando a servi/autopilota autorizzato | PARTIAL-HW | P1 |
| WS8 Blackbox/server/laptop | broker, recording, servizi e manutenzione controllata | Blackbox su release immutabile verificata; server/NAS/GS presenti; autenticazione/lifecycle/endurance incompleti | PARTIAL | P0 |
| WS9 Electrical/enclosure | rete, PSU, protezioni, cablaggi ed enclosure | segmentazione definita; MokerLink candidato non verificato; storico undervoltage Blackbox | PROPOSED / VERIFY-HW | P0 |
| WS10 Quality/testing/safety | test, HIL, fault, safety case ed evidence | numerosi test fail-closed e packager; HIL e campagne reali non eseguite | STRONG-SW / WEAK-HW | P0 |
| WS11 Configuration management | release immutabili, manifest, rollback e tracciabilità | MFD/EMS/Blackbox conformi V1 con rollback/reboot; PFD e recorder ancora aperti | IN PROGRESS / VERIFIED-HW 3 NODES | P0 |
| WS12 Next Plus | piattaforma industrializzata successiva | non avviata; correttamente subordinata alla baseline G6 | TODO | P3 |

## 4. Stato dei nodi e delle infrastrutture

| Nodo | Ruolo previsto | Stato osservato | Conformità al piano | Azione necessaria |
|---|---|---|---|---|
| PFD RPi5 `.45` | PFD/SkyView principale | release `60b3526` confermata; recorder esterno alla release | PARTIAL | migrazione manifestata, acceptance touch/termica/visuale e rollback |
| MFD RPi5 `.49` | MFD/NAV/servizi display | `mfd-3432402-primary-v1`, systemd, rollback e reboot PASS | PARTIAL | acceptance pannello, touch, terrain e soak |
| EMS RPi4 `.36` | EMS dedicato | `ems-7655799-primary-v1`, rollback/reboot, runtime e EDP `LIVE • SIM` PASS | PARTIAL / DEPLOY CONFORME | sorgenti reali, calibrazioni, touch e soak |
| ASSO5-SENSOR RPi4 `.89` | acquisizione sensori motore | RPi4 8 GB identificato; Ethernet senza carrier; Sequent non identificate | BLOCKED-HW | fotografie/inventario, PSU, DIP, mapping, I²C controllato e driver |
| Blackbox RPi5 `.47` | ingestione, broker, recorder e distribuzione | `blackbox-7655799-primary-v1`, rollback/reboot e broker health PASS; engine SIM | PARTIAL / DEPLOY CONFORME | CAN reale, power, recording/recovery/endurance |
| Pixhawk 6X | sorgente flight data read-only | MAVLink osservato e distribuito; no authority | PARTIAL-HW | qualificare link, parametri, M10, airspeed e failure modes |
| Server `.6` | mappe, meteo, navdata e servizi | servizi disponibili e cache imagery usata | PARTIAL | versionare dataset, autenticazione, CA/lifecycle, restore e offline policy |
| Portatile `.39` | ground station monitor-only | monitor MAVLink e firewall provati; timing Wi-Fi degradato | PARTIAL | profilo di manutenzione controllato, test rete cablata e recovery |
| NAS `.22` | mirror/backup repository ed evidence | mirror usato e branch allineati durante lo sviluppo | PARTIAL | eliminare ambiguità fra bare repository e provare restore completo |
| VIDEO-CORE RPi4 | ingestione e compositing FPV separati | ruolo architetturale definito, nodo/prodotto non qualificato | TODO | inventario hardware, decoder, camere, latenza, registrazione e isolamento rete |

## 5. Avanzamento funzionale

La seguente vista usa i valori della Master Matrix, non una valutazione
estetica. Le aree con pochi requisiti possono mostrare una percentuale alta pur
restando dipendenti da verifiche critiche.

| Area | Avanzamento ponderato | Lettura corretta |
|---|---:|---|
| CORE | 81,2% | architettura dati solida; verticale reale ancora aperto |
| INST | 75,0% | strumenti implementati ma entrambi in VERIFY-HW |
| PWR | 75,0% | valore alto dovuto a un solo requisito; qualifica alimentazione non chiusa |
| PRJ | 62,5% | governance forte, ma documenti e hardware mancanti |
| GS | 58,3% | ground station funzionante in forma parziale e monitor-only |
| BBX, CFG, EMS, NAV, PFD, SRV, SV, WX | 50,0% | nucleo presente, integrazione/acceptance incompleta |
| AIR, PIX, VOI | 33,3% | prototipi o collegamenti iniziali; qualifica insufficiente |
| ESP | 25,0% | strumenti accessori ESP32-S3 non ancora portati a prodotto |
| PROD, VER | 20,0% | industrializzazione e verifica sono la parte più arretrata |
| FPV | 16,7% | architettura prevista, implementazione e hardware ancora iniziali |
| PAN | 12,5% | assistenza emergenze incompleta e funzioni attive bloccate per safety |

Le funzioni “panico” devono restare **solo advisory**: possono presentare
assetto, checklist, fraseologia e suggerimenti contestuali, ma non possono
comandare servi, autopilota, flap, carrello, radio o transponder. La conferma
vocale è un ausilio HMI, non una fonte di autorità.

## 6. Release engineering introdotto dalla PR #15

Il branch `work/release-supervision-contract-v1` aggiunge:

- schema del manifest immutabile di release;
- schema del piano di transizione per nodo;
- policy release per PFD, MFD, EMS e Blackbox;
- validatore e builder dei manifest;
- preflight non esecutivo della transizione;
- test e integrazione CI.

Verifica software iniziale: 19 test mirati, 18 PASS e 1 SKIP su Windows per una
prova symlink, coperta dalla CI Linux. La successiva finestra autorizzata ha
completato deployment, rollback drill e reboot su MFD, EMS e Blackbox. Il
rollout globale non è completo perché PFD/recorder restano fuori dal batch;
`PROD-005` resta quindi `PARTIAL`.

## 7. Scostamenti principali dal planning

### Scostamenti tecnici

1. Il lavoro HMI e contrattuale è avanzato più rapidamente della sensoristica
   e dell'impianto elettrico.
2. EMS, Blackbox e MFD condividono ora il modello V1; PFD/recorder devono
   ancora essere ricondotti allo stesso contratto.
3. Il synthetic vision ha dati e pipeline, ma non la qualità visiva e le
   prestazioni richieste dal concept professionale.
4. Il percorso FPV a quattro telecamere non ha ancora un VIDEO-CORE qualificato
   né una matrice camera/decoder/latency accettata.
5. Meteo, voce e fraseologia sono moduli promettenti ma non ancora validati per
   disponibilità, latenza, failure mode e uso offline.

### Scostamenti di processo

1. Diversi batch sono `VERIFIED-SW` ma non sono stati convertiti in evidence
   hardware firmata.
2. La nomenclatura AVIONIX NEXT / EFIS NEXT non è congelata.
3. Cinque documenti indicati dal brief non sono stati reperiti.
4. La strategia GitHub/NAS è operativa ma non completamente chiusa a livello
   di policy, restore e repository bare duplicato.
5. L'espansione funzionale ha superato la capacità di acceptance al banco;
   occorre ora privilegiare chiusure verificabili rispetto a nuove funzioni.

## 8. Rischi ordinati

| Priorità | Rischio | Effetto | Mitigazione immediata |
|---|---|---|---|
| P0 | hardware ASSO5 non identificato | blocca EMS reale e verticale G2 | inventario fotografico 4/4, PSU, DIP e channel map |
| P0 | alimentazione non qualificata | reset, corruzione dati o perdita display | PSU/cavi documentati e brownout/load test controllati |
| P0 | release attive non tracciate | impossibile riprodurre o ripristinare i nodi | integrare PR #15 dopo review, staging nodo per nodo e rollback |
| P0 | acceptance HMI reale assente | difetti touch/leggibilità/termica non rilevati | eseguire harness G3 su PFD, MFD ed EMS con evidence |
| P1 | terreno/LOD non accettabile | synthetic vision visivamente fuorviante | benchmark Treviso–Belluno, correzione proiezione/LOD e frame budget |
| P1 | dati reali M10/DLVR/EMS non qualificati | valori errati o fallback ambiguo | calibrazione, source arbitration e fault injection |
| P1 | Blackbox single point non qualificato | perdita distribuzione/log | watchdog, storage policy, endurance e recovery test |
| P2 | FPV/video non consolidato | latenza o saturazione rete | mantenere rete separata, qualificare decoder/camere e bandwidth |
| P2 | voce/meteo dipendenti da servizi | degrado in assenza rete | cache/offline, annunci deterministici e chiara marcatura disponibilità |

## 9. Piano operativo aggiornato

### Blocco A — chiusure P0 senza nuova espansione funzionale

1. Riesaminare e, se approvata, integrare la PR #15.
2. Congelare naming, policy GitHub/NAS e strategia dei due bare repository.
3. Acquisire l'inventario ASSO5 completo: quattro schede, revisioni, DIP,
   alimentatore, connettori e fotografie con hash.
4. Identificare i supervisor e i binari attivi di MFD e Blackbox.
5. Preparare manifest e piani di transizione per ogni nodo senza eseguirli.

### Blocco B — banco hardware e release controllata

6. Qualificare PSU e cavi; eseguire brownout/load test con criteri concordati.
7. Portare prima un nodo non critico al modello release immutabile e provare
   rollback; estendere poi PFD, MFD, EMS e Blackbox singolarmente.
8. Eseguire l'acceptance G3 sui tre pannelli con scenari LIVE/STALE/LOST/FAIL,
   touch, leggibilità, frame-rate e temperatura.
9. Completare driver, mapping e calibrazione del nodo ASSO5.
10. Eseguire il soak locale di 60 minuti e produrre evidence firmata.

### Blocco C — integrazione dati reale

11. Approvare il trasporto upstream del nodo sensori e il SourceId.
12. Eseguire il verticale reale sensore→Blackbox→PFD/EMS→record/replay.
13. Qualificare Pixhawk 6X, M10 e Matek DLVR con failure modes e fallback.
14. Integrare MGL in sola lettura quando pinout/protocollo saranno disponibili.
15. Eseguire HIL e fault injection su rete, sensori, storage e broker.

### Blocco D — qualità prodotto

16. Correggere synthetic vision e completare benchmark Treviso–Belluno/Dolomiti.
17. Qualificare server cartografico, cache offline, meteo/METAR/TAF e restore.
18. Costruire VIDEO-CORE separato e validare quattro camere, decoder e latenza.
19. Completare EMS, log ore, impianto elettrico, flap/carrello e database aerei
   mantenendo netta la distinzione tra dati simulati, derivati e reali.
20. Eseguire campagna G5, chiudere configuration index e candidare G6.

## 10. Prossimo criterio di successo

Il prossimo avanzamento significativo non è “aggiungere altre venti funzioni”.
È produrre un'evidenza completa e ripetibile su una catena reale:

`sensore reale → ASSO5-SENSOR → Blackbox → PFD/EMS → blackbox log → replay`

con identificazione dell'hardware, manifest delle release, fault controllati,
digest degli artefatti e nessuna ambiguità fra SIM e REAL. Questa prova chiude
il divario più importante fra l'attuale piattaforma software e il piano di un
sistema professionale.

## 11. Regole di dichiarazione dello stato

- `DONE` richiede il criterio della Master Matrix e l'evidenza prevista.
- `VERIFIED-SW` non equivale a verifica hardware né ad approvazione al volo.
- `VERIFY-HW` resta aperto fino alla prova sul nodo/pannello specificato.
- `ACTIVE` descrive un processo osservato, non una release conforme.
- `PASS` di un validator prova soltanto il contratto controllato dal validator.
- nessun modulo AVIONIX NEXT ha oggi autorità sugli organi di volo.

## 12. Fonti autorevoli

- `docs/status/EFIS_NEXT_STATE.md` — stato corrente sintetico;
- `docs/status/EFIS_NEXT_MASTER_MATRIX.md` — requisiti e criteri di chiusura;
- `docs/status/EFIS_NEXT_STATUS_GENERATED.md` — conteggi e avanzamento generati;
- `docs/status/EFIS_NEXT_ACTIVE_RELEASES.md` — release realmente osservate;
- `docs/project/AVIONIX_NEXT_EXECUTION_BACKLOG.md` — attività per gate;
- `docs/project/AVIONIX_NEXT_MASTER_PROJECT_BRIEF_CODEX_R0.md` — piano R0;
- ADR accettate e bundle evidence richiamati dai documenti precedenti.

Questo rapporto fotografa la situazione al commit del branch indicato. Le
modifiche della PR #15 diventano baseline soltanto dopo merge verificato in
`main`; l'esistenza del documento non autorizza deployment o prove hardware.
