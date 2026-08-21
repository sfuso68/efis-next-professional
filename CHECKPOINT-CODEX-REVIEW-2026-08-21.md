# EFIS NEXT — Checkpoint di revisione dopo il lavoro Claude Code

Data: 21 agosto 2026  
Revisore: Codex  
Stato: verifica in sola lettura del codice, seguita da build e test locali. Nessun merge, cambio remote o modifica al software avionico.

## 1. Esito sintetico

Il lavoro di Claude Code è sostanziale, coerente con l'handoff e tecnicamente ben strutturato. Il progetto non è più soltanto un prototipo grafico: dispone ora di un core con ingestion MAVLink read-only, arbitrato per grandezza, health/stale/lost, controlli di stasi e coerenza, registrazione black box, replay, banco sensori, SocketCAN, adapter ESP32 e DroneCAN, oltre a una nuova famiglia di strumenti analogici vettoriali.

La build Windows completa riesce e tutti i test correnti passano. Il progetto non è però pronto per proseguire senza prima consolidare il versionamento: il ramo attivo e `main` sono divergenti, e il remote configurato punta al repository storico `sfuso68/efis-evolved.git`, non a un repository autonomo chiamato `efis-next`.

Stato operativo assegnato: **SVILUPPO COERENTE, GATE P0 DI CONSOLIDAMENTO APERTO**.

## 2. Stato Git verificato

- Working tree: pulito.
- Ramo attivo: `work/claude-strumenti-p1`.
- HEAD: `25b8e9a` — pagina analogica Qt alimentata dal router delle sorgenti.
- Ramo `main` locale: `34e1bf6`.
- Remote `github`: `https://github.com/sfuso68/efis-evolved.git`.
- Remote tracking `github/main`: `9f2e251`, diverso dal `main` locale.
- Il ramo attivo è pubblicato come `github/work/claude-strumenti-p1`.
- `main...HEAD`: 15 commit presenti soltanto su `main`, 3 commit presenti soltanto sul ramo attivo.
- Merge-base: `f5b252c`.
- La simulazione di merge non ha mostrato marcatori di conflitto testuale, ma questo non sostituisce build, test e verifica visuale dopo l'integrazione.

### Gate organizzativo

La decisione di progetto era mantenere i due EFIS legacy e costruire EFIS NEXT come progetto nuovo. Pubblicare i rami di EFIS NEXT dentro `sfuso68/efis-evolved.git` può essere intenzionale come monorepo, ma contraddice il principio di separazione se non è stato deciso esplicitamente.

Prima di altri merge occorre scegliere e documentare una delle due opzioni:

1. creare un repository GitHub dedicato `sfuso68/efis-next` e trasferire storia/rami; oppure
2. dichiarare formalmente `efis-evolved` come repository contenitore, mantenendo directory, branch policy e protezioni che impediscano regressioni dei legacy.

Non cambiare il remote e non riscrivere la storia senza approvazione del proprietario.

## 3. Verifica build e test eseguita da Codex

Ambiente: Visual Studio 2026 Insiders Developer Command Prompt, MSVC x64, CMake/Ninja, Qt completo.

Risultato:

- configurazione CMake riuscita;
- build completa riuscita, inclusi `efis-pfd-qt`, `efis-ems-qt` ed `efis-instruments-qt`;
- 10/10 test CTest superati;
- `efis_instrument_tests`: superato;
- working tree rimasto pulito.

Test superati:

1. `efis_core_tests`
2. `efis_bench_tests`
3. `efis_dronecan_tests`
4. `efis_instrument_tests`
5. `efis_safety_tests`
6. `efis_ingest_tests`
7. `efis_esp32_tests`
8. `efis_can_source_tests`
9. `efis_arbitration_tests`
10. `efis_engine_tests`

Osservazioni non bloccanti:

- Ninja ha segnalato `premature end of file; recovering` sulla cache di build esistente; la build ha recuperato ed è terminata correttamente. Se ricompare, rigenerare una nuova directory di build controllata, tenendo conto del vincolo Smart App Control documentato nel checkpoint Claude.
- MSVC segnala un warning C4996 per `std::getenv` in `simulation_provider.cpp`. Non è un errore funzionale, ma va eliminato o confinato con un wrapper multipiattaforma, senza sopprimere globalmente le diagnostiche.

## 4. Lavoro Claude Code confermato

### 4.1 Versionamento e riproducibilità

- repository Git introdotto;
- `.gitignore` ampliato per build, dati rigenerabili e segreti;
- `.gitattributes` impone LF ai file Linux e marca i binari;
- checkpoint operativo molto dettagliato creato;
- copie Windows/RPi5 confrontate e dichiarate allineate nel checkpoint Claude;
- rilevamento Qt reso esplicito con `EFIS_REQUIRE_QT_RENDERER`.

### 4.2 Backbone P1

Confermato dal codice e dai test:

- parser/adapter MAVLink read-only;
- `SnapshotArbitrator` per grandezza;
- normalizzazione nel `FlightModel`;
- `NodeHealthMonitor`;
- stasis monitor;
- cross-check;
- registrazione segmentata e replay;
- eventi di nodo e cambi sorgente;
- banco sensori alimentato dal percorso reale;
- modello campione ampliato con signal/source/node/channel/raw/unit/timestamp/sequence/validity/quality/origin/calibration.

La separazione adapter → arbitrato → modello → recorder/UI è corretta e mantiene il core indipendente da Qt.

### 4.3 P4 CAN/EMS

Confermato:

- `Esp32CanAdapter` implementato e coperto da test;
- SocketCAN implementato;
- monitor CAN dedicato;
- decoder DroneCAN e riassemblaggio DSDL;
- protocollo ESP32 derivato dal firmware esistente e documentato;
- precedente stack `ENGINE_CAN_V1` ritirato dalla build;
- motore introdotto nell'arbitrato;
- servizio EMS RPi4 su eglfs e cattura su richiesta.

Limite confermato dal codice: `Esp32CanAdapter` non è referenziato da `src/ingest/`. `MavlinkPipeline::recompute()` costruisce ancora `std::array<SnapshotContribution, 1>`. La seconda catena ESP32 è quindi leggibile dal monitor, ma non partecipa ancora all'arbitrato operativo contro Pixhawk.

### 4.4 Strumenti analogici

I tre commit più recenti del ramo attivo aggiungono:

- altimetria e regolazione barometrica nel core;
- dinamica di virata e accelerazione laterale;
- accesso ai segnali per identificativo;
- geometrie di quadranti indipendenti da Qt;
- renderer SVG;
- orizzonte, six-pack e strumenti EMS;
- CHT/EGT a quattro lancette;
- catalogo pannelli, validazione e persistenza atomica;
- renderer Qt scene graph;
- eseguibile `efis-instruments-qt`;
- catture RPi5 normali e con guasti simulati;
- oltre mille righe di test specifici.

La scelta architetturale è corretta: gli strumenti sono consumer dello stesso dato arbitrato del PFD e non diventano nuove sorgenti.

Limiti confermati:

- non esiste ancora `config/panels/` versionato;
- non esiste ancora la voce `ANALOG` nel selettore del PFD principale;
- la pagina analogica è un'applicazione autonoma;
- fase CDI/OBS e display Round non completata;
- capability centrali `analog_basic` / `analog_full` non definite;
- preferenze unità non ancora integrate nel profilo aeromobile;
- prova termica prolungata non eseguita.

## 5. Stato per area

| Area | Stato | Nota |
|---|---|---|
| Core C++20 | avanzato | build e test passano |
| MAVLink read-only | fatto | nessun percorso TX rilevato nel backbone descritto |
| Health/stale/lost | fatto | coperto da test |
| Arbitrato per grandezza | fatto nel core | seconda sorgente reale non ancora collegata |
| Black box/replay | fatto a livello software | durability e power-loss test ancora necessari |
| ESP32 CAN | adapter fatto | nodo fisico non ancora validato end-to-end |
| DroneCAN | decoder fatto | continuare monitor-only per airspeed |
| EMS RPi4 | servizio predisposto | eglfs esclusivo; verificare convivenza hardware |
| Strumenti analogici | fasi 0–2 e parte 3 | non integrati nel selettore principale |
| PFD principale | funzionante | problemi grafici storici ancora da ricontrollare |
| Terreno 3D | prototipo avanzato | qualità/pianura/thermal gate aperti |
| Meteo | simulazione/prototipo | freshness e provider reali da chiudere |
| FPV | specifica/prototipo | pipeline 4 camere non completata |
| Pixhawk/GPS/Matek | stato precedente conservato | nessun parametro toccato da Claude secondo checkpoint |

## 6. Rischi prioritari

### R1 — Repository/remote ambiguo

È il rischio immediato più alto per la continuità del lavoro. Prima di nuove integrazioni va definita la destinazione Git definitiva e una branch policy.

### R2 — Divergenza `main`/ramo attivo

Entrambe le linee contengono lavoro sugli strumenti, ma con storie differenti. Un merge meccanico potrebbe duplicare concetti o mascherare una regressione anche senza conflitti testuali. Serve integrazione controllata in un ramo dedicato, quindi build/test Windows, build/test RPi5 e catture visuali.

### R3 — “Seconda sorgente” non ancora reale

Il core supporta failover e cross-check, ma finché ESP32 non entra nella pipeline le protezioni non hanno due catene indipendenti reali da confrontare.

### R4 — Stato hardware non ricontrollato

Il checkpoint Claude dichiara che nessun parametro Pixhawk è stato letto o scritto. È prudente, ma significa che lo stato corrente GPS, Matek, pre-arm e power module non è stato riconfermato in questa fase.

### R5 — Prova UI e performance incompleta

Le catture RPi5 degli strumenti esistono, ma la Definition of Done richiede 240/320/480/1024 px, guasti, frametime, memoria e prova termica. Il Setup Center aveva circa 9,4 fps con pannello banco aperto: va profilato.

### R6 — Certificazione e fonti dati

Limiti aeromobile/motore, V-speeds, ostacoli, TAWS e dati meteo restano development/non-cert fino a verifica documentale e procedurale.

## 7. Punto fissato per la ripartenza

### Gate P0 — prima di scrivere altro codice

1. Decidere repository definitivo: dedicato `efis-next` oppure monorepo dichiarato.
2. Fare un clone/backup verificato di tutti i branch e tag.
3. Creare un ramo di integrazione da `main`, senza modificare direttamente `main`.
4. Integrare i tre commit del ramo `work/claude-strumenti-p1` con merge controllato.
5. Verificare che non vengano persi i 15 commit presenti soltanto su `main`.
6. Eseguire build/test Windows e RPi5.
7. Catturare PFD, EMS e pagina analogica sul display reale.
8. Solo dopo approvazione, aggiornare `main`.

### Primo sviluppo successivo — P4 multi-source

1. Generalizzare la pipeline da una a più `SnapshotContribution`.
2. Aggiungere una sorgente CAN/ESP32 senza introdurre dipendenza SocketCAN nel core.
3. Mantenere MAVLink read-only.
4. Testare arbitrato misto per singola grandezza.
5. Testare perdita ESP32, perdita Pixhawk, dato stale, valore fermo e divergenza.
6. Registrare ogni cambio sorgente nella black box.
7. Collegare il nodo ESP32 fisico e acquisire una registrazione reale.
8. Non usare dati ESP32 per il controllo di volo: restano monitor/cross-check EFIS.

### Poi — completamento strumenti analogici

1. Versionare `config/panels/` con schema e default.
2. Inserire `ANALOG` nel selettore principale con capability centrali.
3. Implementare pagina Round 240–480 px.
4. Aggiungere CDI/OBS e unità configurabili.
5. Verificare ogni stato valido/simulato/stale/assente/fuori scala.
6. Misurare frametime, memoria e temperatura.

## 8. Vincoli da conservare

- I due EFIS legacy non si modificano.
- `ARSPD_USE` resta 0 finché la procedura di validazione non è conclusa.
- Non disabilitare pre-arm check.
- Nessun comando attuatori/servi durante sviluppo software.
- Nessun dato simulato può apparire reale.
- Nessun valore stale o assente deve essere rappresentato come misura valida.
- Nessun token/password nel repository o nei log.
- Configurazioni critiche con backup, audit, read-back e rollback.
- Catture grafiche non equivalgono a prova del codice corrente senza commit e procedura associati.

## 9. Decisione raccomandata

La scelta raccomandata è creare un repository privato dedicato `sfuso68/efis-next`, trasferire l'intera storia corrente preservando tutti i branch, proteggere `main` e usare pull request per l'integrazione. Solo dopo questo consolidamento conviene completare la pipeline multi-source.

Fino a quella decisione, il punto sicuro da conservare è:

- ramo attivo `work/claude-strumenti-p1`;
- commit `25b8e9a`;
- build completa riuscita;
- 10/10 test superati;
- working tree pulito;
- nessuna modifica hardware o a `main` effettuata durante questa revisione.

## 10. Consolidamento EFIS NEXT Professional

Dopo la decisione del proprietario di mantenere e completare EFIS NEXT
Professional è stato creato il ramo locale:

`work/codex-professional-integration`

Operazioni concluse:

- checkpoint di revisione versionato nel commit `5816b7d`;
- ramo `work/claude-strumenti-p1` integrato senza modificare `main`;
- conflitti risolti conservando l'implementazione analogica più recente;
- preservato l'ordine degli identificativi `Unit` già adottato da `main`, per
  non cambiare il significato dei valori serializzati nelle registrazioni;
- eliminata una dichiarazione CMake duplicata di `efis_instrument_tests`
  emersa soltanto durante la build integrata;
- build completa Windows riuscita;
- 10/10 test CTest superati sulla base integrata.

Il ramo integrato è ora la base locale raccomandata per il prossimo lavoro. Non
è stato eseguito alcun push e `main` resta invariato. Il prossimo gate rimane la
scelta del repository remoto definitivo, seguita dall'integrazione multi-source
MAVLink + ESP32.
