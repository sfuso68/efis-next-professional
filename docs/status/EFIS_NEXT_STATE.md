# EFIS NEXT — Stato corrente sintetico

Aggiornamento: 2026-09-22
Repository autorevole: `sfuso68/efis-next-professional`
Baseline integrata precedente a questo aggiornamento: `origin/main` `fa91bee09a5fc62afb9a7fe778954f172e1a77f7`
HEAD `main` verificato all'ingresso del blocco C7 Linux: `fa91bee09a5fc62afb9a7fe778954f172e1a77f7`
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
| Governance e fonti | PARTIAL | Brief, indice, backlog e controllo progetto presenti; topologia Git chiusa da ADR-018; naming e cinque fonti restano aperti | `PRJ-001`, `PRJ-002`, `AVX-G0-02`…`05` |
| Nodo sensori ASSO5 | BLOCKED-HW | RPi4 `.89` identificato; Sequent 0/4, alimentatore, DIP, mapping e calibrazione non verificati | `PRJ-002`, `AVX-G1-01`…`04` |
| ICD distribuito | FROZEN-SW / VERIFY-HW | DDP V2 ed EDP V1 congelati; replay x86-64 e tre ARM64 PASS; verticale reale aperto | `PRJ-003`, `AVX-G2-01`, `AVX-G2-02` |
| HMI SkyView/EMS | VERIFIED-SW / VERIFY-HW | Contratti e golden multi-risoluzione presenti; touch, pannelli e fault reali aperti | `PRJ-004`, `AVX-G3-03`, `AVX-G3-04` |
| PFD/MFD | PARTIAL | PFD+recorder su `pfdrec-d52e377-primary-v1` e MFD su `mfd-3432402-primary-v1`, entrambi con rollback/reboot PASS; terrain e acceptance aperti | `PFD-001`…`003`, `MFD-001`, `NAV-003`, `VER-003` |
| EMS | PARTIAL / DEPLOY CONFORME | RPi4 su `ems-7655799-primary-v1`; attivazione, rollback drill e reboot acceptance PASS, EDP `LIVE • SIM`; input motore reale e limiti controllati mancanti | `EMS-002`…`004`, `PROD-005` |
| Pixhawk/GNSS/Airspeed | PARTIAL-HW | MAVLink read-only presente; M10 e DLVR non qualificati né calibrati | `PIX-001`, `PIX-002`, `GNSS-001`, `GNSS-002`, `AIR-001`, `AIR-002` |
| Blackbox/rete | PARTIAL | broker su release immutabile `blackbox-7655799-primary-v1`, rollback/reboot PASS; engine SIM, fault/endurance e autenticazione aperti | `BBX-001`…`004`, `NET-001`, `PROD-005` |
| Server/NAS/Ground station | PARTIAL | GitHub/serverha/Blackbox/NAS allineati e replica NAS ristretta attiva; restore drill, CA/lifecycle e ritiro copia temporanea aperti | `SRV-001`…`003`, `GS-001`…`003`, ADR-018 |
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
- Contratti HMI e golden SkyView/EMS multi-risoluzione: validatori e test PASS.
  Il branch C7 Linux aggiunge una baseline separata `offscreen`, compilazione
  Qt 6.8.2 e confronto pixel bloccante; il run bootstrap Core CI #131 è PASS.
  I run diagnostici #133/#134 hanno isolato nei soli stati LOST/FAIL il
  lampeggio CAS non deterministico; la fixture golden lo congela senza
  disabilitarlo nel runtime operativo e i sei frame interessati sono stati
  rigenerati dal run #135 con artifact verificato.
  È verifica software x86-64/Mesa, non acceptance del pilota sui pannelli
  (`PRJ-004`, `AVX-G3-04`).
- Il blocco C8 aggiunge il session manager operativo fra piano A20 e packager
  P20: crea bundle `NOT_RUN` non sovrascrivibili, congela piano/source commit,
  confina i percorsi e finalizza soltanto tre run complete con conferma
  operatore. La pipeline sintetica e 11 test mirati sono PASS; nessun pannello
  è stato contattato e ciò non chiude `AVX-G3-03` (`PRJ-004`).
- Il blocco C9 congela il benchmark synthetic vision Treviso–Belluno: cinque
  checkpoint DAY/NIGHT, attraversamento `N45E012`→`N46E012`, soglie 40 FPS/
  p95 25 ms, fault missing-tile ed evidence con digest. Contratto e test
  fail-closed sono verificati solo software; renderer, rotta e pannelli non
  sono stati eseguiti (`AVX-G3-01`, `SV-001`, `PFD-001`, `MFD-001`).
- Il blocco C10 rende C9 operativamente acquisibile senza inventare risultati:
  prepara una sessione non sovrascrivibile per PFD e MFD, congela commit e
  contratto, separa gli artefatti e finalizza soltanto due run validi con
  conferma operatore. I test sintetici sono PASS, ma nessun nodo o renderer è
  stato eseguito (`AVX-G3-01`, `SV-001`, `PFD-001`, `MFD-001`).
- Il blocco C11 assembla ogni run PFD/MFD dai sei artefatti reali previsti:
  ricava FPS/frame-time e termica da timeline continue, verifica tile/fault,
  dieci screenshot DAY/NIGHT e firma operatore, quindi genera atomicamente un
  risultato C9 non sovrascrivibile. Le 17 prove C9+C10+C11 sono PASS solo
  software; nessun nodo o renderer è stato eseguito (`AVX-G3-01`, `SV-001`,
  `PFD-001`, `MFD-001`).
- Il blocco C12 corregge nel renderer Quick 3D la causa software della fascia
  a bassa definizione sul confine N45/N46: il LOD usa ora la distanza
  dall'impronta tile, con isteresi 18/24 km e preload a 45 km. Modello C++20,
  adapter Qt, build completa, QML AOT e le quattro suite C9–C12 sono PASS.
  Nessun RPi5 è stato contattato: qualità visiva, ≥40 FPS, p95 ≤25 ms, termica,
  fault e firma operatore restano `VERIFY-HW` (`AVX-G3-01`, `SV-001`,
  `PFD-001`, `MFD-001`).
- Inventario, readiness e proposta upstream ASSO5: formati fail-closed pronti,
  ma nessun modello, DIP, channel map o calibrazione reale verificata
  (`PRJ-002`, `AVX-G1-02`, `03`).
- I controlli rerender golden Windows, vettori wire C++ e test comportamentali
  EMS risultano integrati nella baseline corrente. Il gate Linux/Qt resta
  candidato nel branch dedicato fino a PR revisionata e CI finale verde; non è
  deployment né acceptance hardware (`PRJ-003`, `PRJ-004`, `EMS-002`).

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
3. Release: PFD, recorder, MFD, EMS e Blackbox sono conformi al contratto V1.
   Sul PFD la release dual-role `pfdrec-d52e377-primary-v1` ha superato
   manifest, processo canonico, rollback reale e reboot acceptance; il recorder
   scrive fuori dall'albero immutabile. `PROD-005` è `DONE` per il contratto
   update/rollback, senza modificare lo stato DEVELOPMENT/NON-CERT.
4. HMI: touch, leggibilità, sunlight/night, frame-rate, termica e fault su
   pannelli reali non accettati (`PRJ-004`, `AVX-G3-03`).
5. Synthetic vision: la causa LOD al confine è corretta e verificata software
   da C12, ma terreno vicino, proiezione e prestazioni non sono ancora accettati
   sui due RPi5 (`NAV-003`, `AVX-G3-01`).
6. Motore reale: CAN, MGL, limiti Rotax e calibrazioni privi di fonte completa
   e di prova end-to-end (`EMS-003`, `EMS-004`, `AVX-G3-02`).
7. Safety: Emergency Assist resta consultivo; checklist e servi sono bloccati
   da fonti, identificazione, interlock e HIL (`PAN-001`…`004`).

## 7. Prossima sequenza controllata

1. Acquisire le nove immagini e i metadati dell'inventario ASSO5 senza probing
   o modifica DIP (`PRJ-002`, `AVX-G1-02`).
2. Qualificare alimentatore/cavo Blackbox e nodo sensori con piano brownout e
   carico approvato (`PWR-001`, `PROD-002`).
3. Integrare e sottoporre a CI i tre branch safety software prima di qualsiasi
   deployment (`PRJ-003`, `PRJ-004`, `EMS-002`).
4. Solo dopo review/merge, eseguire HMI harness su PFD/MFD/EMS e verticale G2
   reale con nuova autorizzazione (`AVX-G2-02`, `AVX-G3-03`).
5. Dopo integrazione C12, creare sessioni C10 dal commit installato e completare
   C11/C9 su PFD e MFD con confronto visuale della transizione N45/N46
   (`AVX-G3-01`, `SV-001`).

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
rollback e reboot verificati. Il batch PFD/recorder del 2026-09-21 completa lo
stesso contratto sui processi rimanenti; `PROD-005` è ora `DONE`. I gate
hardware di altre famiglie restano aperti.

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
Il batch PFD/recorder è stato completato il 2026-09-21; `PROD-005` è `DONE`.

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

Verifica software: build Windows dei target PFD ed EMS PASS, 18 test HMI PASS,
sette test dei tool golden PASS, validatori HMI/EMS PASS e tre test CTest mirati
PASS. La build ARM64 nativa sul nodo EMS ha ripetuto con esito PASS i tre CTest,
i 18 test HMI e i validatori HMI/EMS.

Verifica hardware del 2026-09-20: la release immutabile
`ems-7655799-primary-v1` è stata attivata sul RPi4 EMS reale dopo validazione
manifesto, prova candidato, rollback drill e ritorno al candidato. Il servizio
`efis-ems.service` è `active/running`, `NRestarts=0`, l'eseguibile proviene
dalla release attiva e `get_throttled=0x0`. Una cattura runtime 800×480
generata sul nodo (`ems-20260920-190524045.png`, SHA-256
`2e74c0a4d6e5f5f1a2fb7da6321764460ce929ef42bf5ac6070162ed4b0e9831`) è
stata ispezionata: mostra un solo `LIMITS INDICATIVE`, marcatori compatti sui
quadranti e `CENTRAL LOG SYNC OFFLINE`, senza il falso `BLACKBOX LINK LOST`.

Reboot acceptance del 2026-09-20: il boot ID è cambiato in
`ec0eed02-052f-426b-804e-53be70646081`; la release candidata è rimasta attiva,
il servizio si è avviato automaticamente con `NRestarts=0`, manifesto PASS,
ambiente kiosk/EDP corretto e socket `10.77.10.12:49003` aperto. Il journal
post-boot ha confermato `LIVE • SIM`, quattro pacchetti già accettati e zero
reject, gap o restart del publisher. `get_throttled=0x0`. La nuova cattura
runtime `ems-20260920-192921616.png` (SHA-256
`28491481cf0b3eb0efd69701f0c2384c4cafcca814327e572cc43deacb10a082`) è
coerente con la cattura pre-reboot.

Restano aperti e non sono dichiarati PASS: ispezione diretta sul pannello
fisico/touch, prova con limiti motore approvati e sorgente CAN motore reale. Lo
stato resta `PARTIAL`/`VERIFY-HW`.

## 13. Release PFD + recorder — attivazione e reboot PASS

L'audit read-only del 2026-09-20 sul nodo `efis-pfd-01` ha riconfermato:

- `/home/efis/efis-next-current` → `efis-next-avionics-60b3526`;
- `efis-pfd.service` attivo, PID valido, `NRestarts=0`;
- `efis-recorder.service` attivo ma ancora eseguito dal checkout mutabile
  `/home/efis/efis-next/build/dev/efis-recorder`;
- 31 GiB liberi, `get_throttled=0x0`, temperatura 53,2 °C.

Sono stati aggiunti tre asset fail-closed per staging dual-role, attivazione
atomica con rollback e verifica post-reboot. I drop-in PFD/recorder portano
working directory, output e stato mutabile fuori dalla release. Venticinque
test Python mirati sono PASS; il caso symlink è SKIP su Windows e resta coperto
dalla CI Linux. La build ARM64 completa ha superato 50/51 CTest: il solo test
QGC aveva un fixture x86-64 fisso, poi corretto e verificato ARM64 21/21 PASS.

Con autorizzazione esplicita sono state create e validate la primaria
`pfdrec-d52e377-primary-v1` e la rollback `pfdrec-60b3526-rollback-v1`. Due
tentativi preliminari sono falliti in modo sicuro prima della chiusura: il
primo per ordine di verifica del symlink, il secondo per proprietà dello
storage recorder. In entrambi i casi il recovery ha ripristinato link, drop-in
e servizi precedenti. Dopo le correzioni, il drill candidato → rollback →
candidato è PASS.

Il reboot del 2026-09-21 ha cambiato boot ID da
`0e51e200-330a-42f6-a539-6992a640aae9` a
`be165369-0f3a-4477-a095-b45ccfab6f32`. Manifest dual-role, eseguibili,
drop-in, DDP `LIVE`, avanzamento recorder, hash e `get_throttled=0x0` sono
PASS. `NRestarts=1` è il retry documentato dell'`ExecStartPre` prima della
creazione di `wayland-0`, seguito da avvio stabile a circa 35 fps. Le evidenze
sono in `evidence/release-immutability-20260920/pfd-recorder/`.

`PROD-005` è `DONE`; `PFD-001` resta `PARTIAL` per visuale/touch, terrain,
sorgenti reali e fault injection. `flight_approval=false` resta invariato.

## 14. Integrazione software candidata — 2026-09-21

Il branch `integration/main-ready-20260921`, costruito da `origin/main`, integra
con merge espliciti la release immutabile PFD/recorder, tredici correzioni
safety e il contratto dati offline AVIONIX. I conflitti sono stati risolti
conservando gli stati hardware più recenti e combinando i comportamenti
fail-closed: health/freshness, provenance SIM, air-data MAVLink, integratori,
isolamento fan-out, CAN bounds, profilo V-speed atomico, integrità recorder,
golden/HMI e vettori ICD.

Project Control accetta ora soltanto i sei stati canonici della Master Matrix;
le qualifiche composite storiche sono state normalizzate senza promuovere
evidenza hardware. Il validatore del contratto offline legge le soglie dalla
nuova autorità unica `FreshnessBudget`. Configurazione e build Windows MSVC/Qt
sono PASS (402/402). I test che richiedono eseguibili freschi restano soggetti
alla policy Windows Smart App Control; CI Linux è il gate autorevole prima del
merge finale.

Questo incremento è verificato soltanto software/documentazione. Non sono stati
modificati nodi, servizi, cablaggi, rete o hardware e non viene dichiarata
alcuna nuova approvazione al volo.

## 15. Topologia repository G0 — 2026-09-21

ADR-018 chiude `AVX-G0-02` e `AVX-G0-03` senza cancellazioni. GitHub
`sfuso68/efis-next-professional` è l'autorità condivisa; serverha ospita il
mirror locale primario e la working copy Claude; il bare Professional sul NAS
è l'archivio replicato; il bare Blackbox è temporaneo e non autorevole; il
precedente `git-repos/efis.git` è `LEGACY`.

I quattro `main` sono stati verificati allo stesso commit
`5891f754ee06948349f711c57c90e8700137d09f`. Il timer serverha→NAS e l'ultimo
oneshot sono risultati rispettivamente attivo e `SUCCESS`; la chiave NAS
dedicata ha rifiutato una richiesta shell. Restano aperti il restore drill, la
protezione fuori sito, il limite crittografico del DS214 e l'eventuale ritiro
controllato della copia Blackbox. Nessun runtime avionico dipende da questa
catena e nessuna verifica di volo è implicata.

## 16. Synthetic Vision C12 — correzione LOD/proiezione

Il branch `fix/synthetic-vision-lod-projection-v1`, costruito da `origin/main`
`195e730`, sostituisce il criterio LOD basato sul centro della tile con la
distanza minima dalla sua impronta geografica. Sul confine `N45E012`/
`N46E012` entrambe le tile restano ora high-detail; una isteresi 18/24 km evita
oscillazioni e il preload a 45 km riduce le mesh mantenute inutilmente attive.
Costanti e logica risiedono in un modello C++20 unico consumato dal QML tramite
adapter, con fail-closed per coordinate non finite.

Build completa MSVC/Qt, `efis-pfd-qt`, QML AOT e le quattro suite mirate
C9–C12 sono PASS. Il CTest Windows complessivo ha 54/64 PASS e dieci blocchi
noti Smart App Control/DLL, dichiarati senza mascherarli. PFD e MFD reali non
sono stati contattati; C12 è `VERIFIED_SW / VERIFY_HW`, non modifica le soglie
C9 e non chiude `AVX-G3-01` o `SV-001`.
