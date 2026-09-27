# AVIONIX NEXT — backlog esecutivo per gate

Aggiornamento: 2026-09-21
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
