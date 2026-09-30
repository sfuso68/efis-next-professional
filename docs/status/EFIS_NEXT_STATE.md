# FUSO AVIONICS NEXT — Stato corrente sintetico

Aggiornamento: 2026-09-30
Repository autorevole: `sfuso68/efis-next-professional`
Baseline integrata esaminata: `origin/main` `ff75b242db0aa4075e19cecc07d5cfa779397e07`
C12 integrato: PR #29, source `a907a87ed58cb6899ccc43937cdfed35be79517a`, merge `61070b8d990c01ca58943a6370402623f4320ce7`
Stato prodotto: DEVELOPMENT / NON-CERT / NON APPROVATO AL VOLO

Questo documento contiene soltanto lo stato corrente. Lo storico completo fino
al 2026-09-19 è preservato senza perdita in
[`archive/EFIS_NEXT_STATE_FULL_THROUGH_2026-09-19.md`](archive/EFIS_NEXT_STATE_FULL_THROUGH_2026-09-19.md).
Il registro operativo riga-per-riga resta
[`EFIS_NEXT_MASTER_MATRIX.md`](EFIS_NEXT_MASTER_MATRIX.md); la release dei nodi
è governata esclusivamente da
[`EFIS_NEXT_ACTIVE_RELEASES.md`](EFIS_NEXT_ACTIVE_RELEASES.md).
Il quadro direzionale corrente con pianificazione R0, gate, workstream,
problemi e attività interattive è in
[`AVIONIX_NEXT_PROJECT_STATE_AND_PLAN_2026-09-22.md`](../project/AVIONIX_NEXT_PROJECT_STATE_AND_PLAN_2026-09-22.md).
Il rapporto del 2026-09-20 resta uno snapshot storico.

Durante il limite mensile GitHub comunicato il 2026-09-30, i lavori successivi
sono conservati soltanto in commit locali secondo
[`CHECKPOINT-2026-09-30-GITHUB-OFFLINE-DEVELOPMENT.md`](CHECKPOINT-2026-09-30-GITHUB-OFFLINE-DEVELOPMENT.md).
La baseline sopra indicata è l'ultima `main` remota osservata, non una
dichiarazione sullo stato remoto durante il periodo offline.

## 1. Quadro immediato

| Gate/area | Stato | Sintesi | ID autorevoli |
|---|---|---|---|
| Governance e fonti | PARTIAL | Nome prodotto `FUSO AVIONICS NEXT` approvato; alias tecnico `EFIS NEXT`; brief, indice, backlog e controllo progetto presenti; topologia Git chiusa da ADR-018; restore NAS→serverha PASS, cinque fonti e protezione off-site restano aperti | `PRJ-001`, `PRJ-002`, `AVX-G0-02`…`05` |
| Nodo sensori ASSO5 | HW-PARTIAL / BLOCKED-HW | RPi4 `.89` riconfermato read-only il 2026-09-29 con G1-C10 10/10 PASS; quattro frontali coerenti con 1×SM-I-018, 2×SM-I-011 e 1×8 Thermocouple DAQ, ma retro e label alimentatore mancanti: manifest ancora 0/4, DIP, mapping e calibrazione non verificati | `PRJ-002`, `AVX-G1-01`…`04` |
| ICD distribuito | FROZEN-SW / VERIFY-HW | DDP V2 ed EDP V1 congelati; replay x86-64 e tre ARM64 PASS; verticale reale aperto | `PRJ-003`, `AVX-G2-01`, `AVX-G2-02` |
| HMI SkyView/EMS | VERIFIED-SW / VERIFY-HW | Contratti e golden multi-risoluzione presenti; touch, pannelli e fault reali aperti | `PRJ-004`, `AVX-G3-03`, `AVX-G3-04` |
| PFD/MFD | PARTIAL / VERIFY-HW | PFD `pfdrec-7051a47-retention-v1` attivo e immutabile dopo archivio checksum, prima potatura, rollback e reboot PASS; root 71%/17,4 GB liberi, soak >=24 h ancora aperto; MFD `mfd-3432402-primary-v1` attivo ma con storico undervoltage `0x50000`; terrain e acceptance aperti | `PFD-001`…`003`, `MFD-001`, `NAV-003`, `PWR-001`, `VER-003` |
| EMS | PARTIAL / DEPLOY CONFORME | RPi4 riconfermato sulla release HMI/CAS `ems-62ba90b-hmi-cas-v1`, processo canonico e `NRestarts=0`; sorgente ancora SIM, input motore reale e limiti controllati mancanti | `EMS-002`…`004`, `PROD-005` |
| Pixhawk/GNSS/Airspeed | PARTIAL-HW | MAVLink read-only presente; M10 e DLVR non qualificati né calibrati | `PIX-001`, `PIX-002`, `GNSS-001`, `GNSS-002`, `AIR-001`, `AIR-002` |
| Blackbox/rete | PARTIAL / POWER VERIFY-HW / NAV FAIL-CLOSED | broker `blackbox-7655799-primary-v1` attivo, engine SIM e `NRestarts=0`; cinque undervoltage recenti senza errori NVMe/USB; drop-in NAV reale arresta exit 2 senza restart, ricevitori ancora assenti | `BBX-001`…`004`, `NET-001`, `NAV-003`, `PWR-001`, `PROD-005` |
| Server/NAS/Ground station | PARTIAL | GitHub/serverha/NAS allineati a `7aa45b66...`, replica ristretta e restore PASS; CA/lifecycle, off-site e ritiro copia temporanea aperti | `SRV-001`…`003`, `GS-001`…`003`, ADR-018 |
| Alimentazione | VERIFY-HW / URGENT | MFD con due e Blackbox con cinque undervoltage kernel recenti; entrambi `get_throttled=0x50000`; PSU/cavi e prove brownout non qualificati | `PWR-001`, `PROD-002`, `VER-003` |

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
- Il job Linux ASan/UBSan ha concluso con successo sei run consecutivi su
  `main` (#174, #177, #180, #189, #199 e #202). La tolleranza
  `continue-on-error` e' stata rimossa: una regressione rilevata dai sanitizer
  blocca ora la CI. Questa e' una verifica software x86-64 e non equivale a
  test ARM64 o hardware (`CORE-001`).

## 5. Ultime verifiche hardware che restano valide

- PFD `.45`, MFD `.49`, EMS `.36`, Blackbox `.47` e serverha `.6` sono stati
  riconfermati read-only il 2026-09-29; il sensor node `.89` è stato acquisito
  lo stesso giorno con G1-C10. NAS `.22` è stato letto e ripristinato tramite
  serverha. Il portatile `.39` non è stato ricontrollato in questo blocco
  (`PFD-001`, `MFD-001`, `EMS-002`, `BBX-003`, `SRV-001`, `PRJ-002`).
- G2 ARM64 ha prodotto validator e 11/11 test PASS su PFD, MFD ed EMS con
  digest coincidenti (`PRJ-003`, `AVX-G2-01`).
- La catena EDP SIM Blackbox→EMS ha mostrato LIVE, STALE, LOST, fallback e
  recovery; non prova la sorgente CAN motore reale (`EMS-002`, `BBX-003`).
- Pixhawk/MAVLink read-only e fan-out monitor-only sono stati osservati; la
  prova Wi-Fi Ground Station resta degradata per timing (`PIX-002`, `NET-001`).
- Il 2026-09-29 MFD e Blackbox hanno mostrato `get_throttled=0x50000`; i log
  kernel confermano rispettivamente due e cinque undervoltage recenti. PFD ed
  EMS erano `0x0`, senza qualificare PSU/cavo (`PWR-001`).
- Nella finestra successiva il PFD ha archiviato su serverha e verificato
  checksum 49.597 record legacy (11.525.831.016 byte), liberando 11 GiB;
  recorder e imagery hanno superato l'acceptance reale. La Blackbox non mostra
  errori NVMe/USB correlati ai transienti, ma `bbxnav` ha 157.031 restart per
  ricevitori assenti (`PFD-001`, `NAV-003`, `PWR-001`).
- Il 2026-09-30 il PFD è stato ricontrollato in sola lettura: recorder
  `NRestarts=0`, 9.120 file/32 GiB e root 84%/9,2 GiB liberi. La retention
  candidata è software-only e non modifica questo stato (`PFD-001`, `BBX-001`).

## 6. Blocchi e rischi prioritari

1. PFD retention: quota e riserva fail-closed sono implementate e testate solo
   software; prima dell'attivazione occorrono autorizzazione alla copia dei 32
   GiB, checksum, release ARM64/rollback e soak con monitoraggio margine
   (`PFD-001`, `BBX-001`).
2. Alimentazione: undervoltage recenti su MFD e Blackbox; nessuna prova
   brownout/load controllata e nessuna PSU finale qualificata (`PWR-001`,
   `PROD-002`).
3. Inventario fisico ASSO5: quattro Sequent, DIP, alimentatore, cablaggi e
   fotografie posteriori non acquisiti (`PRJ-002`, `AVX-G1-02`).
4. HMI: touch, leggibilità, sunlight/night, frame-rate, termica e fault su
   pannelli reali non accettati (`PRJ-004`, `AVX-G3-03`).
5. Synthetic vision: la causa LOD al confine è corretta e verificata software
   da C12, ma terreno vicino, proiezione e prestazioni non sono ancora accettati
   sui due RPi5 (`NAV-003`, `AVX-G3-01`).
6. Motore reale: CAN, MGL, limiti Rotax e calibrazioni privi di fonte completa
   e di prova end-to-end (`EMS-003`, `EMS-004`, `AVX-G3-02`).
7. Safety: Emergency Assist resta consultivo; checklist e servi sono bloccati
   da fonti, identificazione, interlock e HIL (`PAN-001`…`004`).
8. Radionavigazione Blackbox: il restart storm è chiuso, ma il drop-in è ancora
   esterno alla release e ricevitori, frequenze, antenna, decoder e annunci non
   sono qualificati (`NAV-003`).

## 7. Prossima sequenza controllata

1. Completare il soak >=24 h della retention PFD già attiva, verificando
   `NRestarts=0`, status V3 crescente, rotazione reale e spazio >=12 GiB;
   analizzare inoltre lo startup retry PFD e lo stall meteo SIM di 10,1 s
   (`PFD-001`, `BBX-001`).
2. Qualificare alimentatori/cavi MFD e Blackbox con piano brownout e carico
   approvato (`PWR-001`, `PROD-002`).
3. Quando disponibili, completare foto posteriori e label dell'inventario
   ASSO5 senza probing o modifica DIP (`PRJ-002`, `AVX-G1-02`).
4. Solo dopo review/merge, eseguire HMI harness su PFD/MFD/EMS e verticale G2
   reale con nuova autorizzazione (`AVX-G2-02`, `AVX-G3-03`).
5. Dopo un deployment C12 autorizzato, creare sessioni C10 dal commit installato e completare
   C11/C9 su PFD e MFD con confronto visuale della transizione N45/N46
   (`AVX-G3-01`, `SV-001`).
6. Integrare la supervisione `bbxnav` nella prossima release, poi inventariare
   e qualificare ricevitori/frequenze/RF e verificare lo stato NAV non valido
   esplicito sui display (`NAV-003`).

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

La PR #29, integrata in `main` al commit `61070b8`, sostituisce il criterio LOD
basato sul centro della tile con la
distanza minima dalla sua impronta geografica. Sul confine `N45E012`/
`N46E012` entrambe le tile restano ora high-detail; una isteresi 18/24 km evita
oscillazioni e il preload a 45 km riduce le mesh mantenute inutilmente attive.
Costanti e logica risiedono in un modello C++20 unico consumato dal QML tramite
adapter, con fail-closed per coordinate non finite.

Build completa MSVC/Qt, `efis-pfd-qt`, QML AOT e le quattro suite mirate
C9–C12 sono PASS. Il CTest Windows complessivo ha 54/64 PASS e dieci blocchi
noti Smart App Control/DLL, dichiarati senza mascherarli. PFD e MFD reali non
sono stati contattati né aggiornati; C12 è `VERIFIED_SW / VERIFY_HW`, non modifica le soglie
C9 e non chiude `AVX-G3-01` o `SV-001`.

## 17. Configuration/restore pack Git — 2026-09-25

Sul branch `work/git-restore-pack-v1`, derivato da `origin/main` `3066f42`, è
stato implementato il primo incremento software successivo al consolidamento
del piano: un restore drill Git isolato e fail-closed per la replica NAS.

Sono presenti la configurazione controllata
`config/recovery/git-restore-drill-v1.json`, il tool
`tools/recovery/git_restore_drill.py`, il runbook
`docs/deployment/GIT_NAS_RESTORE_DRILL.md` e sei test sintetici. Il tool rifiuta
destinazioni già esistenti e URL contenenti credenziali; clona senza hardlink,
esegue `git fsck --full --strict`, confronta `main` con uno SHA GitHub completo
fornito dall'operatore, inventaria tag e oggetti, verifica i file minimi e può
eseguire il controllo Project Control. Il report JSON viene scritto
atomicamente e non può essere sovrascritto.

**Verificato solo software:** i sei test locali PASS coprono restore riuscito,
SHA errato, tag mancante, destinazione esistente, credenziali nell'URL e SHA
corto. **Verificato su hardware reale:** nulla in questo incremento; NAS e
serverha non sono stati contattati e nessun servizio o runtime è stato
modificato. **Resta aperto:** esecuzione interattiva I-10 su serverha contro il
bare NAS, conservazione dell'evidence reale, protezione fuori sito e decisione
se ritirare in seguito la copia temporanea Blackbox. La chiave di replica
write-only (`git-receive-pack`) non è sufficiente: serve una credenziale di
restore separata e limitata a `git-upload-pack`. `PRJ-001` resta `PARTIAL`
e nessuna approvazione al volo è implicata.

## 18. Runbook unificato HMI + Synthetic Vision — 2026-09-25

È stato preparato il runbook documentale
`docs/deployment/HMI_SYNTHETIC_VISION_NODE_RUNBOOK.md` per gli incrementi
I-04/I-05. Il documento lega senza sovrapporli C8/A20/P20 e C9/C10/C11/C12,
assegna HMI a PFD/MFD/EMS e Synthetic Vision ai soli PFD/MFD RPi5, congela
ordine, gate, artefatti, soglie, stop condition e rollback. I riferimenti di
banco registrati sono PFD `.45`, MFD `.49` ed EMS `.36`, da riconfermare al
preflight e non usati come identità autorevoli.

Questa modifica è soltanto documentale. Nessun nodo è stato contattato, nessuna
release è stata installata, nessun servizio o runtime è stato modificato e non
sono stati eseguiti reboot, fault Ethernet, cache manipulation o benchmark.
Non esiste nuova evidenza hardware: I-04/I-05 restano `NOT-RUN`, `PRJ-004`
resta `VERIFY-HW`, `SV-001` resta `PARTIAL` e `flight_approval=false`.

## 19. Piano release gemella PFD/MFD C12 — 2026-09-27

È stato preparato il piano software non eseguibile che coordina i due piani di
transizione immutabile PFD e MFD prima delle sessioni HMI/Synthetic Vision. Il
tool `prepare_paired_display_release.py` accetta solo piani V1 già validati,
richiede lo stesso SHA sorgente completo e lo stesso digest della policy,
verifica identità, piattaforma, servizio e link attivo dei due ruoli, lega gli
input con digest SHA-256 e produce l'ordine MFD→PFD con rollback PFD→MFD.

**Implementato:** schema JSON fail-closed, coordinatore atomico senza
overwrite, dodici gate, otto evidenze minime, runbook e integrazione CI.
**Verificato solo software:** otto test mirati PASS e controllo sintattico
Python; sono coperti mismatch commit/policy, identità o piattaforma errate,
tentativi di autorizzazione, comandi mutanti, release ID duplicati e mutazioni
degli input. **Verificato su hardware reale:** nulla; PFD e MFD non sono stati
contattati e nessuna release, cache, tile, rete o servizio è stata modificata.
**Aperto:** generare i piani dai manifesti realmente candidati, autorizzare la
finestra, attivare MFD poi PFD, eseguire reboot/rollback e completare C10/C11/C9
con firma operatore. `AVX-G3-01` resta `OPEN`; `PFD-001`, `MFD-001` e `SV-001`
non sono promossi. `flight_approval=false` e `execution_authorized=false`.

## 20. Qualification batch G1–G4 — 2026-09-27

I cinque punti successivi del piano sono stati trasformati in un contratto
unico non eseguibile: mapping/calibrazione ASSO5, verticale G2/replay,
qualifica air-data Matek ASPD-DLVR, endurance/recovery Blackbox e NAV/TAWS
fail-closed. Il contratto contiene cinque pacchetti e 50 controlli; conserva
esplicitamente `execution_authorized=false`, `flight_approval=false` e una
decisione complessiva `NO_GO_PENDING_INTERACTIVE_HARDWARE_EVIDENCE`.

**Implementato:** autorità JSON, validatore fail-closed, dieci test di
regressione, runbook di verifica e gate CI. Il pacchetto rifiuta canali o
coefficienti ASSO5 inventati, un trasporto G2 anticipato, scritture air-data,
GS usata come IAS, fault distruttivi sulla Blackbox di produzione e qualunque
stato TAWS safe quando terrain manca o è scaduto.

**Verificato solo software:** validatore PASS con 5 pacchetti/50 controlli;
dieci test mirati PASS. **Verificato su hardware reale:** nulla in questo
incremento; nessun nodo è stato contattato e nessun servizio, cablaggio,
parametro o dataset è stato modificato. **Aperto:** tutte le dipendenze e le
prove interattive elencate nel runbook. Gli stati `PRJ-002`, `PRJ-003`,
`AIR-001`, `BBX-001`, `NAV-003` e `SV-003` non sono promossi.

## 21. Product contracts batch — 2026-09-27

Sono stati preparati cinque contratti software fail-closed, per complessivi 50
controlli, relativi a `EMS-004`, `PROD-001`, `FPV-002`, `VOI-003` e `VER-002`:

- limiti EMS separati per Rotax 912 UL/ULS con seriale, manuale, revisione e
  pagina obbligatori; il profilo corrente resta `SIMULATION` e
  `INDICATIVE_UNVERIFIED`;
- Standard/Professional su una sola codebase, senza cambio profilo in volo e
  con rifiuto delle feature ignote;
- quattro feed FPV su RPi4 dedicato, separato dall'EMS e senza bridge fra VIDEO
  e FLIGHT-DATA;
- voce naturale offline solo PTT e consultiva, senza trasmissione, comando o
  attuazione;
- HIL deterministico confinato, senza fault su reti/storage production e senza
  uscite attuatore.

Il contratto, il validatore e i test verificano soltanto i confini software.
Fonti motore, manifesti di prodotto, nodo/camere VIDEO, catena audio e fixture
HIL non sono stati verificati. `execution_authorized=false`,
`flight_approval=false` e tutti i pacchetti terminano `NO_GO`.

## 22. Product readiness tools batch — 2026-09-27

I cinque contratti prodotto dispongono ora di registri macchina separati e di
un validatore incrociato: fonti motore 912 UL/ULS, capability Standard e
Professional, capacità FPV a quattro feed, pacchetti fraseologia/asset voce e
sei scenari HIL deterministici.

**Implementato:** cinque configurazioni JSON, validatore fail-closed, test di
mutazione, documento di verifica e gate CI. **Verificato solo software:** il
validator richiede due motori non attivabili, due profili non approvati,
quattro feed con misure `NOT_RUN`, zero pacchetti voce approvati e sei scenari
HIL con digest ancora `UNRESOLVED`. **Verificato su hardware reale:** nulla;
nessun nodo è stato contattato. **Aperto:** acquisizione e review di tutte le
fonti, manifesti, misure, periferiche e fixture. Gli stati `EMS-004`,
`PROD-001`, `FPV-002`, `VOI-003` e `VER-002` restano `TODO` e nessun registro
autorizza deployment o volo.

## 23. Product evidence intake batch — 2026-09-27

I cinque registri prodotto dispongono ora anche del livello di intake che
precede ogni acquisizione interattiva. Ogni template è legato al registro
autorevole tramite SHA-256 e parte obbligatoriamente da `NOT_RUN`.

**Implementato:** intake fonti motore per 912 UL/ULS; intake approvazione SKU
Standard/Professional; intake benchmark FPV a quattro feed; intake
fraseologia/voce per `it-IT` e `en-GB`; intake per i sei run HIL. Il validatore
blocca drift, evidenze anticipate, approvazioni premature, bridge/Wi-Fi VIDEO,
trasmissione automatica, reti o storage production e uscite attuatore.

**Verificato solo software:** validatore PASS su cinque intake; otto test di
mutazione PASS, oltre ai 23 test di regressione dei contratti, registri e
Project Control. **Verificato su hardware reale:** nulla; nessun nodo, camera,
interfono, motore o fixture è stato contattato. **Aperto:** compilazione degli
intake da parte dell'operatore, acquisizione degli artefatti, review tecnica e
sessioni hardware autorizzate separatamente. `EMS-004`, `PROD-001`, `FPV-002`,
`VOI-003` e `VER-002` restano `TODO`; `execution_authorized=false`,
`hardware_verified=false` e `flight_approval=false`.

## 24. Product review gates batch — 2026-09-27

I cinque intake prodotto sono ora seguiti da gate di revisione indipendenti per
fonti motore, SKU/release, benchmark FPV, voce/cabina ed esecuzione HIL. Ogni
gate è legato tramite SHA-256 al proprio intake e congela ruoli, finding e
divieti prima di qualunque decisione operativa.

**Implementato:** cinque manifesti review gate, validatore incrociato, nove
test di mutazione, documento di verifica e gate CI. **Verificato solo
software:** digest drift, path traversal, evidence/reviewer prematuri,
attivazione limiti, deployment SKU, bridge VIDEO, trasmissione automatica e
HIL su produzione/attuatori sono rifiutati. **Verificato su hardware reale:**
nulla; nessun nodo o dispositivo è stato contattato. **Aperto:** compilare gli
intake, creare bundle immutabili, acquisire firme indipendenti ed eseguire le
sessioni autorizzate. Gli stessi cinque requisiti restano `TODO`; tutti i gate
sono `PREPARED_SW_NOT_RUN`, `hardware_verified=false`,
`deployment_allowed=false` e `flight_approval=false`.

## 25. Product evidence packagers batch — 2026-09-27

I cinque gate prodotto dispongono ora di template risultati `NOT_RUN` e di un
confezionatore comune che accetta soltanto acquisizioni esplicitamente
confermate dall'operatore. Il bundle risultante è immutabile per nome, registra
dimensione e SHA-256 di ogni artefatto e resta `PACKAGED_PENDING_REVIEW`.

**Implementato:** template motori, SKU/release, FPV, voce/cabina e HIL;
validazione fail-closed di path, digest, identità, timestamp, set artefatti e
decisione; packaging atomico; test e CI. **Verificato solo software:** tutti i
cinque domini producono bundle sintetici pending review; overwrite, path
escape, file mancanti/vuoti, digest drift e claim safety vengono rifiutati.
**Verificato su hardware reale:** nulla; nessun nodo o dispositivo è stato
contattato. **Aperto:** compilazione sul banco, raccolta delle evidenze reali e
review indipendente. `EMS-004`, `PROD-001`, `FPV-002`, `VOI-003` e `VER-002`
restano `TODO`; `hardware_verified=false`, `deployment_allowed=false` e
`flight_approval=false`.

## 26. Independent product evidence review — 2026-09-27

Il bundle prodotto dispone ora di schema chiuso e validazione autonoma di
sorgenti, path, dimensioni e SHA-256. Cinque template di revisione indipendente
congelano ruoli e finding già definiti nei review gate.

**Implementato:** schema bundle; validatore byte-per-byte; template review per
motori, SKU, FPV, voce e HIL; validatore con tre reviewer distinti, firme,
timestamp e decisione coerente con i finding; test e CI. **Verificato solo
software:** bundle e review sintetici dei cinque domini PASS; tamper, drift,
firme errate, reviewer duplicati, finding incoerenti e claim operativi sono
rifiutati. **Verificato su hardware reale:** nulla. **Aperto:** generare i
bundle reali e farli revisionare dalle autorità definite. Anche una review
positiva termina `EVIDENCE_ACCEPTED_FOR_NEXT_GATE`; gli stessi requisiti
restano `TODO`, senza deployment o approvazione al volo.

## 27. Product evidence session manager — 2026-09-27

La catena prodotto dispone ora di un comando unico per inizializzare una
sessione, misurarne la completezza, finalizzare il bundle, aprire la review e
congelarne l'esito. Ogni output finale rifiuta overwrite e conserva il legame
SHA-256 con il livello precedente.

**Implementato:** cinque comandi operativi e verifica outcome; stato intermedio
`PREPARED_NOT_RUN`; report di artefatti mancanti/non sicuri; test end-to-end sui
cinque domini e gate CI. **Verificato solo software:** pipeline sintetica
completa, review positive e negative, immutabilità e tamper detection.
**Verificato su hardware reale:** nulla. **Aperto:** eseguire le sessioni con
artefatti e reviewer reali. L'esito massimo resta
`NO_GO_PENDING_GATE_DISPOSITION`; nessun requisito cambia stato.

## 28. Product gate disposition — 2026-09-28

La review indipendente può ora essere sottoposta a una disposizione formale
separata. Cinque template congelano autorità, review gate e confine con il
change control; il record finale è immutabile e verificabile tramite digest.

**Implementato:** template per cinque domini; collegamento all'outcome;
separazione fra reviewer e tre autorità decisionali; firme e timestamp;
accettazione/rifiuto; record immutabile; test e CI. **Verificato solo
software:** pipeline sintetica positiva e negativa, tamper, overlap di ruoli e
tentativi di acquisire autorità operativa. **Verificato su hardware reale:**
nulla. **Aperto:** outcome reali, autorità reali e successiva richiesta formale
di change control. Anche l'accettazione termina
`NO_GO_CHANGE_CONTROL_NOT_CREATED`; nessun requisito cambia stato.

## 29. Product change-control request — 2026-09-28

La disposizione positiva può ora alimentare una richiesta formale separata.
Cinque template congelano scope, analisi d'impatto, rollback, verifica e ruoli
di intake; il record conclusivo è immutabile e verificabile tramite digest.

**Implementato:** template per cinque domini; collegamento alla disposizione;
sette aree d'impatto obbligatorie; separazione delle autorità; firme, record,
test e CI. **Verificato solo software:** pipeline sintetica completa, scope e
analisi mancanti, path, digest, firma, overlap di ruoli e tamper.
**Verificato su hardware reale:** nulla. **Aperto:** richieste e firme reali,
decisione CCB e piano di implementazione separato. Il massimo esito resta
`NO_GO_IMPLEMENTATION_NOT_AUTHORIZED`; nessun requisito cambia stato.

## 30. Product CCB decision — 2026-09-28

Una richiesta change-control completa può ora essere sottoposta a decisione
CCB formale. Cinque template impongono quattro ruoli unanimi, indipendenza da
tutte le persone delle fasi precedenti, firme e record immutabile.

**Implementato:** template per cinque domini; collegamento alla richiesta;
approve/reject/defer; quorum e segregazione; record, test e CI. **Verificato
solo software:** pipeline sintetica completa, voto non unanime, overlap di
ruoli, digest/firme, claim operativi e tamper. **Verificato su hardware
reale:** nulla. **Aperto:** richieste e board reali, piano di implementazione
separato ed eventuale successiva autorizzazione. Anche l'approvazione termina
`NO_GO_IMPLEMENTATION_PLAN_NOT_CREATED`; nessun requisito cambia stato.

## 31. Product implementation plan — 2026-09-29

Una decisione CCB positiva può ora alimentare un piano d'implementazione
separato. Cinque template impongono baseline, scope, work package, controlli
rischio, verifica, rollback, deployment e review indipendente.

**Implementato:** template per cinque domini; collegamento CCB; validazione
completa e record immutabile; test e CI. **Verificato solo software:** pipeline
sintetica, baseline/scope invalidi, path traversal, pacchetti incompleti,
firme, overlap, claim operativi e tamper. **Verificato su hardware reale:**
nulla. **Aperto:** compilazione con dati reali e review di autorizzazione
separata. Il massimo esito resta
`NO_GO_IMPLEMENTATION_AUTHORIZATION_NOT_CREATED`; nessun requisito cambia
stato. Stato dell'arte complessivo:
`docs/project/AVIONIX_NEXT_STATE_OF_ART_2026-09-29.md`.

## 32. G1 capture nodo e intake visuale parziale — 2026-09-29

Il nodo `ems-sensor` è stato riconfermato in sola lettura tramite il capture
G1-C10: identità RPi4/aarch64, I2C controller-only, rete e health hanno
superato 10/10 controlli. Il report immutabile è conservato in
`evidence/g1-node-capture-ems-sensor-20260929/` con SHA-256
`956413c4faf489ceff6a6238490f48ae269a3ddc9f5b726223d950d86edaa155`.

Quattro fotografie frontali identificano provvisoriamente 1×SM-I-018 V2.1,
2×SM-I-011 V6.0 e una 8 Thermocouple DAQ V2.1 coerente con SM-I-019. Il quinto
fotogramma non mostra la label dell'alimentatore e contiene più possibili
sorgenti. Le fotografie non sono quindi state importate: mancano i quattro
retro, la label elettrica, la conferma della singola sorgente, operatore/UTC e
review dei DIP. `AVX-G1-01` resta `IN_PROGRESS`, `AVX-G1-02` resta `OPEN`, il
manifesto rimane 0/4 e la decisione è `NO_GO`. Nessun probing, vendor read,
cablaggio, DIP o uscita è stato modificato.

## 33. Gate credenziale restore NAS — 2026-09-29

È stata eseguita una verifica read-only preliminare del restore I-10. GitHub
`main` e il bare repository di replica su `serverha` coincidono a
`7aa45b66fc71824eb0dd4b1e6ffe898ecfcdb897`; il timer di replica risulta
attivo e l'ultima unità osservata ha esito `success`. Su `serverha` è però
presente soltanto l'identità write-only usata dal push verso il NAS. Il test
batch `git ls-remote` dal server verso il bare NAS è stato rifiutato con
`Permission denied`.

È stata quindi installata una nuova chiave distinta, limitata al solo
`git-upload-pack` del bare professionale e senza shell/forwarding/PTY. La chiave
write-only di replica non è stata ampliata. Il restore I-10 è stato eseguito in
`/home/sfuso/restore-drills/20260929T180632Z/repository` con exit code `0`:
clone isolato, `git fsck`, confronto SHA, oggetti, file minimi e Project Control
sono tutti PASS; il main ripristinato è `7aa45b66...`. L'albero e il report
sono conservati, con evidence JSON SHA-256
`134d08da0b1a3b30f0e236839e026344fffd53030698f593816edcb8458a1abc`.

**Verificato su host reale:** confinamento della credenziale, lettura NAS e
restore completo NAS→serverha. **Resta aperto:** protezione fuori sito e prova
da un secondo supporto. Il checkpoint
`CHECKPOINT-2026-09-29-NAS-RESTORE-ACCESS-GATE.md` registra l'evidenza;
`PRJ-001` resta `PARTIAL` per le altre dipendenze governance, mentre I-10 è
`VERIFIED-HOST / PASS`. Nessun runtime avionico è stato contattato.

## 34. Audit read-only dei cinque nodi — 2026-09-29

PFD, MFD, EMS, Blackbox e serverha/NAS sono stati ricontrollati senza modifiche.
Il bundle `evidence/five-node-health-audit-20260929/README.md` conserva i dati
osservati. PFD grafico, MFD, EMS e broker Blackbox sono in esecuzione dai
percorsi release attesi; serverha/NAS sono coerenti a `7aa45b66...`.

L'esito non è però nominale. Sul PFD il filesystem è al 100%, il recorder è in
`auto-restart` con 101.015 restart e l'imagery fallisce `ENOSPC`; il display
resta attivo come unità systemd utente. L'EMS usa correttamente
`ems-62ba90b-hmi-cas-v1`, valore più recente del registro precedente. MFD e
Blackbox riportano `throttled=0x50000`; il kernel mostra due undervoltage MFD
il 23 settembre e cinque Blackbox il 28–29 settembre.

**Verificato su hardware reale:** processi, release, storage, rete, termica e
storico alimentazione. **Non modificato:** servizi, timer, file, record e
release. **Aperto:** recupero conservativo PFD previa autorizzazione e qualifica
PSU/cavi MFD/Blackbox. Nessun dato viene promosso ad approvazione al volo.

## 35. Recovery storage PFD e diagnosi Blackbox — 2026-09-29

La finestra autorizzata ha chiuso il guasto operativo storage del PFD senza
perdere i record legacy. Recorder e imagery sono stati fermati; la directory
`/home/efis/efis-next/records`, distinta dal percorso release attivo, è stata
copiata su serverha. Il confronto checksum successivo ha verificato 49.597
file e 11.525.831.016 byte con zero differenze. Solo dopo questo PASS la copia
PFD è stata rimossa; il root è passato dal 100% all'81%, con 11 GiB liberi.

**Verificato su hardware reale PFD:** recorder `active`, PID `969557`, doppio
campione `NRestarts=0`; scrittura nel percorso release attivo; imagery exit 0,
1.326 tile riusate/verificate e zero failure; timer attivo. La chiave temporanea
serverha→PFD è stata rimossa. **Aperto:** quota/retention, soak e 27 gap tile,
oltre alle acceptance HMI/terrain già previste.

La Blackbox è stata diagnosticata in sola lettura. I cinque transienti di
undervoltage durano circa due secondi e sono seguiti da normalizzazione; broker
`NRestarts=0`, nessun errore NVMe/USB correlato. Questo non qualifica PSU o
cavo. Separatamente, `bbxnav.service` con ricevitori assenti ha raggiunto
157.031 restart perché usa `Restart=always`; non è stato modificato.

**Verificato su hardware reale Blackbox:** storico kernel, servizi, NVMe, USB,
termica e contatori. **Aperto:** qualifica elettrica e correzione fail-closed
del servizio NAV. Evidenza:
`evidence/pfd-storage-recovery-blackbox-power-20260929/README.md`.

## 36. Supervisione bbxnav fail-closed — 2026-09-29

Il servizio NAV legacy sulla Blackbox è stato corretto con un drop-in che non
modifica `ExecStart`, sorgente `ricevitori`, destinazione `vcan0`, utente o
capability. La policy sostituisce il restart illimitato con
`Restart=on-failure`, impedisce il riavvio su exit 2 e limita gli altri errori
a tre tentativi in 60 secondi.

**Verificato solo software:** validatore PASS, 4/4 test PASS, rifiuto delle
modifiche a comando/sorgente, file unito verificato da `systemd-analyze` su
ARM64. **Verificato su hardware reale:** drop-in installato, processo terminato
con exit 2 per ricevitori assenti, unità `failed`, `NRestarts=0` in due
campioni; broker e `bbxlog` restano `active` con zero restart. Nessun dato NAV
simulato e nessun CAN reale sono stati abilitati.

**Aperto:** il drop-in è configurazione esterna alla release immutabile;
ricevitori SDR, frequenze, antenna, qualità RF, decoder e annunci display non
sono qualificati. `NAV-003` resta `PARTIAL`. Evidenza:
`evidence/bbxnav-fail-closed-20260929/README.md`.

## 37. Retention recorder PFD — 2026-09-30

Il riempimento è stato misurato nuovamente in sola lettura: il recorder PFD è
stabile (`NRestarts=0`) ma il buffer attivo è già cresciuto a 9.120 file/32 GiB
e il root è all'84% con 9,2 GiB disponibili. Gli ultimi segmenti sono circa
3,65 MB/minuto, quindi senza controllo il guasto ENOSPC si ripeterebbe.

**Implementato:** retention nativa oldest-first con quota 24 GiB, riserva 12
GiB, protezione di almeno 120 segmenti recenti, preflight senza cancellazioni
quando il target è impossibile, selezione dei soli file regolari
`flight-*.csv` e status V3 con telemetria. La unità release contiene parametri
espliciti; la policy resta disabilitata se non configurata.

**Verificato su hardware reale:** il recorder è stato fermato per ottenere una
fotografia stabile. I 9.208 file, pari a 33.777.901.735 byte, sono stati copiati
in `/home/sfuso/archives/pfd-active-records-pre-retention-20260930/records` su
serverha. Un secondo `rsync -aHnc --delete --itemize-changes` ha prodotto zero
differenze e un file diff vuoto con SHA-256
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
Un primo processo rsync rimasto orfano dopo l'interruzione della sessione è
stato arrestato per PID; il delta finale ha trasferito zero file e zero byte
prima del confronto completo conclusivo.

Il commit completo `7051a47839389f739f2d1e0ad22b385173299f61` è stato
archiviato in `/tmp`, verificato tramite SHA-256, compilato nativamente ARM64
con GCC 14.2 e Qt 6.8.2. `efis_core_tests`,
`efis_recorder_storage_failure_tests` ed
`efis_release_supervision_asset_tests` sono PASS. La release dual-role
`pfdrec-7051a47-retention-v1` è stata validata per PFD e RECORDER, resa
root-owned/read-only e attivata atomicamente. Gli hash attivi sono
`a64edcf2...28854` per `efis-pfd-qt` e `4cb136ec...87ca5` per
`efis-recorder`.

La prima potatura ha eliminato soltanto 2.222 segmenti completati oldest-first,
pari a 8.008.825.406 byte; restavano 6.986 segmenti completati, 25.769.076.329
byte e 17.422.503.936 byte disponibili. Il rollback reale verso
`pfdrec-d52e377-primary-v1` e il ritorno alla candidata sono PASS. Dopo reboot
il boot ID è cambiato in `b52439a1-a261-464d-9005-6025682d1182`; PFD e recorder
sono attivi dai binari canonici, il recorder V3 avanza, DDP è LIVE a circa 20
Hz, la root è al 71% con circa 17,4 GB disponibili e
`vcgencmd get_throttled=0x0`. Un campione successivo ha osservato una seconda
potatura autonoma post-reboot di 12 segmenti/43.890.672 byte, con recorder
`NRestarts=0` e spazio ancora pari a circa 17,4 GB.

**Verificato solo software:** oltre ai test ARM64 citati, i test MSVC di core,
storage process-level e supervisione release sono PASS. Application Control
Windows ha bloccato due eseguibili non pertinenti e un test non pertinente non
ha terminato nella finestra; non sono dichiarati PASS. La build ARM64 conserva
un warning GCC preesistente in `src/protocol/ddp_v2.cpp` e warning Qt su target
plugin mancanti: restano da revisionare.

**Aperto:** il soak >=24 h non è ancora maturato, quindi non è DONE. Al primo
boot il PFD ha eseguito uno startup retry (`NRestarts=1`) ed è poi rimasto
stabile con `Result=success`; manca il log del primo processo. È inoltre stato
osservato uno stall di 10,1 s nel callback `weather_simulation`, da analizzare
separatamente. Restano aperti 27 gap tile e le acceptance visuale/touch/terrain.
`PFD-001` e `BBX-001` restano `PARTIAL`; il prodotto resta
DEVELOPMENT/NON-CERT e `flight_approval=false`.

## 38. Writer meteo simulato asincrono PFD — 2026-09-30

Lo stall `weather_simulation` di 10,1 secondi e' stato ricondotto al percorso
sincrono che comprimeva il frame PNG e lo committava su disco nel callback
GUI. La generazione logica dello scenario resta sul thread grafico, mentre
compressione e commit atomico sono ora eseguiti da un writer dedicato. Una
sola scrittura puo' essere attiva; le richieste concorrenti vengono aggregate
in un refresh successivo, evitando code illimitate.

**Implementato:** writer `QThread`, back-pressure single-frame, pubblicazione
del nuovo URL soltanto dopo commit riuscito, errori fail-visible, telemetria
separata GUI/worker e percorsi diagnostici configurabili.

**Verificato solo software:** build Qt/MSVC del test e di `efis-pfd-qt` PASS.
Il test mirato inietta 400 ms di latenza I/O, verifica ritorno del callback in
meno di 150 ms e pubblicazione asincrona del frame; CTest PASS in 0,46 s.
Il relink finale dopo sola formattazione e' compilato PASS, ma il nuovo hash
del test e' stato bloccato da Windows Device Guard e non viene contato come
seconda esecuzione PASS.

**Verificato su hardware reale:** nulla. La release PFD attiva non e' stata
modificata e lo stall reale non e' ancora chiuso. Dopo review/merge servono
build ARM64, release immutabile con rollback e sessione PFD reale con meteo
SIM e controllo degli eventi `callback_stall`. Il soak retention >=24 h,
startup retry, 27 gap tile e acceptance visuale/touch/terrain restano aperti.
`PFD-001` resta `PARTIAL`; il prodotto resta DEVELOPMENT/NON-CERT e
`flight_approval=false`. Evidenza:
`evidence/pfd-weather-async-20260930/README.md`.

## 39. Pacchetto rollout ARM64 del writer meteo — 2026-09-30

**Implementato:** procedura dedicata e fail-closed per preparare, attivare e
verificare dopo reboot la release dual-role
`pfdrec-3c7bb4d-weather-async-v1`, fissata al merge completo
`3c7bb4d5495e1ce9f582dd185be48b9563993e48`. La preparazione verifica l'hash
dell'archivio sorgente, pretende almeno 12 GiB liberi, compila PFD/recorder e
il test meteo su ARM64, esegue CTest e genera il manifest immutabile. La fase
root pretende come base `pfdrec-7051a47-retention-v1`, prova un rollback reale
e usa un drop-in temporaneo per iniettare 400 ms di latenza nel writer. Il
gate richiede telemetria del worker lento e fallisce se il callback GUI meteo
supera la soglia. Una terza procedura verifica servizi, binari, DDP, recorder,
throttling e assenza dello stall dopo reboot.

**Verificato solo software:** contratto statico dei tre script, pin di commit,
hash archivio, doppio ruolo, rollback, switch atomico e gate sullo stall sono
coperti dai test di supervisione release. La procedura dettagliata e' in
`docs/deployment/PFD_WEATHER_ASYNC_RELEASE.md`.

**Verificato su hardware reale:** nulla. L'accesso SSH da questa workstation
al PFD non e' disponibile; nessuna build ARM64, attivazione, modifica runtime,
prova meteo o reboot acceptance e' stata eseguita. `PFD-001` resta `PARTIAL`,
la release attiva resta `pfdrec-7051a47-retention-v1` e il prodotto resta
DEVELOPMENT/NON-CERT con `flight_approval=false`.

## 40. Gate sanitizer Linux bloccante — 2026-09-30

La baseline inizialmente non bloccante e' stata revisionata sui sei run
consecutivi di `main` #174, #177, #180, #189, #199 e #202. In ogni run il job
`linux-sanitizers` ha completato con successo la configurazione Debug, la
compilazione e CTest con AddressSanitizer e UndefinedBehaviorSanitizer.

**Implementato:** rimosso `continue-on-error` dal job. ASan/UBSan diventa un
gate obbligatorio per push e pull request senza modificare flag, test o timeout.

**Verificato solo software:** evidenza GitHub Actions x86-64 dei sei run e
validazione locale della sintassi workflow. **Verificato su hardware reale:**
nulla. Restano fuori dal perimetro ARM64, nodi di bordo e approvazione al volo.
La futura migrazione dell'immagine `ubuntu-latest` andra' sorvegliata come
rischio infrastrutturale, non aggirata rendendo nuovamente facoltativo il gate.
Evidenza: `evidence/core-sanitizer-baseline-20260930/README.md` (`CORE-001`).

## 41. Preparazione release immutabile Blackbox NAV — 2026-09-30

La policy fail-closed `bbxnav`, già verificata sul nodo reale ma ancora esterna
alla release, è stata integrata nel contratto della candidata
`blackbox-46e56a9-nav-supervision-v1`. Il manifesto copre broker e drop-in NAV;
la futura attivazione collega systemd all'asset della release corrente e prova
rollback congiunto di binario e policy.

**Implementato:** preparazione ARM64 fissata a commit e SHA-256, manifesto a
due artefatti, attivazione con backup del drop-in precedente, switch atomico,
exit 2 senza restart, rollback reale e verifica post-reboot. La preparazione
resta non mutante e termina con `EXECUTION_AUTHORIZED=false`.

**Verificato solo software:** validazione della policy, test di supervisione,
contratto statico dei tre script e sintassi shell. **Verificato su hardware
reale:** nulla in questo incremento. La release attiva resta
`blackbox-7655799-primary-v1` con il drop-in esterno già verificato.

**Aperto:** build ARM64, attivazione, rollback e reboot acceptance richiedono
una finestra separata; ricevitori, RF, decoder e annunci display restano non
qualificati. `NAV-003` e `BBX-003` restano `PARTIAL`; il prodotto resta
DEVELOPMENT/NON-CERT e `flight_approval=false`. Evidenza:
`evidence/blackbox-nav-immutable-preparation-20260930/README.md`.

## 42. Hardening offline cache meteo edge — 2026-09-30

Il servizio edge METAR/TAF non usa più il `mtime` modificabile del file come
fonte primaria di freshness e non considera più implicitamente corrente un
timestamp futuro. Il record deve avere schema V1, payload oggetto e
`fetched_utc` timezone-aware; gli ICAO sono confinati a quattro caratteri
alfanumerici ASCII.

**Implementato:** stati deterministici `CURRENT` (<=10 min), `STALE` (11–20
min), `EXPIRED` (>20 min), `CLOCK_SKEW`, `NO_REPORT`, `NO_DATA` e `INVALID`;
età, provenienza, soglie e visibilità esplicite; `safety_use_allowed=false` in
ogni stato; status V2 aggregato per stazione. Cache scadute o con clock skew
non sono presentabili come osservazioni correnti.

**Verificato solo software:** test locali per confini 600/601/1200/1201 s,
schema/payload/timestamp corrotti, futuro, assenza report, path traversal ICAO,
riepilogo stati e resolver pinning. **Verificato su hardware reale:** nulla.
Provider, licenze, TLS/CA, timer, payload reali, deployment ARM64 e consumer
RPi5 restano aperti; `BBX-004` e `WX-001` restano `PARTIAL`.

## 43. Allowlist fail-closed overlay radar — 2026-09-30

La proprietà QML `available` non accetta più implicitamente ogni stato diverso
da `EXPIRED`: richiede ora un'immagine `Ready` e uno stato esplicitamente
ammesso tra `CURRENT`, `STALE` e `SIMULATED`. Stati ignoti, `NO DATA`, errori e
future estensioni non riconosciute restano quindi non disponibili per default.

**Verificato solo software:** il validatore responsive controlla proprietà,
tre stati ammessi e uso dell'allowlist; la suite locale del contratto passa.
**Verificato su hardware reale:** nulla. Provider, georeferenziazione, cache,
clock skew e resa RPi5 restano aperti; `WX-002` resta `PARTIAL`.
