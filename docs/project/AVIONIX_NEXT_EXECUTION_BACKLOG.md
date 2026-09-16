# AVIONIX NEXT — backlog esecutivo per gate

Aggiornamento: 2026-09-16  
Stato: controllato da WS0/G0; nessuna attività hardware autorizzata da questo documento.

Gli stati ammessi qui sono `PROPOSED`, `OPEN`, `BLOCKED`, `IN_PROGRESS` e
`ACCEPTED`. La chiusura funzionale resta regolata dalla Master Matrix.

| ID | Gate | Descrizione | Input | Output | Dipendenze | Rischio | Accettazione | File previsti | Test | Stima | Stato |
|---|---|---|---|---|---|---|---|---|---|---|---|
| AVX-G0-01 | G0 | Importare brief e indice nel repo autorevole | Brief R0 storico, audit | baseline documentale tracciata | nessuna | basso | brief integro e regole di autorità esplicite | `docs/project/*` | hash/confronto, link check | S | ACCEPTED |
| AVX-G0-02 | G0 | Formalizzare GitHub/NAS e divergenze | refs Git/NAS | registro repository | accesso NAS | medio | `origin/main` e `professional/main` coerenti | indice, stato | rev-parse/read-only | S | OPEN |
| AVX-G0-03 | G0 | Decidere il bare NAS duplicato | inventario NAS | decisione conservazione | autorizzazione utente | medio | nessuna copia ambigua | indice | verifica refs | S | BLOCKED |
| AVX-G0-04 | G0 | Recuperare cinque documenti mancanti | file del proprietario | indice con hash | disponibilità file | alto | 5/5 acquisiti e classificati | `docs/reference/` o registro esterno | hash e apertura | M | BLOCKED |
| AVX-G0-05 | G0 | Congelare nomenclatura AVIONIX/EFIS | decisione prodotto | naming policy | utente | medio | nomi e alias approvati | ADR o brief R1 | controllo riferimenti | S | BLOCKED |
| AVX-G1-01 | G1 | Identificare nodo ASSO5 dedicato | RPi4 reale | identità OS/IP/power/storage | hardware | alto | inventario firmato | inventario HW | comandi read-only | S | OPEN |
| AVX-G1-02 | G1 | Inventariare Sequent e DIP | quattro schede | modello/revisione/ID/foto | AVX-G1-01 | alto | 4/4 identificate | inventario HW | I2C/CLI read-only | M | OPEN |
| AVX-G1-03 | G1 | Implementare adapter real/SIM e health | inventario e ICD | servizio acquisizione | AVX-G1-02, G2 schema | medio | open/short/timeout e recovery testati | core/adapter/deploy | unit/component | L | PROPOSED |
| AVX-G1-04 | G1 | Soak nodo sensori | servizio completo | report 60 minuti | AVX-G1-03 | alto | zero errori non gestiti e log locale continuo | evidence | soak/fault rete | M | PROPOSED |
| AVX-G2-01 | G2 | Congelare ICD AVIONIX sopra DDP/EDP | contratti esistenti | schema, freshness, naming | G0 | medio | compatibilità ARM64/x86-64 | docs/protocol, test vectors | schema/replay | M | OPEN |
| AVX-G2-02 | G2 | Verticale sensore→RPi4→Blackbox→RPi5→replay | G1 e ICD | catena completa riproducibile | AVX-G1-03, AVX-G2-01 | alto | stesso dataset e stessi esiti | adapter/broker/replay | integration/HIL | L | PROPOSED |
| AVX-G3-01 | G3 | Correggere LOD/proiezione SV | renderer e dataset | qualità primo piano accettabile | benchmark/dataset | medio | acceptance visuale e frame-rate | renderer/terrain | screenshot/replay/HW | L | OPEN |
| AVX-G3-02 | G3 | Integrare MGL RS-232 passivo | MGL, pinout, protocollo | parser e confronto EMS | documenti mancanti | alto | MGL indipendente, timeout corretti | adapter/test | parser/HW | L | BLOCKED |
| AVX-G3-03 | G3 | Acceptance touch PFD/MFD/EMS | display reali | checklist operatore | touch disponibile | medio | nessuna sovrapposizione e fail-state leggibili | evidence/status | touch/scenari | M | OPEN |
| AVX-G4-01 | G4 | PSU, harness, enclosure e schema filo-per-filo | inventario finale | prototipo installabile | G1/G3 | alto | design review e brownout test | hardware docs | electrical/environmental | XL | PROPOSED |
| AVX-G5-01 | G5 | HIL e fault injection integrati | G3/G4 | matrice prove ripetibile | gate precedenti | molto alto | fault obbligatori con evidenza | tests/evidence | HIL | XL | PROPOSED |
| AVX-G6-01 | G6 | Congelare baseline EFIS Next | G0–G5 | release/config/recovery controllati | tutti i gate | alto | configuration index e rollback | release docs | acceptance completa | L | PROPOSED |
| AVX-G7-01 | G7 | Porting CM/Next Plus | baseline G6 | parità funzionale su hardware industriale | G6 | alto | replay e benchmark equivalenti | nuovo profilo HW | benchmark/replay | XL | PROPOSED |

## Prossima azione autorizzabile

Dopo il merge documentale WS0/G0, la prima attività tecnica resta
AVX-G1-01/02 in sola ricognizione: identificazione del nodo RPi4 ASSO5 e delle
quattro Sequent. Non include cablaggi, scritture EEPROM/DIP, installazioni o
comandi su uscite.
