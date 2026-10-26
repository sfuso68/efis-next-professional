# AVIONIX NEXT — backlog esecutivo per gate

R1-19/AVX-G5-02,2026-10-26: live fisico verso PFD verificato su banco con
nonce e timeout, non più solo replay. Stati IN_PROGRESS invariati. Priorità:
1) accesso SIL dedicato serverha→PFD, previa autorizzazione, eliminare relay
Windows;2) producer continuo/fanout e pilotaggio virtuale;3) EDP/airframe/
meteo e NAV sui consumer reali;4) nominali/fault/endurance multi-display,
sempre subordinati a power/golden e senza comandi hardware. Touch PFD aperto.
Prove:57 Windows/67 serverha/10 ARM64; live60s/1201frame e restore reali;
fallimento deadline intermedio conservato. Non confondere con prodotto finito.

Incremento AVX-G5-02 R1-18 del2026-10-26: deadline --realtime controllata;
51 test Windows/59 serverha PASS, motore fisico C172P eseguito60s con vento,
virata e fault GNSS/radio/meteo. Replay sul PFD.45 con restore, non nuova
release. Ancora IN_PROGRESS: stream live PFD/MFD/EMS, controlli pilota,
EDP/airframe, meteo nella GUI, modelli reali validati e gate power/golden.
Touch kernel11 coppie PASS, MENU FAIL segnalato; non acceptance HMI.

Aggiornamento: 2026-10-24 (aggiunta requisito simulatore realistico; storico conservato)
Stato: controllato da WS0/G0; nessuna attività hardware autorizzata da questo documento.

Gli stati ammessi qui sono `PROPOSED`, `OPEN`, `BLOCKED`, `IN_PROGRESS` e
`ACCEPTED`. La chiusura funzionale resta regolata dalla Master Matrix.

| ID | Gate | Descrizione | Input | Output | Dipendenze | Rischio | Accettazione | File previsti | Test | Stima | Stato |
|---|---|---|---|---|---|---|---|---|---|---|---|
| AVX-G0-01 | G0 | Importare brief e indice nel repo autorevole | Brief R0 storico, audit | baseline documentale tracciata | nessuna | basso | brief integro e regole di autorità esplicite | `docs/project/*` | hash/confronto, link check | S | ACCEPTED |
| AVX-G0-02 | G0 | Formalizzare GitHub/NAS e divergenze | refs Git/NAS | registro repository | accesso NAS | medio | GitHub, serverha, Blackbox e NAS coerenti | ADR-018, indice, stato | quattro `rev-parse` read-only sullo stesso hash | S | ACCEPTED |
| AVX-G0-03 | G0 | Decidere il bare NAS duplicato | inventario NAS | decisione conservazione | autorizzazione utente | medio | nessuna copia ambigua | ADR-018, indice | autorità/retention esplicite; nessuna cancellazione | S | ACCEPTED |
| AVX-G0-04 | G0 | Recuperare cinque documenti mancanti | file del proprietario | indice con hash | disponibilità file | alto | 5/5 acquisiti e classificati | `docs/reference/` o registro esterno | hash e apertura | M | BLOCKED |
| AVX-G0-05 | G0 | Congelare nomenclatura prodotto/alias | decisione proprietario 2026-09-22 | `FUSO AVIONICS NEXT` prodotto; `EFIS NEXT` alias tecnico | migrazione tecnica futura | basso | nomi e alias approvati, nessun rename automatico | decision register e futuro brief R1 | controllo riferimenti | S | ACCEPTED |
| AVX-G1-01 | G1 | Identificare nodo ASSO5 dedicato | RPi4 reale | identità OS/IP/power/storage | hardware | alto | inventario firmato | inventario HW | comandi read-only | S | IN_PROGRESS |
| AVX-G1-02 | G1 | Inventariare Sequent e DIP | quattro schede | modello/revisione/ID/foto | AVX-G1-01 | alto | 4/4 identificate | inventario HW | I2C/CLI read-only | M | OPEN |
| AVX-G1-03 | G1 | Implementare adapter real/SIM e health | inventario e ICD | servizio acquisizione | AVX-G1-02, G2 schema | medio | open/short/timeout e recovery testati | core/adapter/deploy | unit/component | L | IN_PROGRESS |
| AVX-G1-04 | G1 | Soak nodo sensori | servizio completo | report 60 minuti | AVX-G1-03 | alto | zero errori non gestiti e log locale continuo | evidence | soak/fault rete | M | PROPOSED |
| AVX-G2-01 | G2 | Congelare ICD AVIONIX sopra DDP/EDP | contratti esistenti | schema, freshness, naming | G0 | medio | compatibilità ARM64/x86-64 | docs/protocol, test vectors | schema/replay | M | ACCEPTED |
| AVX-G2-02 | G2 | Verticale sensore→RPi4→Blackbox→RPi5→replay | G1 e ICD | catena completa riproducibile | AVX-G1-03, AVX-G2-01 | alto | stesso dataset e stessi esiti | adapter/broker/replay | integration/HIL | L | IN_PROGRESS |
| AVX-G3-01 | G3 | Correggere LOD/proiezione SV | renderer e dataset | qualità primo piano accettabile | benchmark/dataset | medio | acceptance visuale e frame-rate | renderer/terrain | C9 contract; C10 session; C11 capture assembler; C12 footprint LOD VERIFIED-SW/VERIFY-HW; screenshot/replay/HW | L | OPEN |
| AVX-G3-02 | G3 | Integrare MGL RS-232 passivo | MGL, pinout, protocollo | parser e confronto EMS | documenti mancanti | alto | MGL indipendente, timeout corretti | adapter/test | parser/HW | L | BLOCKED |
| AVX-G3-03 | G3 | Acceptance touch PFD/MFD/EMS | display reali | checklist operatore | touch disponibile | medio | nessuna sovrapposizione e fail-state leggibili | evidence/status | touch/scenari | M | OPEN |
| AVX-G3-04 | G3 | Congelare contratto HMI SkyView/EMS | concept approvato e design esistente | manifesto macchina, 10 gate, 20 controlli fail-closed e test | G0 | medio | stessa semantica a tutte le risoluzioni | config/ui, docs/design, tools/ui | validator/golden/CI/HW | M | IN_PROGRESS |
| AVX-G4-01 | G4 | PSU, harness, enclosure e schema filo-per-filo | inventario finale | prototipo installabile | G1/G3 | alto | design review e brownout test | hardware docs | electrical/environmental | XL | PROPOSED |
| AVX-G5-01 | G5 | HIL e fault injection integrati | G3/G4 | matrice prove ripetibile | gate precedenti | molto alto | fault obbligatori con evidenza | tests/evidence | HIL | XL | PROPOSED |
| AVX-G5-02 | G5 | Simulatore realistico integrato e archivio dati sul server | richiesta proprietario 2026-10-24, modelli e dataset tracciabili | ambiente SIL ripetibile per tutte le applicazioni, bridge simulatori opzionale e pacchetti offline | contratti DDP/EDP/airframe, fonti/licenze, profili validati | alto: coerenza fisica, falsi dati normali, licenze, carico | scenari nominali/fault end-to-end, correlazioni e tolleranze per modello verificate, riproducibilità, nessun comando hardware | simulator/providers, strumenti ingestione, server/catalogo, tests/evidence; design prima del codice | SIL/replay/contract e confronto con riferimenti, HIL separato | XL | IN_PROGRESS |
| AVX-G6-01 | G6 | Congelare baseline EFIS Next | G0–G5 | release/config/recovery controllati | tutti i gate | alto | configuration index e rollback | release docs | acceptance completa | L | PROPOSED |
| AVX-G7-01 | G7 | Porting CM/Next Plus | baseline G6 | parità funzionale su hardware industriale | G6 | alto | replay e benchmark equivalenti | nuovo profilo HW | benchmark/replay | XL | PROPOSED |

## Prossima azione autorizzabile

Dopo il merge documentale WS0/G0, la prima attività tecnica resta
AVX-G1-01/02 in sola ricognizione: identificazione del nodo RPi4 ASSO5 e delle
quattro Sequent. Non include cablaggi, scritture EEPROM/DIP, installazioni o
comandi su uscite.

## Registro avanzamenti successivi alla baseline

| Data | Gate/attività | Evidenza | Esito e limite |
|---|---|---|---|
| 2026-09-17 | AVX-G3-04 | `13d8687`…`caf1b3e`, golden/HMI/EDP diagnostics | IN_PROGRESS; software e CI verificati, pannelli/touch aperti |
| 2026-09-17 | AVX-G3-04 | `AVIONIX_G3_HMI_BATCH20.md`, HMI-B20-01…20 e 17 test | IMPLEMENTED-SW; contratto chiuso in CTest, SkyView golden e acceptance hardware aperti |
| 2026-09-17 | AVX-G3-04 | `AVIONIX_G3_SKYVIEW_GOLDEN_BATCH20.md`, HMI-G20-01…20, bundle EMS/SkyView V2 | VERIFIED-SW; matrice SkyView inclusa Full HD, criticità meteo 1024×600 e acceptance hardware aperte |
| 2026-09-18 | AVX-G3-04 | `AVIONIX_G3_SKYVIEW_RESPONSIVE_BATCH20.md`, HMI-R20-01…20 e golden aggiornate | VERIFIED-SW; overlay meteo separato dalla heading ribbon, acceptance hardware aperta |
| 2026-09-18 | AVX-G3-04 | `AVIONIX_G3_EMS_STATE_BATCH20.md`, HMI-E20-01…20 e sei golden EMS 800×480 | VERIFIED-SW; stati degradati senza verde normale e funzioni derivate inibite in LOST/FAIL; acceptance hardware aperta |
| 2026-09-18 | AVX-G3-04 | `AVIONIX_G3_EMS_RESPONSIVE_BATCH20.md`, HMI-M20-01…20 e matrice EMS 6×3 | VERIFIED-SW; semantica dei sei stati verificata a 800×480, 1024×600 e 1280×800; acceptance hardware aperta |
| 2026-09-18 | AVX-G3-03 | `AVIONIX_G3_HMI_HARDWARE_ACCEPTANCE_BATCH20.md`, HMI-A20-01…20 | PREPARED-SW/NOT_RUN; harness fail-closed pronto, nessuna prova su pannelli e attività resta OPEN |
| 2026-09-18 | AVX-G3-03 | `AVIONIX_G3_HMI_EVIDENCE_PACKAGER_BATCH20.md`, HMI-P20-01…20 | VERIFIED-SW/NOT_RUN; packager atomico con digest e divieto di flight approval, prove reali ancora aperte |
| 2026-09-17 | AVX-G2-01 | `AVIONIX_DATA_ICD_V1.md`, specifica JSON, schema evidence bundle, due vettori e validator indipendente | FROZEN-SW; test workstation PASS, replay ARM64 e hardware aperti |
| 2026-09-17 | AVX-G2-01 | `CHECKPOINT-2026-09-17-G2-ARM64-PENDING.md` | verifica ARM64 autorizzata ma non eseguita; accesso SSH workstation rifiutato, zero evidenza hardware |
| 2026-09-17 | AVX-G2-02 | EDP SIM Blackbox→EMS già operativo; replay deterministico disponibile | resta PROPOSED per il verticale reale, dipendente da G1 e CAN reale |
| 2026-09-18 | AVX-G2-01 | `CHECKPOINT-2026-09-18-G2-ARM64-VERIFIED.md` e bundle consolidato | ACCEPTED; PFD/MFD RPi5 ed EMS RPi4 ARM64: validator e 11/11 test PASS, digest congelati coincidenti; verticale live escluso |
| 2026-09-18 | AVX-G2-02 | `AVIONIX_G2_VERTICAL_ACCEPTANCE_BATCH20.md`, contratto e validator G2-V20 | PREPARED-SW/NOT-RUN; 20/20 controlli e 10/10 test PASS; trasporto RPi4→Blackbox UNRESOLVED e NO-GO fino ad AVX-G1-01/02/03 e nuova approvazione |
| 2026-09-18 | AVX-G1-01/02 | `AVIONIX_G1_SENSOR_DISCOVERY_BATCH20.md`, contratto e validator G1-D20 | PREPARED-SW/NOT-RUN; ricognizione fail-closed definita, identità nodo 0/1 e schede 0/4; nessuna esecuzione hardware o scansione I²C autorizzata |
| 2026-09-18 | AVX-G1-01 | `CHECKPOINT-2026-09-18-G1-NODE-IDENTITY-HW.md` e node report | HW-PARTIAL; `ems-sensor` `.89`, RPi4 8 GB Bookworm aarch64 e bus `i2c-1` verificati; alimentatore ignoto, Ethernet senza carrier e Sequent 0/4 |
| 2026-09-18 | AVX-G1-01 | `AVIONIX_G1_NODE_CAPTURE_BATCH10.md`, acquisitore e test | VERIFIED-RPI4/NO-GO; report ripetibile 10/10 PASS sul nodo, senza probing; board inventory e power ancora mancanti |
| 2026-09-18 | AVX-G1-02 | `AVIONIX_G1_PHYSICAL_INVENTORY_BATCH10.md`, manifest e validator G1-I10 | IMPLEMENTED-SW/NOT-CAPTURED/NO-GO; 10/10 controlli e 8/8 test PASS; quattro slot e alimentatore pronti per evidenza visuale, schede reali ancora 0/4 |
| 2026-09-18 | AVX-G1-02/03, AVX-G2-02 | `AVIONIX_G1_G2_READINESS_BATCH100.md`, contratto e validator R100 | IMPLEMENTED-SW/PREPARED-NOT-RUN/NO-GO; 100/100 controlli validati, 100 mutazioni fail-closed e 8/8 test PASS; hardware 0/4, mapping/calibrazione/SourceId/trasporto upstream ancora irrisolti |
| 2026-09-18 | AVX-G1-03 | `AVIONIX_G1_ADAPTER_HEALTH_BATCH20.md`, kernel C++ e 20 test | IMPLEMENTED-SW/HW-NOT-RUN; REAL/SIM, sequence, freshness, fault e recovery implementati senza driver, channel map o publisher |
| 2026-09-19 | AVX-G1-02 | `AVIONIX_G1_VISUAL_INVENTORY_INTAKE.md`, template intake, importatore e 9 test | IMPLEMENTED-SW/AWAITING-EVIDENCE/NO-GO; pipeline per 9 foto con SHA-256 e anti-overwrite pronta, ma schede reali ancora 0/4 |
| 2026-09-19 | AVX-G1-03/AVX-G2-02 | `AVIONIX_G1_UPSTREAM_PROPOSAL_BATCH20.md`, ADR-016, contratto e validator G1-U20 | PROPOSED-SW/NOT-APPROVED/NO-GO; ERAP V1/UDP 49004 e SourceId 9 riservati senza modificare runtime/ICD; switch 5 porte bloccante per requisito ≥7 |
| 2026-09-19 | AVX-G1-03/AVX-G4-01 | `CHECKPOINT-2026-09-19-G1-SWITCH-CANDIDATE.md`, contratto G1 V1.1 e test fail-closed | DOCUMENTED-CANDIDATE/NOT-VERIFIED/NO-GO; MokerLink `IND-F082GS` presunto con 9 porte rame dichiarate e margine numerico +2, ma Waveshare resta installato e servono identità più 9 evidenze hardware; VIDEO separato invariato |
| 2026-09-19 | AVX-G1-03/AVX-G4-01 | `AVIONIX_G1_SWITCH_ACCEPTANCE_BATCH20.md`, contratto/risultati/validator G1-S20 | PREPARED-SW/NOT_RUN/NO-GO; 20 controlli fail-closed e dieci evidenze definite, bundle sintetico ammesso solo per test; nessuna alimentazione, rete o prova hardware eseguita |
| 2026-09-20 | AVX-G0-02/AVX-G6-01 | inventario read-only release e `docs/status/EFIS_NEXT_ACTIVE_RELEASES.md` | PARTIAL; PFD `60b3526` e MFD `3432402` osservati, EMS da `build/dev`, Blackbox senza manifest e MFD con supervisore ignoto |
| 2026-09-20 | AVX-G6-01 | schema manifest/piano transizione, policy nodi, builder, validator e preflight nella PR #15 | IMPLEMENTED-SW/NOT-DEPLOYED; 19 test mirati, 18 PASS e 1 SKIP Windows symlink; CI Linux PASS, nessun nodo modificato |
| 2026-09-20 | AVX-G6-01 / PROD-005 | `release-immutability-20260920` e registro release | VERIFIED-HW su MFD/EMS/Blackbox: manifest, eseguibile canonico, rollback drill e reboot PASS; EMS EDP `LIVE • SIM` e runtime persistente; PFD/recorder restano aperti |
| 2026-09-20 | Program control | `AVIONIX_NEXT_PROGRESS_VS_PLAN_2026-09-20.md` | DOCUMENTED; 44,7% ponderato su 71 requisiti, gate/workstream/nodi confrontati con il piano; nessuna promozione di evidenza software a hardware |
| 2026-09-21 | AVX-G0-02/03 | ADR-018, GitHub/serverha/Blackbox/NAS e timer replica | ACCEPTED; quattro `main` coincidenti a `5891f754ee06948349f711c57c90e8700137d09f`, ruoli e retention espliciti; restore drill e ritiro copia Blackbox restano attività separate |
| 2026-09-21 | AVX-G3-03 / PRJ-004 | `CHECKPOINT-2026-09-21-HMI-HARDWARE-SESSION.md`, session manager e 11 test mirati | VERIFIED-SW/NOT_RUN; pipeline fail-closed init→finalize→evidence completa, ma pannelli, touch, soak, replay, fault e firma operatore restano hardware aperto |
| 2026-09-21 | AVX-G3-01 / SV-001 | `CHECKPOINT-2026-09-21-SYNTHETIC-VISION-BENCHMARK.md`, contratto e validatore C9 | PREPARED-SW/NOT_RUN; rotta, checkpoint, soglie, fault e evidence congelati, ma renderer e pannelli non eseguiti |
| 2026-09-21 | AVX-G3-01 / SV-001 | `CHECKPOINT-2026-09-21-SYNTHETIC-VISION-SESSION.md`, session manager C10 | VERIFIED-SW/NOT_RUN; bundle PFD+MFD legato a commit/contratto e finalizzazione fail-closed pronti, ma nessun renderer o pannello eseguito |
| 2026-09-21 | AVX-G3-01 / SV-001 | `CHECKPOINT-2026-09-21-SYNTHETIC-VISION-CAPTURE-ASSEMBLER.md`, assembler C11 | VERIFIED-SW/NOT_RUN; log continui, tile/fault, screenshot e firma producono risultati C9 atomici; 17 test C9+C10+C11 PASS, nessuna acquisizione RPi5 |
| 2026-09-22 | AVX-G3-01 / SV-001 | `CHECKPOINT-2026-09-22-SYNTHETIC-VISION-LOD-PROJECTION.md`, correzione C12 | VERIFIED-SW/VERIFY-HW; distanza dall'impronta, isteresi e preload centralizzati; build Qt/QML AOT e 4/4 suite C9–C12 PASS; nessun deployment o acceptance RPi5 |
| 2026-09-25 | AVX-G0-02 / AVX-G6-01 / PRJ-001 | configuration/restore pack I-10, tool fail-closed, runbook e sei test | PREPARED-SW/NOT-RUN; clone isolato, fsck, confronto SHA, tag/oggetti, file minimi e Project Control automatizzati; NAS/serverha non contattati e restore reale ancora aperto |
| 2026-09-27 | AVX-G3-01 / AVX-G3-03 / SV-001 | piano release gemella PFD/MFD C12, schema, coordinatore e otto test | PREPARED-SW/NOT-RUN/NO-GO; stesso commit e policy obbligatori, MFD prima del PFD, rollback inverso e digest input; nessun nodo contattato e deployment ancora soggetto ad autorizzazione separata |
| 2026-09-27 | AVX-G1-02/03, AVX-G2-02, AIR-001, BBX-001, NAV-003, SV-003 | `AVIONIX_QUALIFICATION_BATCH_G1_G4_V1.md`, contratto, validatore e dieci test | PREPARED-SW/NOT-RUN/NO-GO; cinque pacchetti e 50 controlli fail-closed definiscono dipendenze/evidenze senza inventare mapping o calibrazioni e senza autorizzare hardware, fault distruttivi o uso in volo |
| 2026-09-27 | EMS-004, PROD-001, FPV-002, VOI-003, VER-002 | `AVIONIX_PRODUCT_CONTRACTS_BATCH_V1.md`, contratto, validatore e test fail-closed | PREPARED-SW/NOT-RUN/NO-GO; cinque pacchetti e 50 controlli congelano autorità limiti EMS, SKU, FPV, voce e HIL; nessuna fonte motore, capability, periferica o fixture viene dichiarata verificata |
| 2026-09-27 | EMS-004, PROD-001, FPV-002, VOI-003, VER-002 | `AVIONIX_PRODUCT_TOOLS_BATCH_V1.md`, cinque registri, validatore e test mutazione | IMPLEMENTED-SW/NOT-RUN/NO-GO; fonti motore, SKU, capacità FPV, pacchetti voce e scenari HIL sono ora acquisibili senza poter dichiarare valori o misure non revisionati |
| 2026-09-27 | EMS-004, PROD-001, FPV-002, VOI-003, VER-002 | `AVIONIX_PRODUCT_EVIDENCE_BATCH_V1.md`, cinque intake legati ai registri e otto test | PREPARED-SW/NOT-RUN/NO-GO; artefatti e firme possono essere raccolti con provenienza congelata, ma nessuna evidenza o approvazione è stata acquisita |
| 2026-09-27 | EMS-004, PROD-001, FPV-002, VOI-003, VER-002 | `AVIONIX_PRODUCT_REVIEW_GATES_V1.md`, cinque gate legati agli intake e nove test | PREPARED-SW/NOT-RUN/NO-GO; ruoli e finding obbligatori sono congelati, ma bundle, reviewer, hardware, deployment e volo restano non approvati |
| 2026-09-27 | EMS-004, PROD-001, FPV-002, VOI-003, VER-002 | `AVIONIX_PRODUCT_EVIDENCE_PACKAGERS_V1.md`, cinque template e packager comune | IMPLEMENTED-SW/NOT-RUN/NO-GO; artefatti operator-confirmed possono essere verificati e congelati con SHA-256, ma i bundle restano pending review e non autorizzano hardware, deployment o volo |
| 2026-09-27 | EMS-004, PROD-001, FPV-002, VOI-003, VER-002 | `AVIONIX_PRODUCT_INDEPENDENT_REVIEW_V1.md`, schema bundle, validatori e cinque template review | IMPLEMENTED-SW/NOT-RUN/NO-GO; integrità byte-per-byte, tre ruoli distinti, firme e finding sono verificabili, ma anche un esito positivo passa soltanto al gate successivo |
| 2026-09-27 | EMS-004, PROD-001, FPV-002, VOI-003, VER-002 | `AVIONIX_PRODUCT_EVIDENCE_SESSION_V1.md`, session manager e test end-to-end | IMPLEMENTED-SW/NOT-RUN/NO-GO; init, status, packaging, apertura review e outcome immutabile sono guidati, ma acquisizione e decisione del gate restano interattive |
| 2026-09-28 | EMS-004, PROD-001, FPV-002, VOI-003, VER-002 | `AVIONIX_PRODUCT_GATE_DISPOSITION_V1.md`, cinque template, autorità separate e record immutabile | IMPLEMENTED-SW/NOT-RUN/NO-GO; review e gate sono collegati con digest, ma l'accettazione consente soltanto una futura richiesta change-control separata |
| 2026-09-28 | EMS-004, PROD-001, FPV-002, VOI-003, VER-002 | `AVIONIX_PRODUCT_CHANGE_CONTROL_REQUEST_V1.md`, cinque template, impact analysis e record immutabile | IMPLEMENTED-SW/NOT-RUN/NO-GO; una disposizione positiva può alimentare l'intake, ma il record termina prima della decisione CCB e non autorizza implementazione, deployment o volo |
| 2026-09-28 | EMS-004, PROD-001, FPV-002, VOI-003, VER-002 | `AVIONIX_PRODUCT_CCB_DECISION_V1.md`, cinque template, quorum unanime e record immutabile | IMPLEMENTED-SW/NOT-RUN/NO-GO; approve/reject/defer sono verificabili, ma anche l'approvazione consente soltanto un futuro piano di implementazione separato |
| 2026-09-29 | EMS-004, PROD-001, FPV-002, VOI-003, VER-002 | `AVIONIX_PRODUCT_IMPLEMENTATION_PLAN_V1.md`, cinque template, baseline/work package e record immutabile | IMPLEMENTED-SW/NOT-RUN/NO-GO; il piano è verificabile ma termina prima dell'autorizzazione a implementare e non consente deployment o volo |
| 2026-09-29 | AVX-G1-01/02 | `CHECKPOINT-2026-09-29-G1-PARTIAL-VISUAL-INTAKE.md` e capture nodo | HW-PARTIAL/VISUAL-INTAKE-INCOMPLETE/NO-GO; nodo `.89` riconfermato read-only con 10/10 PASS e quattro modelli frontali coerenti, ma retro, label alimentatore, operatore/UTC e review DIP mancanti; manifest ancora 0/4 |
| 2026-09-29 | AVX-G0-02 / AVX-G6-01 / PRJ-001 | `CHECKPOINT-2026-09-29-NAS-RESTORE-ACCESS-GATE.md` e report I-10 | VERIFIED-HOST/PASS; chiave read-only confinata a `git-upload-pack`, isolamento shell verificato e restore NAS→serverha a `7aa45b66fc71824eb0dd4b1e6ffe898ecfcdb897` con fsck/file/oggetti/Project Control PASS; protezione fuori sito resta aperta |
| 2026-09-29 | PFD-001, MFD-001, EMS-002, BBX-003, SRV-001, PWR-001 | `evidence/five-node-health-audit-20260929/README.md` | MIXED/ACTION-REQUIRED; PFD display attivo ma storage 100%, recorder 101.015 restart e imagery ENOSPC; EMS registry corretto a `62ba90b`; MFD/Blackbox con undervoltage recenti; serverha/NAS integri |
| 2026-09-29 | PFD-001, BBX-001, BBX-003, NAV-003, PWR-001 | `evidence/pfd-storage-recovery-blackbox-power-20260929/README.md` | PFD-RECOVERY-PASS/BLACKBOX-VERIFY-HW; 49.597 file legacy copiati e verificati checksum su serverha, root 81%, recorder `NRestarts=0`, imagery PASS; Blackbox broker stabile ma transienti alimentazione aperti e `bbxnav` a 157.031 restart senza ricevitori |
| 2026-09-29 | NAV-003, BBX-003 | `evidence/bbxnav-fail-closed-20260929/README.md` | VERIFIED-SW+HW/PARTIAL; drop-in supervision-only installato sulla Blackbox, exit 2 senza ricevitori non riparte (`NRestarts=0`), broker e bbxlog invariati; integrazione release e catena NAV reale restano aperte |
| 2026-09-30 | NAV-003, BBX-003, PROD-005 | `evidence/blackbox-nav-immutable-preparation-20260930/README.md` | PREPARED-SW/NOT-DEPLOYED; candidata Blackbox fissata a commit/SHA, broker e drop-in NAV nello stesso manifesto, rollback congiunto e reboot gate predisposti; build/attivazione ARM64 e catena NAV reale restano aperte |

Il freeze documentale AVX-G2-01 è stato autorizzato il 2026-09-17 e prodotto
senza modificare wire o runtime. Il replay degli stessi vettori è stato
completato il 2026-09-18 sui tre target ARM64 autorizzati e AVX-G2-01 è
`ACCEPTED`; AVX-G2-02 non è autorizzato da questo freeze e resta separato.

Il 2026-09-18 il proprietario ha autorizzato il batch software/documentale di
venti controlli per AVX-G2-02. L'attività è ora `IN_PROGRESS`, ma il contratto
impone `NOT_RUN`: nessuna prova live o modifica hardware è autorizzata finché
le dipendenze G1 non sono chiuse e il runbook non riceve approvazione separata.

Il batch successivo ha preparato i venti controlli della sola ricognizione G1.
`AVX-G1-01` resta `IN_PROGRESS` e `AVX-G1-02` resta `OPEN`: i contratti non
valgono come inventario fisico. Il nodo è stato acquisito senza probing, mentre
le fotografie delle schede e dell'alimentatore richiedono ancora una finestra
separata con il proprietario presente.

Il kernel health porta `AVX-G1-03` a `IN_PROGRESS` solo sul piano software. La
dipendenza dall'inventario resta effettiva: il codice non contiene accesso bus,
conversioni, mapping Sequent o endpoint di rete e non abilita la prova G2 live.

Il readiness batch R100 rende espliciti e verificabili i cinque confini che
precedono l'integrazione: inventario, 18 segnali, calibrazione, trasporto ed
evidence. I 49 controlli software in PASS non promuovono i 51 controlli
PREPARED/BLOCKED e non autorizzano probing o deployment.

## Nota vincolante di planning — simulazione realistica e archivio server

Richiesta del proprietario del 2026-10-24, AVX-G5-02. **IN_PROGRESS / PARZIALE**:
correzione consumo R1-11 e base autonoma R1-12 verificate solo software, non simulatore
realistico completato. La simulazione deve diventare un ambiente coerente di prova
dell'intero sistema, non una collezione di numeri animati indipendenti. Si mira
a utilizzare le applicazioni come durante una missione, senza dichiarare che
un simulatore equivalga a un aeroplano reale o sostituisca la qualifica hardware.

### Fedeltà richiesta

- Un unico tempo di simulazione e uno scenario comune per PFD, MFD/Canvas,
  EMS, navigazione, meteo, allarmi/emergenze, voce, FPV dove pertinente e blackbox.
  Assetto, traiettoria, accelerazioni, IAS/TAS/GS, vento, pressione/QNH, quota,
  VS, GNSS e sensori devono essere correlati, non generati separatamente.
- Modello configurabile per aeromobile/motore: massa e centraggio, aerodinamica
  e prestazioni, inviluppo e limiti, motore/termica, consumi/serbatoi,
  elettrico/utenze, carrello e flap con comando distinto da feedback. I limiti
  devono riferirsi alla variante effettiva, non ereditati da un modello simile.
- Fault ripetibili: open/short, pitot/statico, bias/stasi/disaccordo sensori,
  GNSS perso, degrado EDP/DDP, perdita/ritardo/reordine rete, guasti motore,
  elettrico e airframe. Conservare validità, età e provenienza per segnale.
- Scenari seedati/versionati, avvio/pausa/seek coerenti e replay deterministico;
  stessi ingressi alle interfacce di produzione, catture e log confrontabili.
  Tutto rimane marcato SIM; rete di test isolata e nessuna autorità su servi,
  Pixhawk o altri attuatori reali. Un bridge esterno non può mascherarsi LIVE.

### Modelli e fonti da acquisire, non ancora acquisiti

Valutare Microsoft Flight Simulator 2024, altri simulatori e modelli aperti
come possibili sorgenti/bridge di stato e dati prestazionali. Verificare prima
SDK/interfacce ufficiali, disponibilità dei campi, unità, frequenze, licenze e
diritti di esportazione/archiviazione per ciascun contenuto. Non assumere che
scenari, modelli commerciali, navdata o streaming del simulatore siano copiabili
nel nostro server. Se non esportabili, usare il bridge autorizzato o dati
alternativi con licenza compatibile: niente estrazione non autorizzata.

Priorità ai manuali POH/AFM e del costruttore motore per limiti e prestazioni;
dati del simulatore sono supplementari, non automaticamente validati o
certificati. Catalogare fonte/revisione/variante/unità/licenza, confidenza e
campi mancanti; nessun valore inventato classificato come dato verificato.

### Archivio geografico e operativo comune

Serverha ospita catalogo, ingestione controllata e pacchetti versionati; NAS
conserva le copie secondo la pipeline di backup/restore prevista. Archiviare
modelli, configurazioni, scenari e dataset consentiti: DEM/terreno, ortofoto e
texture, ostacoli, aeroporti/piste, radioassistenze/frequenze, spazi aerei,
cartografia, meteo storico/scenari e piani di volo. Priorità Nord Italia,
Treviso–Belluno e Dolomiti, senza limitare il formato a questa regione.

Ogni pacchetto deve dichiarare copertura geografica, livello di dettaglio,
CRS/datum verticale, unità, data di osservazione/validità e aggiornamento,
fonte/licenza, dimensioni, checksum e compatibilità. Mappe e terreno devono
allinearsi a coordinate, pista, ostacoli e quota dello scenario. Un inventario
scaricato non dimostra completezza, attualità o idoneità aeronautica.

Distribuzione ai nodi per zona e corridoio di piano di volo, con livelli di
dettaglio, quote/cache, ripresa download, verifica integrità, aggiornamento
atomico e rollback; funzionamento offline e segnalazione esplicita dei gap.
Non modificare oggi percorsi server, retention o copie esistenti e non avviare
download massivi senza stima di spazio, banda, licenze e piano di acquisizione.

### Ordine di esecuzione e chiusura

1. Inventario simulatore e dataset già presenti; specifica del modello comune,
   contratti/versioni, clock e budget di fedeltà con tolleranze per segnale.
2. Catalogo fonti/licenze e profili aeromobile/motore con validazione distinta
   da dati dimostrativi. Decisione motivata modello interno/bridge/ibrido.
3. Verticale SIL completa di una missione Treviso–Belluno: tutti i consumer
   dallo stesso scenario e dalle stesse interfacce, registrazione e replay.
4. Ingestione e pacchetti server/NAS, prove checksum/restore/offline/gap;
   estensione scenari e fault a tutte le applicazioni censite, matrice copertura.
5. Confronto con riferimenti indipendenti per ogni modello e tolleranze, prove
   integrazione, prestazioni e recovery con evidenze. Solo dopo, sessioni HIL
   autorizzate e hardware: nessuna promozione a DONE per la sola nota o SIL.

### R1-11 — primo incremento verificato (2026-10-24)

Inventario sorgente: SystemSimulator campiona SimulationSource e
EngineSimulationProvider, poi sovrascrive valori secondo fasi dimostrative.
La singola missione dura 600 secondi; i profili motore e traiettoria non sono
un modello prestazionale validato. EMS standalone e publisher Blackbox hanno
ancora propri generatori/clock: NON è già un SIL multi-nodo unificato.

Implementato nel solo SystemSimulator: un'unica tabella di portata alimenta
indicazione e integrale analitico dei consumi; ripartizione simmetrica tra i
due serbatoi dimostrativi. Motore fermo = portata zero e nessun consumo;
reset, seek avanti/indietro, pause e numero dei campioni non accumulano errori.
Provenienza SIM conservata. Quantità iniziale 10,57 US gal per serbatoio e
portate restano valori sintetici, NON dati approvati del 912 UL o dell'Asso.
Nessuna nuova semantica del fuel selector o gestione alimentazione motore.

Verifiche software: eseguibile C++ Linux PASS su tutti i quattro scenari con
601 campioni ciascuno, integrale manuale, seek, reset, limiti temporali,
provenienza e motore spento; mutazione integrale azzerato REJECTED. GCC nel
container serverha esistente, rete disabilitata, source read-only, hash tar
confrontato. Build/link MSVC PFD+EMS+test PASS e regressione QML 81 esiti PASS.
Evidenza `../status/evidence/r1-11-simulation-fuel-20261024/verification.json`.
Nessuna prova avionica reale o modifica di servizi; staging test conservato.

Audit ancora aperto: quota e VS provengono da leggi diverse (salita da 800 a
5500 ft in 85 s ma VS esposta 900 ft/min); posizione avanza anche nelle fasi
al suolo e non è integrata dalla GS; heading/assetto e traiettoria non sono
un'unica dinamica. QNH/quota indicata, termica, meteo e aeroporti richiedono
coerenza di modello e datum. Non correggere questi problemi presentando altri
valori arbitrari come prestazioni reali. Priorità successiva: specifica di
clock/frame comune e catalogo profili/dataset tracciabili, poi verticale SIL.
FS2024/altre fonti non ancora investigati o acquisiti, nessun nuovo archivio
geografico pubblicato su serverha/NAS e nessun download massivo eseguito.

### R1-12 — motore autonomo cinematico/impiantistico (2026-10-24)

AVX-G5-02 resta IN_PROGRESS. Il proprietario installerà MSFS 2024 più avanti:
non è una dipendenza del banco autonomo. ID correlati PRJ-004, EMS-002,
VER-001 invariati; NON un completamento del simulatore realistico integrato.

Implementato nel Core C++20 senza Qt: MissionProfile, MissionSimulator,
SimulationClock. Il vecchio SystemSimulator resta disponibile come demo;
le nuove missioni sono una modalità esplicita `mission-*` nella console PFD.

- Missione parametrica: grande cerchio tra due coordinate, distanza integrata
  dalla GS, segmento cruise dimensionato dalla distanza residua, nessun moto
  prima del taxi; quota e VS provengono dagli stessi segmenti. Durata calcolata,
  non più compressa a 600 s. QNH standard e quota indicata/pressione coerenti
  nella sola atmosfera di riferimento sintetica.
- Vento costante nord/est, velocità relativa aria, crab, pitch cinematico,
  rateo prua e bank della traiettoria. IAS è una PROXY EAS con densità
  isotermica semplificata (scala 8500 m), non una catena pitot/CAS qualificata.
- Motore: risposta RPM del primo ordine, consumi integrati dalla stessa
  risposta, ripartizione proporzionale dei serbatoi e arresto per esaurimento;
  olio/CHT/EGT con riscaldamento e raffreddamento. Elettrico: alternatore,
  carico e capacità batteria finita, perdita generazione e bus senza energia.
  Quattro cilindri e coefficienti sintetici fissi V1, NON famiglia Rotax validata.
- Flap comandato distinto da misura con velocità finita, consensi carrello
  dopo transito. Finestre di guasto configurabili per pressione olio, CHT,
  alternatore, IAS, barometro, GNSS, flap bloccato, carrello non bloccato,
  pressione carburante e arresto motore. Health/provenance/fault identity
  conservate; nessuna autorità verso attuatori reali.
- Ricostruzione dello stato da tempo/seed/profilo/finestre: nessuna dipendenza
  dalla storia dei campioni per valori, termica, carburante, flap o batteria.
  La sequenza di trasporto cresce; le annotazioni frame sono stato attivo,
  non una coda di edge-event da eseguire una seconda volta.
- Clock indipendente dal sistema operativo: pausa, seek, fattore 0,125–20,
  nanosecondi del chiamante senza perdita sistematica dei sotto-millisecondi.
  Il PFD usa il clock anche per la demo precedente. Input NaN e timing fuori
  contratto rifiutati; nessuna modifica al clock del sistema operativo.
- CLI `efis-mission-simulator`: JSONL offline, 18 campioni motore con unità,
  identità, raw, validità e SIM; dati volo e airframe nello stesso frame.
  `--describe` esporta seed, durata e 18 parametri del profilo. Parametri
  numerici espliciti validati; sempre SYNTHETIC_UNVALIDATED/flight_approval=false.
  Nessun socket, writer hardware o file esistente aperto in sovrascrittura.

Uso dopo build (stdout da archiviare in un nuovo run, non path di produzione):

```text
efis-mission-simulator --describe
efis-mission-simulator --scenario full-flight --step-ms 1000 --seed 20261024
efis-mission-simulator --scenario electrical-failure --step-ms 1000
efis-mission-simulator --describe --wind-east-kt 12 --ambient-c 5
```

Build/test mirati del nucleo senza display (in un checkout completo):

```text
cmake -S . -B build/sil -DEFIS_BUILD_QT_RENDERER=OFF -DEFIS_BUILD_EMS_NODE=OFF -DEFIS_BUILD_TESTS=ON
cmake --build build/sil --target efis_mission_simulator_tests efis-mission-simulator
ctest --test-dir build/sil -R efis_mission_simulator --output-on-failure
```

Il test CLI CTest richiede Python rilevato in configurazione. Su questa
workstation Windows il discovery Python del CMake esistente non lo rileva:
il test black-box CLI è stato quindi eseguito esplicitamente su Linux; non
considerare PASS un gate CTest locale non registrato. Nuovi eseguibili Windows
compilati ma non avviati per la policy del sistema, senza aggiramenti.

La CLI NON è DDP/EDP e NON è un formato di importazione diretto dei display.
I JSONL e la descrizione possono essere conservati come artefatti offline;
catalogazione automatica serverha/NAS e replay tramite trasporto richiedono
ancora un componente dedicato. La copia dei sorgenti sul server per test non
è una pubblicazione del catalogo, né un nuovo servizio server operativo.

Verifiche solo software: missione C++ con derivate posizione/GS, quota/VS e
serbatoi/portata, termica, fault/recovery, esaurimento, clock e seek; ASan/UBSan;
sei test CLI black-box; controller PFD C++ reale su Qt Core/Network senza GUI;
88 esiti QML inclusi sette esiti console/init-cleanup; 47 Python regressione;
build/link Release MSVC CLI/PFD/EMS/test. Evidenza sorgente/log/hash in
`../status/evidence/r1-12-autonomous-mission-20261024/verification.json`.
Il mutante che aggiunge 100 ft/min alla VS viene respinto dal test di derivata.

Limiti da NON occultare: traiettoria prescritta, non dinamica a forze/momenti;
un guasto motore non calcola ancora la planata. Taxi e pista sono segmenti
sulla rotta, non una geometria aeroportuale; nessun controllo collisione DEM
o ostacoli. Meteo operativo/METAR/TAF dichiarati UNAVAILABLE, non VFR inventato.
Non ci sono sei gradi di libertà, stallo, vento variabile/turbolenza, coeff.
di massa/CG/aerodinamica validati, selector/crossfeed completo, modelli distinti
dei due motori o avionica radio realisticamente propagata. I clock dei nodi
EMS standalone/Blackbox non sono ancora agganciati a una sessione centrale.
Nessuna nuova prova ARM64/display, volo o attivazione; power e golden aperti.

Prossimi blocchi: (1) backend dinamico e contratto di stato compatibile,
(2) profili aeromobile/motore con fonti e tolleranze indipendenti, (3) publisher
unico SIL isolato verso DDP/EDP più contratto airframe, (4) catalogo/run e
dataset server/NAS, (5) radar/meteo/terrain e matrice copertura multi-app,
(6) HIL/acceptance autorizzati. Nessuna promozione a DONE per la sola build.

Ricognizione primaria 2026-10-24, NON integrazione o concessione di diritti:
[JSBSim Manual](https://jsbsim-team.github.io/jsbsim-reference-manual/) descrive
un FDM non lineare 6DoF e licenza LGPL della libreria; i diritti dei singoli
modelli/dataset richiedono verifica separata. È candidato per il backend
dinamico, non dipendenza installata da R1-12.
[SDK MSFS2024](https://docs.flightsimulator.com/msfs2024/retail/programming-apis/simconnect/simconnect-sdk/)
e [SimVars](https://docs.flightsimulator.com/msfs2024/retail/programming-apis/simvars/simulation-variables/)
documentano interfacce per il futuro bridge; non autorizzano da soli la copia
di scenari, navdata o contenuti commerciali. Nessun SDK/pacchetto esterno,
modello MSFS o dataset geografico è stato scaricato in questo blocco.

### R1-13 — MSFS installato, prima acquisizione locale e bridge diagnostico

Aggiornamento proprietario: MSFS2024 Steam ora installato sul PC Windows
WIN-9MB3K78GSR2 / 192.168.1.33, SDK in D:\MSFS 2024 SDK. Nessuna dipendenza
aggiunta al simulatore autonomo. Sonda C++ preliminare: OPEN/12 frame PASS del
volo dichiarato C172; non certifica identità o fedeltà del modello. Nessun dato
o asset FS archiviato sul server e nessun diritto di redistribuzione dedotto.

Implementato `tools/simulation/msfs_readonly.py`, CLI Windows x64 senza librerie
Python esterne: sette SimVar provate dalla sonda, sola lettura locale, output
JSONL esclusivo/SIM/flight_approval=false, tempi host di ricezione, sequenze
output e sorgente separate, WAITING→SIMULATED→STALE→LOST, fault terminali,
NaN/corruzione rifiutati, valori non correnti null. Non è DDP/EDP e non contiene
publisher/rete, setter o comandi ad attuatori. Assi nativi non convertiti,
quote e orologio da qualificare prima di qualsiasi normalizzazione EFIS.

19 test offline PASS, inclusi ABI mock/allowlist API e file non sovrascrivibili;
66 test Python combinati PASS (19 nuovi + 47 regressioni); CMake Release
con Python esplicito registra il gate ed esegue CTest 1/1 PASS. Nessuna nuova
build C++/Qt completa: questo incremento non cambia core o renderer.
Il primo configure Debug e il riuso della cache hanno incontrato il C1902
MSVC/PDB già noto; configure Release e TRY_COMPILE_CONFIGURATION=Release
in directory separata PASS, senza modificare toolchain/security del PC.

Il tentativo runtime del nuovo client trova FS non più osservato in esecuzione:
open 0x80004005, exit 1, FAULT/null e summary FAIL. È soltanto prova negativa;
la sessione positiva della sonda C++ NON è attribuita al tool finale. Nuova
sessione positiva, pausa/disconnect e variazioni di volo controllate PENDING.
Runbook: `../development/MSFS_READONLY_BRIDGE_V1.md`.

AVX-G5-02 IN_PROGRESS e PRJ-004/EMS-002/VER-001 invariati. Prossimo passo:
riaprire cockpit e acquisire il nuovo client, poi mappare assi/quote/tempo,
aggiungere canali disponibili verificandoli sul modello scelto, preparare
registrazione/replay/catalogo server e adapter/publisher SIL isolati via
Blackbox. Nessun collegamento diretto MSFS→display, servizi/firmware/firewall,
power waiver, deployment sui nodi o HIL non autorizzato. Gli altri gate R1-12,
6DoF/profili reali e golden review restano aperti.

### R1-14 — priorità simulazione headless su serverha / grafica EFIS reale

Decisione proprietario 2026-10-25: nessun visualizzatore esterno necessario;
sviluppare i modelli su serverha e verificare in seguito le applicazioni EFIS
NEXT reali contemporaneamente, alimentate da una sessione comune.

Prima base fisica implementata ed eseguita: JSBSim 1.3.3/NumPy 2.3.4 isolati
su serverha, C172P incluso REFERENCE_UNVALIDATED, pilota interno al FDM,
integrazione 100 Hz/output 20 Hz, quattro scenari e replay completi con hash.
26 test serverha PASS (18 offline, 8 dinamica reale), 37 Windows e CTest 2/2
PASS. Quattro run persistenti, ciascuno 60 s/1201 frame, verificati senza rete.
Fonte e dettagli: `../development/SERVERHA_HEADLESS_SIL_V1.md` ed evidenza
`../status/evidence/r1-14-serverha-sil-20261025/verification.json`.

NON risolto implicitamente l'intero AVX-G5-02: prima di mostrare questi dati
sui pannelli servono adapter canonico per aria/quote/heading, clock/sessione
SIL e publisher DDP/EDP/airframe isolati, source selection opt-in senza sensori
reali e codec/consumer loopback; poi canary release/power su display reali.
Le role_view attuali verificano solo identità/tempo delle proiezioni offline.
Nessun servizio di stream o renderer attivato, nessun bridge/NAT o bypass
firewall, nessun nodo avionico modificato. Servizi serverha esistenti intatti.

Restano profili reali Asso/Pioneer e due motori, sonde per cilindro, elettrico,
missioni decollo/atterraggio, radio/meteo/terrain e catalogo/replica NAS. Il
C172P non è un sostituto validato degli aerei del proprietario. CHT/EGT medie
non vengono moltiplicate per quattro; i canali non modellati sono UNAVAILABLE.
AVX-G5-02 IN_PROGRESS e ID PRJ-004/EMS-002/VER-001 invariati. Il motore non ha
più una traiettoria prescritta come R1-12, ma non è il prodotto completo.

### R1-15 — richiesta collegamento display: percorso software e preflight

Adapter diagnostico offline JSBSim→tipi core→codec/consumer canonici DDP/EDP
implementato e verificato su serverha: cinque test black-box e quattro replay
fisici da 1201 frame, due consumer DDP e uno EDP allineati. SIM conservato;
CRC, duplicati, source identity e timeout STALE/LOST verificati. Nessun socket,
publisher o endpoint attivato. Identità test 65000 distinta dalla Blackbox 47;
consumer production configurati 47 rifiutano le fixture. Target MSVC build PASS,
CTest Windows 3/3 PASS, gate di build/test aggiunto alla CI Qt senza toccare
golden/tolleranze. Evidenza r1-15-sil-link-preflight-20261025.

Nuovo inventario read-only: PFD attivo via unità utente, MFD/EMS/Blackbox attivi.
EMS realmente osservato è a0be9a5, non più 62ba90b; aggiornato registro canonico
con manifesto e /proc/exe, non nuovo deploy. MFD/Blackbox mantengono 0x50000;
MFD ha undervoltage del 25 ottobre 00:02:26→28. Attivazioni restano BLOCCATE
dal gate alimentazione, non da una semplice richiesta di conferma software.

Nessuna equivalenza ingannevole CAS→IAS/MSL→QNH/TRUE→heading: mapping non
approvato rimane UNAVAILABLE. Prossimo software: modello sensori aria/heading,
publisher di sessione con aging/restart reali e ingresso SIM opt-in sul broker,
test loopback e rollback. Poi finestra fisica con sorgenti reali isolate,
qualifica PWR-001/release/golden e prova contemporanea sui renderer REALI.
Non attivare due publisher sulle porte operative, non impersonare source47,
non cambiare firewall o bridge fra LAN servizio e FLIGHT-DATA. Runbook:
`../development/SIL_CANONICAL_LINK_PREFLIGHT_V1.md`. AVX-G5-02 IN_PROGRESS.

### SIL-WX/NAV — estensione obbligatoria richiesta il 2026-10-25

Stato PIANIFICATO, non nuova capacità verificata. Riferimenti NAV-001/002/003,
WX-001/002, VER-001 e AVX-G5-02; stati correnti invariati. Un unico scenario
serverha, clock, seed e identificativo di sessione per volo, EMS, ambiente,
prodotti meteo e navigazione. Grafica esclusivamente EFIS NEXT reale.

| Blocco | Perimetro da sviluppare | Criterio di accettazione software |
|---|---|---|
| WX-A ambiente fisico | pressione/QNH, temperatura/densità, vento per quota, raffiche, turbolenza e shear; visibilità, nubi e precipitazioni come campi di scenario; ghiaccio solo con modello dedicato | vento influenza davvero TAS/GS/TRK/deriva e dinamica; QNH/quote/unità/convenzioni controllati; effetti non modellati dichiarati, mai simulati come validati |
| WX-B prodotti | METAR/TAF sintetici e radar georeferenziato evolutivo, osservazioni/previsioni distinte, stazioni/tempi/lead-time/provenienza | stessi parser/cache/consumer del prodotto; legenda e SIM sempre, coordinate e valid time coerenti, CURRENT/STALE/EXPIRED/OFFLINE/clock-skew, mai cache operativa contaminata |
| NAV-A geometria radio | VOR radiali e TO/FROM/OBS/CDI, DME distanza obliqua, NDB/ADF bearing, ILS localizer/glideslope; convenzioni TRUE/MAG e declinazione esplicite | casi geometrici indipendenti, passaggio stazione e ambiguità/cone-of-confusion, fronte/retro e limiti ILS, copertura e terrain masking solo se modellati; assenza segnale non centratura fittizia |
| NAV-B GNSS e rotta | posizione/GS/TRK, disponibilità/accuratezza/degrado GNSS; aeroporti/piste/stazioni/frequenze versionati; piano di volo, leg, direct-to, nearest, XTK/DTK, distanza, ETA/ETE e alternati | sequenziamento leg e selezione sorgente coerenti, intercetti e crosswind, errori/fault/recovery, dataset dichiarato e ripetibile; procedure pubblicate solo con dataset idoneo e diritti verificati |
| NAV-C impianti e HMI | selezione/frequenze NAV e identificativo stazione simulati, annunci CDI e source/age sui renderer reali; eventuali cue autopilota solo nel FDM | confronto numerico PFD/MFD con stessa sessione; nessun controllo Pixhawk/servi o trasmettitore RF; perdita fonte chiaramente annunciata |
| DATA/TEST | catalogo serverha di scenari, dataset/licenze/versioni e registrazioni con hash; replica NAS da implementare senza sovrascrivere archivio operativo | replay deterministico, stop/pause/seek/restart, separazione SIM/LIVE, test analitici/unitari/fault e infine acceptance end-to-end sui nodi dopo gate |

Distinzione obbligatoria: la verità fisica del mondo, il sensore simulato e
il bollettino ricevuto sono livelli diversi. Un radar ritardato può essere
coerentemente diverso dal tempo attuale; un TAF è una previsione, non copia
del presente. QNH e modello altimetro devono usare la stessa atmosfera, senza
alias geometrico→indicato. Non inventare integrità GNSS certificata o precisione
radio/ostacoli che non sia modellata e verificata. DME non è distanza al suolo.

Scenari minimi: Treviso–Belluno nominale con vento e variazione QNH; intercetto
radiale VOR e passaggio stazione; rotta NDB; intercetto ILS con fuori copertura;
GNSS perso con navigazione radio ancora disponibile; stazione guasta o
identificativo errato; bollettino/radar scaduto e perdita hotspot; shear/raffiche
e recovery. Profili addestrativi NON sono procedure operative approvate.

Ordine: 1) contratto scenario/clock e test di riferimento; 2) atmosfera e
sensori aria/compasso; 3) geometria VOR/DME/GNSS e piano di volo; 4) NDB/ILS,
copertura e fault; 5) METAR/TAF/radar sintetici tramite cache reale separata;
6) publisher comune isolato, registrazione/catalogo e prove renderer end-to-end.
Possibile sviluppo offline mentre PWR-001 blocca attivazioni; non equivale a
collaudo fisico o completamento «navigazione completa». Sviluppi RNAV avanzati,
procedure e funzioni di guida vanno censiti separatamente prima di dichiararli
coperti. Nessun nuovo codice runtime o hardware modificato da questa nota.

### R1-16 — implementazione prima base SIL-WX/NAV, ancora PARZIALE

2026-10-25: implementato mondo offline deterministico e integrato nel backend
JSBSim tramite --world. Vento/raffiche per quota e temperatura/QNH influenzano
il FDM; pressione reale letta per il modello altimetro. Geometria VOR/DME,
NDB/ILS, GNSS con fault, rotta/direct-to/XTK/DTK/ETA verso WP; prodotti METAR,
TAF di persistenza e radar PNG/dBZ con riferimenti, date/age e fault
offline/delay/skew. Configurazione e radiofari SIM fittizi espliciti,
manifest/hash, nuova sessione soltanto, replay semantico fail-closed.

Evidenza software: 66 test Windows, 53 serverha (6 fisici JSBSim), CTest 4/4;
vento modifica GS/posizione, ripetizione byte-identica, GNSS perso con VOR
disponibile, frame NAV mutato respinto con hash ricalcolato. Sorgenti serverha
isolate world-source-v6 e vecchie copie conservate. Runbook aggiornato
../development/SERVERHA_HEADLESS_SIL_V1.md, evidence r1-16-sil-world-20261025.

NON chiusi WX-A..DATA/TEST: non sono simulazioni complete di meteo/RF e non
ci sono nuovi consumer delle applicazioni reali. Prossima implementazione:
adapter tipizzati del nuovo gruppo world e namespace cache SIM per prodotti,
test dei parser effettivi/raw TAF evoluti e radar dinamico sui renderer,
dataset NAV/licenze/versioni e aeroporti/alternati, poi publisher isolato con
rollback. Terrain masking/RNAV/procedure/missione completa/catalogo-NAS e
qualifica dei due motori restano distinti. Gate power/golden e rete operativa
invariati, nessun nodo o attuatore modificato, nessun avanzamento a DONE.

### R1-17 — primo pannello collegato, lease banco e limiti osservati

PFD.45 con0x0, binario installato7051a47 invariato, temporary screen/feed in
namespace privato condiviso (solo loopback), clock di emissione/instance
separati dal replay. Nominale60s senzafault ciclico900s, source65000/port49152.
Trasferimento file da serverha, NON stream live. Stato/settings/cache isolati;
recorder1419 invariato; ritorno automatico allo stopfeed e restore service
eseguiti con successo. Timer930s/RuntimeMax900 predisposti, scadenza completa
non osservata. Tool ARM64+6test PASS; MSVC+CTest5/5 PASS. Schermo reale SIM,
terreno locale e20Hz/0reject/0gap osservati, power sempre0x0.

Non chiuso AVX-G5-02: nuova integrazione parziale di IAS/quote/MAG/GNSS/OBS/CDI,
non intera NAV UI, EMS/meteo/airframe/VS indipendente. UI7051 legacy espone
ancora label/impianti dimostrativi e zeri sottoFAIL; V-speeds reference non
inventate, indicazioneUNAVAILABLE. Startup stalls fino1.6s e27gap imagery
rimangono difetti. Prima della nuova release servono consumer world/HMI e
profilo modello approvato, review golden/ARM64, cache meteo SIM, ident NAV,
runtime supportato e acceptance touch/loop/stop. Nessun waiver MFD/Blackbox
power; nessun writer/attuatore, rete operativa o main modificati.
