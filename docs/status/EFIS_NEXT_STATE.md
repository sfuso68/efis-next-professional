# EFIS NEXT — Stato corrente sintetico

Aggiornamento: 2026-09-20
Repository autorevole: `sfuso68/efis-next-professional`
Baseline osservata per questo aggiornamento: `origin/main` `67aee2e`
Stato prodotto: DEVELOPMENT / NON-CERT / NON APPROVATO AL VOLO

Questo documento contiene soltanto lo stato corrente. Lo storico completo fino
al 2026-09-19 è preservato senza perdita in
[`archive/EFIS_NEXT_STATE_FULL_THROUGH_2026-09-19.md`](archive/EFIS_NEXT_STATE_FULL_THROUGH_2026-09-19.md).
Il registro operativo riga-per-riga resta
[`EFIS_NEXT_MASTER_MATRIX.md`](EFIS_NEXT_MASTER_MATRIX.md); la release dei nodi
è governata esclusivamente da
[`EFIS_NEXT_ACTIVE_RELEASES.md`](EFIS_NEXT_ACTIVE_RELEASES.md).
Il confronto dettagliato fra pianificazione R0, gate, workstream e stato reale
è in
[`AVIONIX_NEXT_PROGRESS_VS_PLAN_2026-09-20.md`](../project/AVIONIX_NEXT_PROGRESS_VS_PLAN_2026-09-20.md).

## 1. Quadro immediato

| Gate/area | Stato | Sintesi | ID autorevoli |
|---|---|---|---|
| Governance e fonti | PARTIAL | Brief, indice, backlog e controllo progetto presenti; naming, cinque fonti e bare NAS duplicato aperti | `PRJ-001`, `PRJ-002`, `AVX-G0-02`…`05` |
| Nodo sensori ASSO5 | BLOCKED-HW | RPi4 `.89` identificato; Sequent 0/4, alimentatore, DIP, mapping e calibrazione non verificati | `PRJ-002`, `AVX-G1-01`…`04` |
| ICD distribuito | FROZEN-SW / VERIFY-HW | DDP V2 ed EDP V1 congelati; replay x86-64 e tre ARM64 PASS; verticale reale aperto | `PRJ-003`, `AVX-G2-01`, `AVX-G2-02` |
| HMI SkyView/EMS | VERIFIED-SW / VERIFY-HW | Contratti e golden multi-risoluzione presenti; touch, pannelli e fault reali aperti | `PRJ-004`, `AVX-G3-03`, `AVX-G3-04` |
| PFD/MFD | PARTIAL | PFD `60b3526` confermato; MFD su release immutabile `mfd-3432402-primary-v1` con rollback/reboot PASS; terrain e acceptance aperti | `PFD-001`…`003`, `MFD-001`, `NAV-003`, `VER-003` |
| EMS | PARTIAL / DEPLOY CONFORME | RPi4 su `ems-7655799-primary-v1`, rollback/reboot e persistenza PASS, EDP `LIVE • SIM`; input motore reale e limiti controllati mancanti | `EMS-002`…`004`, `PROD-005` |
| Pixhawk/GNSS/Airspeed | PARTIAL-HW | MAVLink read-only presente; M10 e DLVR non qualificati né calibrati | `PIX-001`, `PIX-002`, `GNSS-001`, `GNSS-002`, `AIR-001`, `AIR-002` |
| Blackbox/rete | PARTIAL | broker su release immutabile `blackbox-7655799-primary-v1`, rollback/reboot PASS; engine SIM, fault/endurance e autenticazione aperti | `BBX-001`…`004`, `NET-001`, `PROD-005` |
| Server/NAS/Ground station | PARTIAL | Servizi e copie esistono; manifest release, CA, lifecycle e doppia copia verificata non chiusi | `SRV-001`…`003`, `GS-001`…`003`, `AVX-G0-02`, `03` |
| Alimentazione | VERIFY-HW | Storico Blackbox `get_throttled=0x50000`; PSU/cavo e prove brownout non qualificati | `PWR-001`, `PROD-002`, `VER-003` |

## 2. Architettura vincolante

- Blackbox è l'unico punto di ingestione, arbitraggio, normalizzazione e
  registrazione; i display non devono consumare direttamente i frame grezzi
  (`BBX-002`, `CORE-003`, ADR-003).
- PFD e MFD usano RPi5. EMS e VIDEO-CORE usano due RPi4 distinti. Il nodo
  ASSO5-SENSOR è un ulteriore RPi4 e non va confuso con EMS o VIDEO-CORE
  (`PRJ-002`, `EMS-002`, `FPV-001`, ADR-006/010/013).
- FLIGHT-DATA e VIDEO restano reti separate. La Blackbox non è autorizzata a
  fare bridge, NAT o routing fra domini (`NET-001`, `PROD-004`, ADR-010/015).
- Pixhawk è una sorgente read-only. Nessuna funzione EFIS ha autorità su
  autopilota, servi, radio, transponder, flap o carrello (`PIX-003`, `PAN-003`,
  ADR-012).
- Dati SIM, fallback e reali devono restare distinguibili e tracciabili
  (`CORE-002`, `EMS-001`, `PRJ-003`, `PRJ-004`).

## 3. Release e configurazione

Lo stato “attivo” non viene dedotto dai checkpoint. La verifica read-only del
2026-09-20 ha confermato PFD `60b3526`. La successiva finestra autorizzata ha
migrato MFD, EMS e Blackbox al contratto immutabile V1, con manifest, processo
canonico, rollback drill e reboot acceptance PASS su tutti e tre i nodi.
Il dettaglio è nel registro canonico
[`EFIS_NEXT_ACTIVE_RELEASES.md`](EFIS_NEXT_ACTIVE_RELEASES.md) e nell'evidenza
[`release-inventory-20260920`](evidence/release-inventory-20260920/README.md) e
[`release-immutability-20260920`](evidence/release-immutability-20260920/README.md)
(`PFD-001`, `MFD-001`, `EMS-002`, `BBX-003`, `PROD-005`).

## 4. Verificato solo software/documentazione

- ICD V1, CRC/SHA, evidence bundle e replay: workstation più PFD/MFD/EMS ARM64
  PASS; il verticale sensore reale resta escluso (`PRJ-003`, `AVX-G2-01`).
- Contratti HMI e golden SkyView/EMS multi-risoluzione: validatori e test PASS;
  nessuna acceptance del pilota sui pannelli (`PRJ-004`, `AVX-G3-04`).
- Inventario, readiness e proposta upstream ASSO5: formati fail-closed pronti,
  ma nessun modello, DIP, channel map o calibrazione reale verificata
  (`PRJ-002`, `AVX-G1-02`, `03`).
- I branch non ancora integrati `safety/hmi-golden-rerender-v1`,
  `safety/icd-cpp-vectors-v1` e `safety/hmi-behavioral-contract-v1` aggiungono
  rispettivamente rerender golden reale, vettori wire C++ e test comportamentali
  EMS. Sono evidenze software candidate, non baseline `main` e non deployment
  (`PRJ-003`, `PRJ-004`, `EMS-002`).

## 5. Ultime verifiche hardware che restano valide

- PFD `.45`, MFD `.49`, EMS `.36` e Blackbox `.47` sono stati raggiunti e
  inventariati read-only il 2026-09-20 con 20 controlli complessivi. Server
  `.6`, portatile `.39`, NAS `.22` e sensor node `.89` non sono stati
  riconfermati in questo blocco (`PFD-001`, `MFD-001`, `EMS-002`, `BBX-003`,
  `GS-001`, `SRV-001`).
- G2 ARM64 ha prodotto validator e 11/11 test PASS su PFD, MFD ed EMS con
  digest coincidenti (`PRJ-003`, `AVX-G2-01`).
- La catena EDP SIM Blackbox→EMS ha mostrato LIVE, STALE, LOST, fallback e
  recovery; non prova la sorgente CAN motore reale (`EMS-002`, `BBX-003`).
- Pixhawk/MAVLink read-only e fan-out monitor-only sono stati osservati; la
  prova Wi-Fi Ground Station resta degradata per timing (`PIX-002`, `NET-001`).
- Blackbox ha mostrato storicamente `get_throttled=0x50000`; campioni successivi
  `0x0` non cancellano i bit storici né qualificano PSU/cavo (`PWR-001`).

## 6. Blocchi e rischi prioritari

1. Inventario fisico ASSO5: quattro Sequent, DIP, alimentatore, cablaggi e
   fotografie non acquisiti (`PRJ-002`, `AVX-G1-02`).
2. Alimentazione: storico undervoltage Blackbox, nessuna prova brownout/load
   controllata e nessuna PSU finale qualificata (`PWR-001`, `PROD-002`).
3. Release: MFD, EMS e Blackbox sono conformi al contratto V1; PFD non ha
   ancora il manifesto V1 e il recorder PFD resta fuori release, quindi
   `PROD-005` resta `PARTIAL`.
4. HMI: touch, leggibilità, sunlight/night, frame-rate, termica e fault su
   pannelli reali non accettati (`PRJ-004`, `AVX-G3-03`).
5. Synthetic vision: terreno vicino stirato/sfocato e LOD/proiezione non
   accettati (`NAV-003`, `AVX-G3-01`).
6. Motore reale: CAN, MGL, limiti Rotax e calibrazioni privi di fonte completa
   e di prova end-to-end (`EMS-003`, `EMS-004`, `AVX-G3-02`).
7. Safety: Emergency Assist resta consultivo; checklist e servi sono bloccati
   da fonti, identificazione, interlock e HIL (`PAN-001`…`004`).

## 7. Prossima sequenza controllata

1. Conservare l'evidenza MFD/EMS/Blackbox e completare separatamente PFD e
   recorder con la stessa disciplina di manifest, rollback e reboot
   (`PROD-005`).
2. Acquisire le nove immagini e i metadati dell'inventario ASSO5 senza probing
   o modifica DIP (`PRJ-002`, `AVX-G1-02`).
3. Qualificare alimentatore/cavo Blackbox e nodo sensori con piano brownout e
   carico approvato (`PWR-001`, `PROD-002`).
4. Integrare e sottoporre a CI i tre branch safety software prima di qualsiasi
   deployment (`PRJ-003`, `PRJ-004`, `EMS-002`).
5. Solo dopo review/merge, eseguire HMI harness su PFD/MFD/EMS e verticale G2
   reale con nuova autorizzazione (`AVX-G2-02`, `AVX-G3-03`).

## 8. Criterio di verità

`DONE` richiede evidenza conforme al criterio di chiusura della Master Matrix.
Build, test software, presenza di codice o decisione architetturale non
sostituiscono una prova hardware quando il criterio la richiede. Nessuna voce
in questo documento modifica hardware, rete, servizio o release (`PRJ-001`,
`PRJ-002`).

## 9. Chiusura del blocco documentale P4–P8

- P4: creata la fonte canonica delle release e risolte le dichiarazioni
  contraddittorie senza presumere lo stato attuale (`PRJ-001`, `PROD-005`).
- P5: ogni blocco dello stato corrente rimanda agli ID della Master Matrix o
  del backlog AVIONIX (`PRJ-001`).
- P6: introdotto il requisito alimentazione con lo storico Blackbox
  `0x50000`, esplicitamente non chiuso (`PWR-001`, `PROD-002`, `VER-003`).
- P7: `CORE-003`, `INST-002`, `GS-002` e `BBX-002` sono stati declassati da
  `DONE` a `VERIFY-HW`/`PARTIAL` perché i rispettivi criteri hardware non hanno
  evidenza sufficiente.
- P8: le 4.011 righe pregresse sono state spostate nell'archivio immutato e lo
  stato corrente è stato ricostruito come sintesi operativa (`PRJ-001`).

Verifica iniziale del blocco P4–P8: generatore e `--check` PASS, controllo
sintattico Python PASS e `git diff --check` PASS. La successiva acquisizione
read-only del 2026-09-20 ha eseguito 20 controlli sui quattro nodi principali;
non ha modificato hardware o runtime e non promuove alcun requisito a `DONE`.

## 10. Contratto release immutabili V1 candidato

Il blocco candidato introduce schema manifesto, validatore fail-closed, policy
nominativa dei cinque processi e drop-in systemd candidati. I test software
verificano hash, traversal, ruolo/piattaforma,
`flight_approval=false`, assenza di `build/dev` e mantenimento esplicito della
simulazione Blackbox. Gli asset sono installati su MFD, EMS e Blackbox con
rollback e reboot verificati. `PROD-005` resta `PARTIAL` perché PFD/recorder non
fanno parte del batch concluso e i gate hardware complessivi restano aperti.

## 11. Packaging e preflight release V1

Implementato sul branch `work/release-supervision-contract-v1` un packager
deterministico del manifesto e un generatore di piano di transizione puramente
descrittivo. Il validatore rifiuta ora anche symlink negli artefatti; manifesto
e piano sono scritti atomicamente senza sovrascrittura implicita. Il piano lega
nodo, ruolo, piattaforma, servizio, candidato, rollback e digest, mantiene
`execution_authorized=false`, non contiene comandi mutanti e richiede nove gate
prima di ogni futura finestra di banco (`PROD-005`).

Verifica software iniziale: 19 test mirati, 18 PASS e un caso symlink SKIP su
Windows, coperto dalla CI Linux. Verifica hardware successiva: MFD, EMS e
Blackbox hanno manifest PASS, eseguibile canonico, rollback drill e reboot
acceptance PASS. Sull'EMS sono state inoltre corrette e verificate la separazione
dei dati mutabili e la conservazione delle variabili EDP/kiosk del launcher.
PFD e recorder restano aperti; `PROD-005` rimane `PARTIAL`.

## 12. Correzione candidata HMI EMS — limiti e stato CAS

Sul branch `fix/ems-limit-cas-status-v1` la dicitura ripetuta
`LIMITS UNVERIFIED` è stata sostituita da un solo stato ambra
`LIMITS INDICATIVE` nel pannello CAS e da piccoli marcatori sui quadranti. La
provenienza non è stata promossa: il profilo attivo resta
`INDICATIVE_UNVERIFIED` e soltanto il valore esatto `VERIFIED` può rimuovere
l'avviso (`EMS-004`, `PRJ-004`).

È stato inoltre corretto il falso `BLACKBOX LINK LOST`: il CAS usa
`dataSourceState` per la perdita del trasporto EDP, mentre
`centralBlackBoxState` segnala esclusivamente `CENTRAL LOG SYNC OFFLINE`. La
catena EDP osservata sul nodo prima della modifica era `LIVE • SIM`; nessuna
sorgente reale è stata introdotta (`EMS-002`, ADR-011).

Verifica esclusivamente software: build Windows dei target PFD ed EMS PASS,
18 test HMI PASS, sette test dei tool golden PASS, validatori HMI/EMS PASS, tre
test CTest mirati PASS e cattura software 800×480 ispezionata senza
sovrapposizioni. Nessuna release è stata creata o distribuita e nessuna
verifica sul pannello RPi4 è ancora avvenuta; lo stato resta
`PARTIAL`/`VERIFY-HW`.
