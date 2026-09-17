# AVIONIX NEXT — PROJECT AUDIT

Aggiornamento: 2026-09-17
Ambito: ricognizione e piano prescritti dalla sezione 24 del Master Project
Brief R0.
Stato: DEVELOPMENT / NON-CERT; nessuna autorizzazione implicita a modifiche
hardware, deployment, merge o funzioni di comando.

## REPOSITORY

- path: `C:/Users/stefa/.codex/.chatgpt-projects/g-p-6a8162f124ac8191abb9a466a8cdcd25/tmp/efis-next-project-control`;
- branch: `work/skyview-ems-hmi-contract-v1`;
- head: `dfb35103d70227131ca1550bc6869c4b4b41e067`;
- working tree: nessuna modifica sorgente; presenti soltanto artefatti non
  tracciati preesistenti sotto `output/`, esclusi dall'audit e non modificati;
- remotes: `origin` = `sfuso68/efis-next-professional`, `nas` = bare ufficiale
  `/volume1/homes/sfuso/git-repos/efis.git`, `project-control-source` =
  `sfuso68/efis-evolved` storico;
- divergence: il branch corrente è 18 commit avanti e zero indietro rispetto
  a `origin/main` (`4416ee2`); non è autorizzato alcun merge;
- gate remoti sull'HEAD: EFIS Project Control #85 PASS, EFIS Core CI #69 PASS.

## SOFTWARE, TOOLCHAIN E TEST

- Core C++20 indipendente da Qt con snapshot, provenance, arbitraggio, health,
  DDP V2, EDP V1, DroneCAN, motore, safety ed Emergency Assist.
- Applicazioni: core, CAN monitor, recorder, simulatore, DDP publisher,
  Blackbox broker, renderer di riferimento, PFD Qt, strumenti Qt ed EMS Qt.
- UI Qt Quick/Quick3D/QuickControls; versione minima Qt 6.8. Build CMake/Ninja,
  GCC ARM64 e MSVC usati nelle evidenze correnti; Python 3 per strumenti e CI.
- Quarantacinque file di test C++/Python nel repository. Il blocco EDP corrente
  copre cattura passiva, assessment indipendente e replay deterministico con
  26/26 test locali e Core CI Linux PASS.
- Ventinove file deployabili in `deploy/systemd`, inclusi PFD, MFD, EMS,
  broker, recorder, firewall/observer MAVLink, imagery ed eventi.

## EXISTING SYSTEMS

- RPi4: EMS `efis`/`.36` presente e separato per ADR-006/013; EDP V1 SIM
  osservato in precedenza. ASSO5-SENSOR-NODE e VIDEO-CORE dedicato non sono
  ancora identificati e non devono essere confusi con l'EMS.
- RPi5: PFD `.45`, MFD `.49` e Blackbox `.47` risultano configurati nelle
  ultime evidenze hardware. Le release operative e i rollback sono registrati
  nello stato, ma il branch corrente non è stato distribuito.
- synthetic vision: renderer EFIS Next e pacchetto terrain/imagery disponibili;
  la texture ravvicinata resta stirata/sfocata e non accettata.
- black box: broker unico, recorder, DDP, EDP SIM e fan-out MAVLink monitor-only
  esistono; CAN motore reale, fault fisici e endurance restano incompleti.
- server/NAS: server `.6` con mapapp/Caddy e dataset; NAS `.22` con bare
  ufficiale e un secondo bare ambiguo. Lifecycle, trust CA e decisione sul
  duplicato restano aperti.
- laptop/tools: portatile `.39` con QGC monitor-only e firewall; Ethernet e
  acceptance simultanea restano aperte. Strumenti di cattura/assessment sono
  presenti nel repository.
- Pixhawk/MAVLink: Pixhawk 6X `10.77.10.3`, ArduPlane e stream read-only
  risultano integrati; comandi autopilota sono fuori dallo scope P2.
- EMS/MGL/Sequent: EMS grafico e simulatore sono avanzati; MGL RS-232,
  quattro schede Sequent, DIP e nodo ASSO5 non sono identificati. I limiti
  Rotax reali restano bloccati da documentazione/seriale applicabili mancanti.

## GAP ANALYSIS PER WORKSTREAM

| WS | Esiste/funziona | Incompleto, duplicato o obsoleto | Dipendenze, rischi e test | Decisione richiesta |
|---|---|---|---|---|
| WS0 | brief R0, indice, matrice, backlog e CI di controllo | cinque fonti mancanti; naming e bare NAS ambigui | G0 non chiudibile senza fonti e decisioni | naming, bare NAS, fonti |
| WS1 | core, provenance, DDP V2, EDP V1, freshness e replay | ICD AVIONIX complessivo non congelato | compatibilità ARM64/x86-64, vectors e replay | approvare freeze ICD |
| WS2 | architettura del nodo definita | ASSO5/Sequent/DIP non inventariati; nessun adapter reale qualificato | accesso fisico, I2C read-only, open/short/timeout | identificare nodo e schede |
| WS3 | PFD/MFD/EMS e contratto HMI con golden/CI | touch e acceptance reale incompleti; branch non distribuito | frame-rate, sunlight/night, stale/lost, rollback | approvare prossimo blocco HMI |
| WS4 | SV, DEM, imagery, navdata e cache | LOD/proiezione vicino non accettati; branch storico solo LEGACY | dataset, screenshot, rotta e benchmark HW | priorità correzione terrain |
| WS5 | Pixhawk, M10 e Matek rilevati; modelli core presenti | calibrazioni e health reali incompleti | banco pitot/statico, cielo aperto, failover | finestra test hardware |
| WS6 | EMS Rotax SIM, EDP, setup e diagnostica DATA LINK | CAN reale, MGL e limiti manuale non qualificati | documenti controllati, seriale, replay e fault | fornire manuali/seriali |
| WS7 | Pixhawk read-only e observer fan-out | nessuna autorità comando; P2 non congelata | interlock e HIL prima di ogni attuazione | mantenere P2 read-only |
| WS8 | broker, recorder, server mappe, NAS e QGC | retention, CA, reboot/endurance e doppio bare aperti | fault storage/rete e rollback | conservazione NAS |
| WS9 | requisiti e allocazione preliminare | schema filo-per-filo, PSU, harness ed enclosure mancanti | brownout, EMI, termica e review design | inventario elettrico finale |
| WS10 | CI, test core/HMI, osservatori e manifesti | HIL integrato e safety case non completi | fault matrix, indipendenza evidenze, acceptance | criterio di ingresso HIL |
| WS11 | profilo CM pianificato | porting non iniziato | dipende da baseline G6 | nessuna prima di G6 |
| WS12 | profilo Next Plus pianificato | hardware industriale e porting non iniziati | dipende da G6/WS11 | nessuna prima di G6 |

## CONFLICTS WITH MASTER BRIEF

Restano applicabili C-001…C-009 del registro in
`AVIONIX_NEXT_DOCUMENT_INDEX.md`. I conflitti operativi non chiusi sono:

- rete preliminare del brief contro ADR-010/015;
- ruoli distinti EMS, ASSO5 e VIDEO-CORE;
- Blackbox RPi5 reale contro descrizione RPi4 preliminare;
- Waveshare 5P insufficiente come rete finale;
- branch `sv3d-fondamenta` senza genealogia condivisa;
- naming AVIONIX NEXT/EFIS NEXT non congelato;
- secondo bare NAS senza decisione di conservazione.

## BLOCKERS

- Cinque documenti elencati nell'indice non sono disponibili.
- Nodo ASSO5, quattro Sequent e DIP non sono identificati.
- Naming prodotto e politica alias non sono approvati.
- Bare NAS duplicato non ha una decisione formale.
- PAN-002 richiede POH/manuali/revisioni applicabili reali.
- PAN-003 richiede identificazione servi, meccanica, interlock e HIL.

## RECOMMENDED GATES

- G0: PARTIAL — chiudere decisioni documentali, naming e repository NAS.
- G1: OPEN/BLOCKED-HW — sola ricognizione ASSO5/Sequent, senza scritture.
- G2: IN_PROGRESS-SW — congelare ICD DDP/EDP e catena di evidenza; verticale
  reale bloccato da G1.
- G3: IN_PROGRESS-SW/VERIFY-HW — contratto HMI avanzato; touch, SV e replay su
  pannelli restano da accettare.
- G4: PROPOSED — non iniziare prima dell'inventario elettrico.
- G5: PROPOSED — non iniziare prima di G3/G4 e matrice HIL approvata.
- G6: PROPOSED — baseline possibile solo dopo chiusura G0–G5.
- G7: DEFERRED — CM/Next Plus solo dopo baseline G6.

## FIRST SAFE CHANGE

- scope: congelamento documentale dell'ICD AVIONIX sopra DDP V2/EDP V1 e del
  formato dell'evidence bundle, senza cambiare wire protocol o runtime;
- files proposti: nuovo `docs/protocols/AVIONIX_DATA_ICD_V1.md`, aggiornamento
  backlog/matrice/stato ed eventuali test vector non eseguibili;
- tests proposti: confronto schema, digest dei vector, link/document check e
  project status `--check`;
- rollback: un singolo commit documentale reversibile;
- autorizzazione: **NON ANCORA CONCESSA**. Nessuna implementazione deve partire
  finché l'utente non approva esplicitamente questo scope o ne sceglie un altro.

## QUESTIONS REQUIRING USER DECISION

1. Il nome prodotto definitivo è `AVIONIX NEXT`, mantenendo `EFIS NEXT` come
   nome tecnico/legacy, oppure resta `EFIS NEXT`?
2. Il secondo bare repository NAS va mantenuto come archivio, rinominato o
   rimosso in una futura attività autorizzata?
3. Dove sono disponibili i cinque documenti mancanti elencati nell'indice?
4. Quale unità fisica è il nodo ASSO5 e quali sono modello/revisione/DIP delle
   quattro schede Sequent?
5. Si approva come prossimo blocco il solo freeze documentale G2 dell'ICD e
   dell'evidence bundle?
