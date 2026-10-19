# FUSO AVIONICS NEXT — Stato corrente sintetico

Aggiornamento: 2026-10-19
Repository autorevole: `sfuso68/efis-next-professional`
Baseline integrata esaminata: `main` `4aea1e4edf8678c180e04171813d842ca6ff58a0` (PR #49); candidate applicative ancora a0be9a5
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
Il periodo offline è storico: la baseline in testa è stata riconfermata dopo
il merge PR #48; non implica che la stessa versione sia distribuita ai nodi.

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

## 44. Freshness radar C++ a precisione di secondo — 2026-09-30

Il servizio Qt non classifica più l'età dopo una divisione intera per minuti,
che manteneva erroneamente `CURRENT` fino a 10 minuti e 59 secondi. Il nuovo
classificatore usa i secondi: `CURRENT` fino a 600, `STALE` da 601 a 1200 ed
`EXPIRED` da 1201; timestamp oltre 5 secondi nel futuro producono `CLOCK SKEW`.
Il valore di età pubblicato non espone inoltre minuti negativi.

**Verificato solo software:** test Qt sui confini -6/-5, 600/601 e 1200/1201
secondi, oltre al precedente test del writer asincrono. **Verificato su hardware
reale:** nulla. Download, timestamp del provider, resa grafica e transizioni
reali su RPi5 restano aperti; `WX-002` resta `PARTIAL`.

## 45. Isolamento cache e risposte radar asincrone — 2026-09-30

Il servizio radar associa ora ogni file di cache a prodotto, zoom, X e Y. Al
cambio di prodotto o tile carica soltanto la corrispondenza esatta, altrimenti
cancella il frame e dichiara `NO DATA`; non mantiene più un'immagine del
contesto precedente con una nuova etichetta. Le callback di rete conservano il
contesto originale: una risposta superata può essere archiviata nella propria
chiave, ma non può aggiornare il frame visibile dopo un cambio di posizione,
prodotto o passaggio alla simulazione. Il prodotto è confinato all'allowlist
VMI/SRI/CAPPI 3000; stringhe path-like e coordinate negative non generano un
percorso di cache. CAPPI 3000 resta solo training e l'uscita dalla simulazione
riporta il prodotto live a VMI, evitando richieste live non supportate.

**Verificato solo software:** build Qt/MSVC e test servizio radar PASS; test
deterministici dimostrano chiavi distinte VMI/SRI, normalizzazione CAPPI e
rifiuto di prodotto path-like e coordinate negative.
**Verificato su hardware reale:** nulla. Race con server controllato, cache
persistente, provider, georeferenziazione e RPi5 restano aperti; `WX-002`
rimane `PARTIAL`.

## 46. Harness radar con server controllato — 2026-09-30

Il target `efis_weather_service_tests` include ora un server HTTP locale
controllato che riceve due richieste reali Qt Network, conserva la VMI iniziale,
risponde prima alla SRI corrente e consegna intenzionalmente per ultima la VMI
ormai superata. Il test richiede che entrambe le risposte restino nelle rispettive
cache `product/zoom/tile`, che soltanto SRI diventi il frame visibile e che la
risposta VMI tardiva non possa sostituirlo.

L'endpoint radar sostitutivo è disponibile esclusivamente compilando
`EFIS_WEATHER_TEST_HOOKS`; il prodotto continua a costruire il servizio con
l'endpoint DPC HTTPS fisso. Il workflow Qt compila ed esegue ora esplicitamente
`efis_weather_service_tests` come gate. Il primo run ha rilevato una dipendenza
implicita dalle impostazioni del prodotto; il test è stato isolato impostando
esplicitamente `VMI`. Il run pull request Linux `36786267067` è quindi PASS per
`linux-qt-golden`, `linux-core` e `linux-sanitizers`, includendo l'inversione
controllata delle risposte VMI/SRI.

**Verificato solo software:** build Qt/MSVC e test end-to-end Linux CI. **Verificato
su hardware reale:** nulla. Provider reale, validazione del payload,
georeferenziazione, cache persistente e resa RPi5 restano aperti; `WX-002`
rimane `PARTIAL`.

## 47. Correzione leggibilità PFD da pannello reale — 2026-10-03

Una fotografia del MFD reale ha localizzato quattro difetti della pagina PFD:
lo stato `AIRSPEED FAIL` non era dominante nel punto di lettura IAS, ALT/VS
avevano gerarchia e separazione insufficienti, la bussola inferiore esponeva
etichette senza un arco graduato chiaramente distinguibile e i dati NAV da
6–8 px si perdevano sul terreno. Il meteo compatto invadeva inoltre la fascia
della prua superiore. La fotografia e il relativo digest sono conservati sul
branch di evidenza `work/mfd-hardware-baseline-20261003`, commit `08203d4`.

**Implementato:** flag locale `IAS FAIL` fail-closed; finestre IAS/ALT e VS più
opache, separate e leggibili; fascia ALT/VS ampliata; bussola inferiore con arco
reale ±35°, tacche ogni 5° e valori ogni 10°; pannello NAV contrastato da
340×48 px con testo principale da 10 px e secondario da 9 px; scala distanza
spostata fuori dal pannello; overlay meteo ridotto e abbassato sotto la fascia
heading. Il contratto responsive e il validatore sono stati aggiornati senza
modificare autorità di comando, sorgenti dati o logica di validità.

**Verificato solo software:** `qmllint` senza errori; compilazione completa
Qt/MSVC Release di 413 step, inclusa generazione QML AOT, PASS; validatore
SkyView responsive PASS; 21 test Python mirati HMI/SkyView PASS. La cattura
offscreen Windows non è stata prodotta perché Smart App Control ha bloccato
l'eseguibile locale non firmato. La CI è stata inoltre estesa con una cattura
Linux/offscreen PFD-SkyView separata nelle risoluzioni 1024×600, 1280×800 e
1920×1080 e con artefatto di review a conservazione limitata. Il primo
artefatto ha rivelato che la fixture
SkyView ometteva nove flag di validità e produceva quindi `ATTITUDE INVALID`,
VS `FAIL` e bussole nascoste pur con CI verde. La fixture e il validatore sono
stati corretti in modo fail-closed. Il nuovo artefatto del commit `35059b9`,
generato il 2026-10-04T06:53:02Z, è stato revisionato alle tre risoluzioni:
assetto, scala heading superiore, arco HSI inferiore, dati NAV, IAS, ALT e VS
sono presenti; `ATTITUDE INVALID` e i falsi `FAIL` non compaiono; le finestre
ALT e VS non si sovrappongono. A 1024×600 il layout resta denso ma utilizzabile;
1280×800 e 1920×1080 hanno margini e gerarchia migliori. SHA-256 immagini:
`f72cc2a955dd4d4c742d01badab7f1f28321879561fba058f52b8619116a91e2`,
`f171aef9e51803b1cf2f7b8cbe4ca115b0b1eecd08526cc9c165a1088d81f82a` e
`024cb4c414f3d6bac2851cf2dc1a9a231aec2caf666cbfbf8da1527082e9bf48`.
I run GitHub Actions `37183809227` (push) e `37183811270` (pull request) sono
PASS per `linux-qt-golden`, `linux-core` e `linux-sanitizers`. Il target Qt/MSVC
è stato inoltre ricompilato incrementalmente dopo la correzione della fixture.

**Verificato su hardware reale:** soltanto la release precedente che ha
originato il difetto e la fotografia di riferimento; questa correzione non è
stata installata né osservata su RPi5. Cattura Linux/Qt, review visuale
1024×600 e CI sono concluse solo a livello software. **Aperto:** restano build
ARM64, release immutabile, rollback e acceptance touch/leggibilità sul MFD
reale prima di qualunque distribuzione PFD. `PFD-002` e `MFD-001` restano
`PARTIAL`; nessuna approvazione al volo.

## 48. Preparazione release ARM64 MFD per la leggibilità PFD — 2026-10-04

È stato formalizzato il gate successivo alla review Linux della sezione 47,
senza contattare o modificare il nodo `pi5`. La candidata MFD è fissata al
commit `76afead639599a08cb187bf93f1127c84a4df9ed`; il relativo archivio Git
riproducibile misura 50.548.958 byte e ha SHA-256
`b318d9da3914826cdc4c5a07b5803be25d17973b4685df64633646a053ee740b`.

**Implementato:** procedura dedicata di preparazione nativa ARM64 senza
switch; attivazione root con identità nodo, architettura, riserva disco,
manifesto, supervisore unico, assenza di servizi display concorrenti,
`get_throttled=0x0`, porta DDP 49002, switch atomico e recupero automatico;
rollback reale verso `mfd-3432402-primary-v1`; ritorno alla candidata e
verifica post-reboot read-only. Il drop-in MFD azzera inoltre la condizione
legacy sul percorso non canonico `build-rpi5` e controlla il binario verificato
`bin/efis-pfd-qt`.

**Verificato solo software:** sintassi dei tre script con Bash; 37 test release,
supervisione e paired-display PASS (un test symlink non applicabile su Windows
è SKIP); Project Control `--check` PASS; l'archivio è stato generato due volte
con identico digest e dimensione. La preparazione dichiara
`EXECUTION_AUTHORIZED=false`; la verifica reboot dichiara
`VISUAL_TOUCH_ACCEPTANCE=NOT_RUN` e `FLIGHT_APPROVAL=false`.

**Verificato su hardware reale:** nulla in questo incremento. La release attiva
resta `mfd-3432402-primary-v1`; non sono stati eseguiti build ARM64, switch,
restart, reboot, rollback o nuova osservazione grafica sul nodo.

**Aperto / bloccante:** il precedente `get_throttled=0x50000` impedisce
intenzionalmente l'attivazione finché alimentatore/cavo non sono qualificati e
un avvio pulito non restituisce `0x0`. Dopo autorizzazione di banco occorrono
trasferimento archivio, build/test ARM64, attivazione, rollback, reboot e
acceptance operatore a 1024×600 su IAS, ALT/VS, bussole, NAV, overlay, touch e
terrain. Solo dopo un PASS MFD si potrà preparare il PFD primario. `MFD-001`,
`PFD-002`, `PWR-001` e `VER-003` restano aperti/PARTIAL.

## 49. Planning e cruscotto progetto — 2026-10-10

PRJ-001 resta PARTIAL. Implementati audit documentale e planning aggiornato
in `docs/project/FUSO_AVIONICS_NEXT_STATE_AND_PLAN_2026-10-10.md`, generatore
di viewer offline ricercabile/filtrabile con evidence/rischio/next/acceptance
e download handoff. Fonte unica: matrice; nessuna modifica di stato dalla UI.
Il digest identifica lo snapshot; nessuna acquisizione live o automazione
ricorrente installata. GitHub non raggiungibile nel controllo locale odierno:
main, CI, merge e replica NAS non riconfermati. Hardware: NOT_RUN.
Restano da verificare browser interattivo e pubblicazione serverha.
Verificato solo software: generazione dashboard PASS, confronto deterministico
`generate_dashboard --check` PASS, `generate_status --check` PASS, 2/2 test
unitari PASS (determinismo/embedding sicuro e rifiuto colonne incomplete).
Correzione del conteggio sezione 48: 37 test totali, 36 PASS e 1 SKIP,
non 37 PASS più 1 SKIP.

### Pubblicazione serverha — 2026-10-10

Installati sotto utente sfuso viewer read-only `avionix-dashboard.service`
e refresh `avionix-dashboard-refresh.timer` ogni cinque minuti, con linger
già attivo. Bind esclusivo 127.0.0.1:8766, accesso via tunnel SSH; nessuna
modifica Caddy/firewall/servizi avionici. Il viewer serve solo index/status,
non espone repository né credenziali, usa no-store e mostra errori/stale.
Verifica reale server: servizi active, HTTP index/status PASS, bind e timer
PASS. Reboot e verifica visuale browser NOT_RUN.
Fonte attuale mirror locale `7aa45b66…`, datato 29 settembre, NON aggiornato
da GitHub. Main remoto letto oggi `1cc0a786…`: sincronizzazione incompleta.
Stato esposto LOCAL_MIRROR_ONLY/github_verified=false. Chiave deploy dedicata
generata su serverha, autorizzazione GitHub read-only ancora necessaria.
Nessun commit/push/merge o cambiamento di stato requisiti: PRJ-001 PARTIAL.

Completamento: deploy key autorizzata e verificata; host key confrontata con
GitHub Docs e fissata in file dedicato. Refresh legge direttamente main via
SSH read-only in `.local/state/avionix-dashboard/github.git`, senza toccare
mirror NAS. Fetch fail-closed senza force; GitHub main `1cc0a78682741872e19082ae84dae936370e3d7e`
osservato e servito con `GITHUB_SYNC_OK`, Result=success/ExecMainStatus=0.
SSH isolato con `-F /dev/null` evita errore permessi della configurazione
systemd nel servizio confinato; nessuna modifica SSH globale.
Il timer aggiorna main ogni cinque minuti: branch/PR non integrati non sono
inclusi. Nessuna qualifica hardware implicita e reboot server NOT_RUN.

Audit successivo read-only: `SERVERHA_NAS_AUDIT_2026-10-10.md` conferma
GitHub/cruscotto `1cc0a786…`, serverha backup/NAS `7aa45b66…` arretrati,
timer NAS active/success e 114 GB disponibili su serverha. Allineamento
backup e restore aggiornato restano aperti; nessuna sovrascrittura.

## 50. Pipeline backup corretta — 2026-10-12

PRJ-001 resta PARTIAL. Implementata pipeline GitHub read-only -> serverha
-> NAS per main e tag, senza force/prune/delete. Bundle preventivo verificato
con digest, copie refs storiche, lock esclusivo, controllo ancestry server
e NAS, rifiuto tag divergenti, CAS main e verifica preservazione altre refs.
Wrapper precedente conservato `efis-git-sync-nas.pre-github-20261012`;
timer esistente 15 minuti ripristinato active. Nessun nodo avionico modificato.

Verificato su infrastruttura reale: main `1cc0a786…` allineato; restore
isolato NAS read-only, fsck completo e SHA-256 di matrice, stato e CMakeLists
PASS. Seconda esecuzione invariata PASS senza nuovo bundle; mantiene data
della prova restore precedente separata dal checked_utc. Evidenza completa
`backup-pipeline-20261012.json`. Primo restore FAIL per uso della chiave
receive-pack, poi configurazione corretta con alias efis-nas-restore.

Aperto: restore da bundle preventivo non eseguito (solo bundle verify);
reboot/power-loss, fault test automatizzati, quota/retention bundle e off-site
non verificati. Backup dei nuovi branch GitHub non incluso: pipeline conserva
gli storici NAS ma replica automaticamente solo main/tag. Non è full mirror.

## 51. Blocco autonomo controllo e regressione — 2026-10-12

Implementati gate CI dashboard, 7 guardie test backup, riserva 2 GiB senza
pruning, poll main/tag e monitor backup/errori/stale con data restore separata.
Registro attività/coda: `docs/project/AUTONOMOUS_EXECUTION_2026-10-12.md`.
Verificato software: 16 test controllo/dashboard/backup e 57 regressioni
G1/G2/qualification PASS. Serverha: 8 test PASS e servizi monitor aggiornati;
esecuzione backup completa PASS, endpoint backup HTTP PASS e servizi active.
GitHub CI non verificata; nuova
acceptance visuale browser NOT_RUN. Nessun nodo avionico/sensore modificato.
PRJ-001/002 restano PARTIAL, PRJ-003 VERIFY-HW: nessuna verifica offline
equivale a dato reale o qualifica di banco. Coda A1–A7 resta da eseguire.

## 52. Restore bundle e HTTP — 2026-10-13

PRJ-001 PARTIAL invariato. Implementato verificatore bundle con SHA-256
prima dell'import, directory temporanea, import tutte refs, fsck completo,
confronto refs annunciate e digest file. Bundle pre-migrazione `eb5c9326…779b3`
recupera main `7aa45b66…` e 41 refs: PASS reale serverha, evidenza
`bundle-restore-20261013.json`, nessuna copia attiva sostituita.

Software su serverha: 3 test bundle (digest errato, bundle invalido,
round-trip refs archiviate) e 5 test HTTP PASS. Backup JSON non valido dà
503, POST rifiutato, path non permessi 404, cache no-store e dettagli interni
esclusi. Servizio monitor active aggiornato; browser/reboot NOT_RUN.
20 regression locali HMI responsive/EMS state/product review PASS, non
acceptance visuale hardware. Gate CI esteso; run GitHub non verificata.

A2 verificato; A1 HTTP/fault verificato ma packaging completo aperto.
A3 policy proposta in `docs/project/BACKUP_POLICY_PROPOSAL_2026-10-13.md`:
branch, retention/off-site NON attivati, nessuna potatura.
Nessun collaudo o modifica avionica in questo incremento.

## 53. Packaging e regressione cache meteo — 2026-10-16

PRJ-001/WX-001/WX-002 restano PARTIAL. Implementati manifest deterministico
di nove file, verifica SHA-256/allowlist/symlink, installazione versionata
idempotente e runbook `docs/project/DASHBOARD_PACKAGE_RUNBOOK.md`.
Pacchetto `8361a0440dfcd858127725e21f0f1791baf96d81f3ef52a3908857a1becb6e34`.
Due test packaging locali PASS; staging serverha verificato e ripetuto,
senza cambio release attiva. Attivazione atomica/rollback suite intera aperti:
non dichiarare A1 completamente chiuso.

Meteo: aggiunte fixture JSON corrotto, override stato/provenance, limite
clock skew -5/-6 s. Dodici test edge-weather offline su serverha PASS;
nessun servizio/provider interrogato. Python embedded Windows non carica
_ssl: test meteo locale bloccato dal runtime, non prova fallimento prodotto.
Il binario Qt preesistente Windows fallisce: `controlled radar server did not
complete both responses`. Build corrente/provenance del binario non
riconfermate: WX-002 NON PASS e occorre ricompilazione/test Linux controllato.
Provider reali, payload/georeferenziazione e ARM64 rimangono aperti.
CI aggiornata per packaging/edge weather, run remota non verificata.

## 54. Chiusura attivazione e test radar aggiornato — 2026-10-16

Implementata attivazione transazionale suite monitor/refresh/backup con
manifest verificato, lock, snapshot drop-in, restart/health HTTP versione,
rollback automatico di configurazione e controllo HTTP della versione
precedente. Tre unit test PASS. Reale serverha: drill ripristina `f6a356…`
e verifica HTTP; attivazione definitiva `79c511…` ACTIVE, backup PASS e timer
attivi. Vecchi file/release conservati. Evidenza `activation-radar-20261016.json`.
Chiusa A1 nel perimetro di aggiornamento di installazione già configurata;
bootstrap credenziali, reboot e fault power-loss restano separati.

Radar Qt: ricompilati test e weather_service.cpp dal checkout corrente con
MSVC/Qt 6.11.1; esecuzione diretta PASS e CTest ripetuto 3/3 PASS fuori
sandbox. Nessuna modifica codice radar necessaria. Il fallimento precedente
non si riproduce fuori sandbox; la build sandbox riporta C1902 PDB e il test
HTTP sandbox falliva. Non è dimostrata una regressione prodotto. Digesti
binario/sorgenti registrati; chiusa la verifica Qt desktop richiesta.
WX-002 resta PARTIAL: provider/georeferenziazione/ARM64 ancora non provati.
PRJ-001 resta PARTIAL per off-site/retention e ulteriori gate.
CI aggiornata, run GitHub non verificata. Nessun nodo avionico modificato.

## 55. Restante piano autonomo A3–A7 — 2026-10-17

Registro `docs/project/AUTONOMOUS_REVIEW_2026-10-17.md`, evidenze sotto
`docs/status/evidence/autonomous-20261017/`. Implementati runner ripetibile
per gruppi con timeout/log e inventario backup read-only. 37 contratti,
42 product/review, 37 HMI (dopo fast path PNG) e 21 controllo PASS; sei nuovi
test runner/storage PASS. Test nativi ricompilati: nove Windows PASS; test
PFD profili bloccato dalla policy applicazioni Windows, non dal codice.
Stesso sorgente/hash in container GCC14.2/Qt6.8.2 Linux x64 senza rete PASS.
Non disabilitata alcuna protezione Windows. Container immagine/archivio privati
conservati su serverha, nessuna distribuzione a pannelli o cambi servizi.

Correzione WX-002: validazione PNG 256×256 decodificato e limite 4 MiB prima
di cache/pubblicazione, cache corrotta/symlink rifiutata, buffer limitato,
fixture reali/negative. Build Qt Windows PASS e CTest 3/3 PASS. Solo SW,
NON distribuita ai display. Miglioramento tool golden PNG None: stessi pixel
e stato riga precedente preservati, test misto None/Up PASS.

Inventario reale serverha: nove bundle/498.108.584 byte e 121.493.479.424
byte filesystem liberi al controllo; nessuna selezione/cancellazione.
Stati requisiti invariati: manuali/seriali UL/ULS non acquisiti, limiti non
attivati; G1/G2 live, ARM64, provider/georef, coverage e acceptance HMI aperti.
CI estesa ma run GitHub non verificata. Il report Windows iniziale rimane
INCOMPLETE, con consolidamento Linux separato, senza nascondere il blocco OS.

## 56. Consolidamento tramite PR #48 — 2026-10-17

Autorizzazione utente: integrazione del branch `work/project-dashboard-planning-v1`
in main tramite PR #48. Baseline sorgente del lotto `21ac032…`, main prima
dell'integrazione `1cc0a786…`; 16 commit/59 file, inclusa la legibilità PFD e
preparazione MFD non ancora distribuite. Confronto GitHub: ahead 16/behind 0,
nessun conflitto. PR: https://github.com/sfuso68/efis-next-professional/pull/48.

Implementato/verificato software: sezioni 49–55 e relative evidenze. Verificato
su infrastruttura reale: monitor/backup/restore serverha e NAS, container test
Linux isolato; nessuna nuova verifica su hardware avionico. Gli esiti finali
CI e il commit di merge sono tracciati nella PR GitHub, non dedotti dai test
locali. Non integrare con gate falliti o bypassando le protezioni.

Il merge aggiorna il codice e la documentazione condivisi, NON le release
PFD/MFD/EMS/Blackbox. Dopo integrazione, cruscotto e backup acquisiscono main
tramite i timer; qualunque deploy avionico resta separato dai gate hardware.
Stati requisiti invariati; restano aperti i punti elencati nella sezione 55.

## 57. Rollout richiesto sui nodi disponibili — 2026-10-17

Autorizzata la distribuzione di main a0be9a5. Inventario SSH reale su quattro
nodi, manifesti e processi riconfermati in `evidence/rollout-20261017/`.
MFD e Blackbox hanno storico alimentazione 0x50000; EMS sano ma richiede sudo
interattivo; sensore .89 irraggiungibile e comunque vincolato a G1. Il PFD sano
attende il canary MFD previsto dal piano: nessun bypass e nessun reboot per
azzerare l'evidenza dei transienti.

Implementato solo helper di staging versionato come evidenza; nessun sorgente
avionico modificato. Sul MFD compilati nativamente renderer, test radar e
test profili da main: CTest 5/5 PASS (radar, profili, golden tools, contratto,
responsive). Sono verifiche software ARM64 sul nodo reale, NON acceptance
grafica del pannello. Il preparatore iniziale fallisce perché il rollback
storico non contiene il validatore; log preservato. Completion separata con
validatore corrente controlla entrambi i manifesti/file PASS, ldd risolto.

Preparata candidata `mfd-a0be9a5-main-v1` con simulation_mode=true e
flight_approval=false, non ancora sigillata root-owned o attivata. PID MFD 1395
e NRestarts 0 invariati. Nessuna release attiva aggiornata, nessun restart,
bus, rete, firmware o pulizia modificati. Restano: qualificazione power,
finestra root EMS, inventario sensore, attivazione MFD con rollback/reboot e
acceptance visuale, poi PFD dual-role con recorder/retention e gli altri ruoli.
WX-002 resta PARTIAL; PROD-005 DONE riguarda il contratto storico, non questo
rollout. Stato e conteggi globali non promossi.

## 58. Preparazione autonoma delle quattro candidate — 2026-10-18

Main remoto riconfermato a0be9a5. Evidenza `evidence/rollout-20261018/`.
Nessun sorgente avionico modificato; helper di staging/test e log conservati.
MFD: regressioni estese 15/15 PASS e test storage recorder 1/1 PASS. Su MFD
RPi5 compilati anche recorder e broker; PFD+recorder e Blackbox trasferiti ai
destinatari in staging. Manifesti candidata/rollback, hash e dipendenze PASS
sui destinatari, senza eseguire il broker candidato o cambiare current.

EMS: build nativa RPi4/Qt6.8.2 con Make (Ninja assente), 7/7 test PASS;
manifesti candidata/rollback PASS e startup QML offscreen isolato PASS, con
SIM/EDP non configurato e cattura software. Nessuna prova degli indicatori
GPU o del pannello fisico implicita. Errori helper di import Python e wrapper
SSH conservati; completamento con import root esplicito e verifica indipendente,
senza modifiche alle release precedenti. Warning GCC/QML nei log non nascosti.

Le quattro candidate a0be9a5 sono solo staging, non sigillate root-owned o
attivate. Servizi/retention/configurazioni operative invariati; PFD NRestarts 1
storico e recorder/MFD/EMS/Blackbox 0. Serverha viewer/timer attivi, report
backup/restore PASS riconfermato (non un nuovo restore completo).

Restano bloccanti: qualificazione power MFD/Blackbox, canary MFD prima del
PFD, sudo EMS, inventario G1 e accesso sensore .89, rollback/reboot/visuale di
ogni nuova release. Nessuna interazione CAN/Pixhawk, modifica rete o potatura.
Non chiusi provider radar/georeferenziazione, calibrazione o motori reali.
Gli stati globali e l'avanzamento rimangono invariati; staging e test software
sui Raspberry non equivalgono a collaudo avionico o autorizzazione al volo.

## 59. Preflight e sblocco attivazioni — 2026-10-19

Implementato checker read-only con pin candidati/rollback, identità fisica,
ruoli completi PFD+recorder, PID/eseguibili e gate alimentazione/privilegi.
15 unit test Windows e 15 Linux ARM64 PASS. Aggiunto alla CI Project Control;
esito della nuova CI non ancora verificato. Runbook
`docs/deployment/ACTIVATION_UNBLOCK_2026-10-19.md`, evidenza
`evidence/activation-preflight-20261019/`.

Esecuzione reale: PFD controlli automatici PASS, canary MFD manuale aperto;
MFD/Blackbox bloccati soltanto da power history; EMS da sudo non interattivo.
Manifesti ed eseguibili verificati; Blackbox richiede lettura root limitata
del link /proc/PID/exe, non esecuzione privilegiata del checker. Sudo locale
operatore non concede automaticamente il timestamp alla sessione SSH agente.
Sensore .89 irraggiungibile/G1 separato: non è un blocco dei display SIM.

Rischio individuato in sola lettura: attivatore MFD storico pinned a76afead
arma recovery prima del gate power e potrebbe riavviare anche senza switch.
Non eseguito; il nuovo checker non attiva né arma quel trap. L'attivatore
transazionale aggiornato e il suo recovery restano da preparare/verificare
prima della finestra root: questo blocco non li dichiara completati.

Serverha cruscotto e replica NAS già su main 4aea1e4, report restore PASS e
refs preservati. Nessuna release attivata, servizio riavviato, rete, hardware,
bus o retention modificati. PWR-001 VERIFY-HW e PROD-005 contratto storico DONE
invariati; prima azione operatore: identità PSU/cavi e metodo di alimentazione
MFD/Blackbox, poi piano power sotto carico e finestra banco/acceptance.

## 60. Attivatore EMS pronto per comando locale — 2026-10-19

Richiesta esplicita: preparare l'attivazione EMS e fornire il comando sudo;
l'agente NON ha eseguito --activate. Tool standalone
`tools/deployment/activate_ems_a0be9a5.py` consegnato in
`/home/pi/activate_ems_a0be9a5.py`, hash e comando fissati in
`docs/deployment/EMS_A0BE9A5_ACTIVATION.md`. Nuovo target previsto
`/opt/efis-next-ems-releases/ems-a0be9a5-main-v1`, non ancora creato.

Implementati: pin host/modello, source archive e binario, manifesti, profilo,
unità e validatore globale root; lock, sigillatura da sorgente verificato,
fsync e switch atomico; candidata→precedente→candidata e recovery armato solo
prima del primo stop. Il comando root legge e verifica in memoria i byte
dell'attivatore con Python -I; nessun helper da candidata scrivibile eseguito.
Unità/environment, calibrazioni e contatori non modificati dall'attivatore;
snapshot file ore prima della transizione. Non esegue reboot.

Verifica solo software: 17 test fault/orchestrazione su Linux ARM64 PASS;
Windows 16 PASS/1 SKIP symlink. Verifica reale READ-ONLY: unità/environment,
profilo identico e codice runtime store invariato tra old/new, manifesti,
PID, socket, archivio e preflight PASS; lanciatore hash-check --check-only PASS.
Evidenza `evidence/ems-activation-20261019/`. Il servizio precedente conserva
PID 949 e NRestarts 0. Nessuna prova reale di stop/switch/sealing/recovery,
rollback drill, reboot o pannello dichiarata: attende esecuzione operatore.

Questa preparazione riguarda EMS soltanto: attivatore MFD aggiornato e gate
power MFD/Blackbox restano aperti. Il primo avvio deve essere al banco con
Blackbox/Ethernet attivi; in FAIL non ripetere o cancellare staging alla cieca.
Esito operativo successivo e registro release andranno aggiornati solo dopo
una nuova osservazione del nodo. Nessuna promozione di stato o uso in volo.

## 61. EMS attivato dall'operatore, verifiche prima del reboot — 2026-10-19

ID coinvolto PROD-005. Operatore dichiara eseguito il comando locale; controllo
SSH read-only alle 19:18 UTC conferma current sigillato root/read-only
`/opt/efis-next-ems-releases/ems-a0be9a5-main-v1`, sorgente a0be9a5,
PID 46812 e NRestarts 0. Manifesto, binario, profilo, unità/environment,
validatore e release precedente superano i pin. Verificate 1363 entry sigillate.

Verifica su hardware reale: result.json PASS e rollback_drill PASS; journal
mostra candidata PID 46751 → precedente 46780 → candidata 46812, tutti con
startup e EDP LIVE da BLACKBOX 47 SIM. Socket 10.77.10.12:49003 del PID
corretto. Tredici heartbeat della candidata finale 55.1–58.5 Hz, snapshot circa
20 Hz; nessun fault runtime/QML fra quelli cercati nel journal del PID finale.
Power 0x0, temperatura 59.9 °C. File ore esterno presente, 213 byte; nessun
reset o ripristino dati eseguito dal controllo. Non è una misura di accuratezza
dei sensori né una qualifica prestazionale o di power-loss recovery.

Evidenza locale originale root:
`/var/lib/efis-next/activation-evidence/ems-a0be9a5-20261019T191627560963Z`;
report osservato e hash dei file in `evidence/ems-activation-20261019/post-activation.json`.
L'agente non ha attivato nuovamente, riavviato o modificato il nodo.
Restano NOT_RUN la verifica visiva/touch e reboot acceptance della nuova release.
Chiedere conferma pannello prima di concordare un reboot; flight_approval=false.
Nessuna promozione degli altri rollout o dei gate alimentazione MFD/Blackbox.

## 62. EMS: reboot autorizzato e avvio automatico verificato — 2026-10-19

PROD-005. Operatore conferma pannello leggibile e funzionante, touch compreso,
prima del reboot; consenso esplicito al riavvio del solo EMS. Un primo guard
SSH mal quotato è fallito senza mutazioni; successivo systemctl reboot exit 0.
Primo tentativo SSH durante avvio in timeout, poi nodo di nuovo raggiungibile.

Hardware reale: boot ID da ec0eed02-052f-426b-804e-53be70646081 a
e61b90b8-4aa4-4e23-86b4-b5dbf13fe6d5; servizio enabled/active/running,
PID 948, NRestarts 0, ExecMainStatus 0. Current resta sigillato a0be9a5.
Pin di manifesti candidata/precedente, binari, profilo, unità/environment,
validatore, root/read-only 1363 entry e socket EDP del PID corretto PASS.
Startup EGLFS/Qt6.8.2 ed EDP LIVE BLACKBOX 47 SIM presenti nel nuovo boot;
quattro heartbeat osservati 49.5–56.2 Hz, snapshot circa 20 Hz. Nessun fault
QML/runtime fra quelli cercati. Power 0x0, temperatura 58.9 °C; file ore
esterno presente, non resettato dall'agente. Report in
`evidence/ems-activation-20261019/reboot-acceptance.json`.

Verifica automatica di reboot PASS; conferma visiva/touch pre-reboot registrata
come dichiarazione operatore, non osservazione agente. Controllo pannello dopo
reboot ancora da confermare; nessuna prova di sensori reali, power-loss,
accuratezza ore o qualifica al volo dedotta. Flight approval rimane false.
Nessun altro nodo, firmware, rete o sorgente avionico modificato.

## 63. EMS acceptance operatore e nuovo controllo power — 2026-10-19

PROD-005/PWR-001. Operatore conferma schermata e touch EMS funzionanti anche
dopo reboot: chiusa la riconferma visiva di questo rollout SIM, distinta dalle
misure automatiche già conservate. Non è una qualifica sensori o al volo.

Preparato attivatore MFD nuovo standalone con pin e recovery armato soltanto
prima dello stop; correzione atomica futura della ConditionPathExists legacy
nel solo drop-in, ripristino originale in recovery. Test software ARM64 sul
MFD 23/23 PASS, Windows 22 PASS/1 SKIP. Verificati realmente manifesti/binari
candidato e precedente, unità, KMS e whitelist environment; servizio PID 1328
invariato. --check-only bloccato come atteso: qualified power approval missing.
Nessuna installazione root, sigillatura, attivazione o rollback reale MFD.
Dettagli e gate in `docs/deployment/MFD_A0BE9A5_PREPARATION.md`.

Nuova osservazione passiva power, sette campioni in 30 s per nodo:
MFD 0x0, EXT5V PMIC 5.02902–5.18044 V, nessun evento undervoltage del boot;
Blackbox 0x50000, EXT5V 4.95666–5.04242 V, due undervoltage nei primi 14 s
del boot corrente seguiti da normalizzazione. Quindi il problema Blackbox
non è soltanto lo storico del vecchio avvio; nessun nuovo evento nei 30 s.
Timestamp kernel prima del sync NTP fuorviante, eventi confermati con uptime
monotono. I nuovi boot sono stati osservati, non provocati dall'agente.

Evidenze `evidence/power-20261019/`. Nessuna modifica a tensioni/soglie,
override USB, firmware, rete o cablaggio; nessun carico sintetico aggiunto.
0x0 MFD non è approvazione fisica: foto/modello PSU e cavo, prova rappresentativa
ed approvazione documentata per boot rimangono necessari. PWR-001 VERIFY-HW
invariato. La capacità 5 A dell'alimentatore non è attestata dai PDO osservati.

## 64. A5/WX-001: integrità cache METAR/TAF senza attivazione — 2026-10-19

Richiesta: avanzare autonomamente, lasciando bloccate tutte le attivazioni fino
alla verifica alimentatori. Branch software `work/weather-cache-integrity-v1`
derivato dal branch PR #50, non una nuova release da attivare.

Riprodotto su Linux: due test generano cinque failure inclusi subtest. La cache
accettava metar booleano/numero/oggetto/array come report CURRENT e sync poteva
sovrascrivere l'ultima generazione buona con quel payload. Implementati
contratto testuale fail-closed, identità ICAO se presente, lettura JSON limitata,
rifiuto duplicati/non finiti/UTF-8 invalido e validazione prima della scrittura.
Serializzazione limitata prima del file temporaneo; test errori fsync/rename
preservano i byte precedenti e puliscono solo il temporaneo del writer.
Confini freshness frazionari corretti; età dichiarata FETCH_TIME_ONLY e
provider_report_time_validated=false, nessuna autorità safety.

Verificato SOLO SOFTWARE: 28/28 test Linux serverha e 28/28 Linux ARM64 MFD,
in directory di test isolate, con HTTP loopback sintetico e riapertura in un
interprete separato. Windows embedded Python bloccato all'import DLL _ssl:
nessun PASS dichiarato e nessuna protezione OS aggirata. Evidenza con hash e
log in `evidence/weather-cache-20261019/`; runbook WEATHER_CACHE_INTEGRITY.md.
Non è una prova di power-loss, write concurrency, payload/TLS/provider reali,
età osservativa METAR/validità TAF o resa del consumer sul pannello.

WX-001 resta PARTIAL. Nessun servizio/cache di produzione, release, rete,
firmware, timer o soglia power modificati; nessuna attivazione o reboot.
Rimangono aperti qualifica provider/CA/licenza, adapter del payload reale e
prove end-to-end; power e gli altri gate hardware restano bloccanti invariati.
