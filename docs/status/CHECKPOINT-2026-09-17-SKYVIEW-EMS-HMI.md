# Checkpoint — SkyView/EMS HMI Contract V1 — 2026-09-17

## Implementato offline

- Congelati dieci gate grafici e operativi per SkyView RPi5 ed EMS RPi4.
- Aggiunto manifesto macchina per risoluzioni, token, stati dati, freshness,
  V-speed, HSI, strumenti motore, flap, carrello ed elettrico.
- Aggiunti validatore fail-closed e sei test negativi/positivi.
- Integrato il controllo nel workflow Project Control.
- Conservato il concept approvato in `assets/concepts/` con hash SHA-256 nel
  documento di design; resta esplicitamente non operativo.

## Verificato solo software

- Il JSON è sintatticamente valido.
- Il validatore rifiuta swap RPi4/RPi5, perdita dello stato SIM, touch target
  sottodimensionati, VNE assente e qualunque autorità di comando nell'HMI.
- La CI Linux resta l'evidenza indipendente richiesta per Python e test.

## Hardware reale

- Nessuna nuova verifica o distribuzione hardware in questo incremento.

## Aperto

- Implementare componenti QML/Scene Graph e golden screenshot.
- Eseguire build ARM64, replay DDP/EDP e acceptance visiva/touch sui pannelli.
- Validare frame-rate, termica, sunlight/night mode e fault per 60 minuti.

## Incremento componenti — cinque fondazioni software

- `HmiTokens`: palette, misure minime e mapping stato-colore comuni.
- `HmiDataStateBadge`: LIVE/SIM/STALE/LOST con età esplicita.
- `HmiArcGaugeV4`: gauge V3 riusabile con health e limiti non verificati.
- `SkyViewCompassArc`: arco realmente circolare ±35°, CDI e dati waypoint.
- `HmiComponentShowcase`: superficie deterministica 1024×600 per golden futuri.

I cinque gauge principali dell'EMS usano ora V4 e propagano
`controller.dataSourceState`. I limiti sono intenzionalmente marcati
`UNVERIFIED` finché EMS-004 non viene chiuso con manuale applicabile. Nessuna
verifica grafica o hardware viene dichiarata da questo incremento.

`qmllint` Qt 6.11.1 sui cinque nuovi componenti: PASS. Configurazione e build
MSVC 19.51/Qt 6.11.1 dei target `efis-pfd-qt` ed `efis-ems-qt`: 224/224 step,
link finale PASS. Questa è evidenza software Windows; non prova resa, frame-rate
o interazione sui target ARM64.

## Incremento EMS — cinque passi successivi

- `HmiCylinderMatrixV2`: quattro canali CHT/EGT, hottest channel, spread e
  perdita esplicita del valore in stato LOST/FAIL.
- `HmiAirframePanel`: tre consensi carrello e flap verticale con posizione
  comandata/misurata e detent configurabili.
- `HmiFuelStatus`: quantità sinistra/destra, totale, riserva e degrado dati
  senza usare il solo colore.
- `HmiElectricalSynoptic`: alternatore, main bus ed essential bus come
  rappresentazione read-only con health esplicita.
- Integrazione adattiva dei quattro componenti nella pagina EMS reale e nella
  showcase deterministica; il validatore impedisce la loro rimozione silenziosa.

I limiti CHT/EGT restano `UNVERIFIED`, quindi questo incremento non chiude
EMS-004 e non costituisce approvazione al volo. Verifica hardware ancora
assente.

Verifica software dell'incremento: `qmllint` Qt 6.11.1 PASS; validatore HMI
PASS; 7/7 test dedicati PASS; build incrementale MSVC/Qt dei target PFD ed EMS
75/75 step con entrambi i link finali PASS.

## Incremento golden multi-risoluzione — cinque passi

- Aggiunto controller QML congelato con dati Rotax 912 UL simulati, stato SIM,
  Blackbox online e orologio fisso; non riceve dati avionici e non ha autorità.
- Aggiunta modalità renderer `--hmi-golden`, separata dall'avvio operativo, con
  normalizzazione DPI e cattura alle dimensioni richieste.
- Aggiunto tool di cattura ripetibile per 800×480, 1024×600 e 1280×800.
- Aggiunto validatore fail-closed di schema, risoluzioni, dimensione minima e
  SHA-256, con test positivo e prova di manomissione.
- Versionate tre golden Windows native e collegata la validazione alla CI
  Project Control.

### Evidenza software

- QML AOT e build PFD: 117 step, link PASS; rebuild dopo normalizzazione DPI:
  4/4 step PASS.
- `py_compile` dei tool PASS; suite HMI congiunta 10/10 PASS; validatore
  contratto PASS; validatore golden PASS.
- Una seconda cattura indipendente differisce soltanto per rumore raster
  subpixel (delta massimo 7/255 e frazione modificata sotto 0,00006 a 800×480):
  comparatore visuale con tolleranze chiuse PASS. L'hash file continua a
  proteggere gli artefatti versionati, mentre il confronto visuale usa i pixel.
- Ispezione visiva software: nessun taglio strutturale nelle tre immagini,
  cerchi non ellittici e moduli fuel/electrical/airframe/cilindri presenti.

### Limite rilevato

- A 800×480 diverse etichette secondarie rimangono troppo piccole per dichiarare
  acceptance ergonomica. Le immagini sono baseline di regressione software,
  non prova di leggibilità in cabina, sunlight, touch o frame-rate.
- Nessuna verifica hardware, ARM64 o pannello è stata eseguita.

## Incremento compact 800×480 — cinque passi

- Modalità compact automatica a `<=820×500`, senza cambiare il canvas logico.
- Header contestuale abbreviato e navigazione laterale ridotta a 72 px.
- Quattro azioni inferiori con altezza minima fisica di 44 px.
- Minimi tipografici per gauge e moduli EMS secondari, con propagazione
  esplicita dello stato compact.
- Contratto fail-closed e due test negativi per testo e touch target.

Evidenza solo software: contratto PASS, 12/12 test HMI PASS, `qmllint` senza
errori e build PFD MSVC/Qt 6.11.1 con link PASS. Golden rigenerate alle tre
risoluzioni, validazione SHA/pixel PASS e confronto con seconda cattura PASS.
La cattura 800×480 è stata ispezionata; non sostituisce acceptance sul display RPi4.
Build ARM64, touch, sunlight/night, 40 FPS, replay EDP, termica e fault restano
aperti. Nessuna funzione è promossa a flight approved.

## Incremento stati degradati — cinque passi

- Fixture golden parametrica per stato ed età.
- Header EMS fedele a `LIVE/STALE/LOST/FAIL/SIM`.
- Età esplicita per valori congelati `STALE/LOST`.
- Scale neutre, senza normalizzazione verde, in `LOST/FAIL`.
- Test fail-closed e comando per golden degradate; la cattura resta aperta dopo
  timeout del renderer nella sessione Windows corrente. Nessuna acceptance
hardware implicita.

## Incremento isolamento dati non validi — cinque passi

- `LOST/FAIL` hanno precedenza sul CAS e generano `ENGINE DATA LOST/FAIL`.
- `STALE/DEGRADED` producono una caution esplicita con richiesta di verifica.
- Trend e lean non acquisiscono campioni non utilizzabili; il lean attivo viene
  arrestato al decadimento della sorgente.
- Le checklist automatiche non possono risultare verdi con dati non validi.
- Età e stato sono propagati anche a cilindri, airframe, fuel ed electrical.

Verifiche esclusivamente software; nessun fault fisico o pannello reale incluso.

## Incremento EDP operativo stato/età — cinque passi

- Aggiunta età monotona opzionale al consumer EDP.
- Propagata l'età attraverso la sorgente UDP senza cambiare il wire protocol.
- Esposti `dataAgeMs` e stati HMI canonici dal controller EMS operativo.
- Definiti reset fail-closed: attesa senza età, link perso con ultima età nota,
  simulazione autonoma `SIM/-1`.
- Aggiunti test protocollo/loopback e contratto controller-QML.

Evidenza software: `efis_edp_v1_tests` PASS, contratto HMI 14/14 PASS e build
PFD/EMS MSVC/Qt con link finale PASS. Non sono stati eseguiti replay Blackbox,
deployment ARM64, fault fisici o acceptance sul pannello RPi4; pertanto lo
stato di requisito non viene promosso a DONE.

## Incremento transizioni/recovery EDP — cinque passi

- Snapshot atomico `LinkStatus` per stato ed età.
- Nomi diagnostici canonici dei quattro stati di collegamento.
- Controller EMS convertito a una sola osservazione per ciclo.
- Test dei confini temporali e del mancato refresh su CRC errato.
- Recovery verificata sia nel consumer sia nel trasporto UDP loopback.

Evidenza esclusivamente software: test EDP PASS, contratto HMI 14/14 PASS e
build/link PFD+EMS PASS. Nessuna prova Blackbox/RPi4, fault Ethernet, reboot o
endurance è inclusa; EMS-002 e BBX-003 non cambiano stato nella master matrix.

## Incremento diagnostica DATA LINK EMS — cinque passi

- Esposta mappa read-only completa delle metriche EDP.
- Aggiunto evento journal strutturato sulle sole transizioni.
- Creata pagina Setup `DATA LINK` con freshness e contatori.
- Riallineati gli indici Setup e aggiunta anteprima dedicata.
- Aggiunto test fail-closed del contratto diagnostico.

Verifica software: HMI 15/15 PASS, `qmllint` senza errori e build/link PFD+EMS
PASS. La cattura offscreen Windows non ha terminato ed è stata interrotta, quindi
non esiste nuova evidenza grafica. Nessun deployment o test RPi4/Blackbox è
incluso e nessun requisito viene promosso a DONE.

## Incremento tracciabilità publisher EDP — dieci passi

1. Definiti nomi canonici stabili per tutti gli esiti di decodifica EDP.
2. Introdotto uno snapshot diagnostico atomico del consumer.
3. Esposti source node, publisher instance e ultima sequenza accettata.
4. Conservato l'ultimo motivo di rifiuto senza cancellarlo al recovery.
5. Aggiunta l'età monotona dell'ultimo rifiuto, con protezione da regressione
   dell'orologio.
6. Propagato lo snapshot diagnostico attraverso la sorgente UDP read-only.
7. Calcolati percentuale valida e stato cumulativo della sessione nel
   controller EMS senza modificare l'autorità della sorgente.
8. Estesa la pagina DATA LINK con identità, sequenza e ultimo rifiuto; la
   pagina non contiene handler di comando.
9. Esteso il journal delle transizioni con identità del publisher e ultimo
   rifiuto.
10. Aggiunti test fail-closed per attesa, CRC, identità errata, duplicato,
    restart e recovery.

### Evidenza software

- `efis_edp_v1_tests`: PASS, incluse identità autorevole e persistenza
  dell'evidenza di rifiuto attraverso il recovery.
- Contratto HMI: 15/15 PASS; la pagina DATA LINK è confermata read-only.
- `qmllint` Qt 6.11.1: nessun errore; restano warning preesistenti sugli
  accessi QML non qualificati.
- Build MSVC/Qt 6.11.1 dei target `efis-pfd-qt` ed `efis-ems-qt`: link PASS.

### Hardware reale e aperto

- Nessuna distribuzione o verifica su RPi4/Blackbox in questo incremento.
- Source node, publisher instance, sequenza, contatori ed età del rifiuto
  devono essere confrontati sul pannello con journal e broker reali durante
  `LIVE → STALE → LOST → LIVE`.
- Nessun requisito viene promosso a `DONE`; EMS-002 e BBX-003 restano
  `PARTIAL` fino alle evidenze hardware già definite nella matrice.

## Incremento autorità e policy EDP — cinque passi

1. Lo snapshot diagnostico espone la policy effettiva del consumer, inclusa
   l'autorità `expected_source_node` configurata.
2. Il controller pubblica sorgente attesa, sorgente attiva e risultato
   `UNRESTRICTED/WAITING/MATCH/MISMATCH` senza modificare l'autorità avionica.
3. Le soglie operative `stale_after` e `lost_after` sono esposte in
   millisecondi dalla stessa policy usata per classificare il collegamento.
4. L'ultimo rifiuto resta evidenza cumulativa, mentre `recentRejection`
   identifica separatamente soltanto un rifiuto avvenuto negli ultimi 5 s.
5. La pagina Setup `DATA LINK` mostra autorità, soglie, publisher, sequenza e
   marcatore `RECENT`; continua a non contenere azioni di comando.

### Evidenza software

- `efis_edp_v1_tests`: PASS, inclusi policy snapshot, limite inclusivo della
  finestra recente ed estinzione del marcatore dopo 5 s.
- Contratto HMI: 15/15 PASS e pagina DATA LINK confermata read-only.
- `qmllint` Qt 6.11.1: nessun errore; restano warning preesistenti sugli
  accessi QML non qualificati.
- Build MSVC/Qt 6.11.1 dei target `efis-pfd-qt` ed `efis-ems-qt`: link PASS.

### Hardware reale e aperto

- Nessuna distribuzione o verifica su RPi4, RPi5 o Blackbox in questo
  incremento.
- Autorità, soglie e finestra `RECENT` devono essere confrontate con broker e
  journal reali durante traffico valido, source node errato, perdita e recovery.
- Nessun requisito viene promosso a `DONE`; EMS-002 e BBX-003 restano
  `PARTIAL`.

## Incremento replay EDP deterministico — venti passi

1. Implementato encoder completo dei 18 campioni EDP V1.
2. Conservati magic, schema, envelope, dimensioni e ordinamento big-endian.
3. Calcolato CRC32C del payload per ogni frame.
4. Aggiunto scenario nominale `clean` a 20 Hz.
5. Aggiunto fault isolato di gap di sequenza.
6. Aggiunto fault isolato di duplicato senza gap collaterale.
7. Aggiunto fault isolato di inversione senza gap collaterale.
8. Aggiunto restart con nuova publisher instance e sequenza riavviata.
9. Aggiunto CRC errato con payload altrimenti conforme.
10. Aggiunta sorgente non autorevole per verificare il gate node 47.
11. Aggiunta provenienza mista a livello di singolo campione.
12. Aggiunto feed interamente reale, sempre qualificato soltanto per banco.
13. Aggiunta pausa deterministica oltre la soglia `LOST` e recovery.
14. Imposti limiti a frame, frequenza, pausa, identità e porta.
15. Limitata la trasmissione non-loopback a IPv4 private e opzione esplicita.
16. Resi non trasmissivi per default CLI e piano `DRY_RUN`.
17. Aggiunto manifesto atomico senza payload, con digest di wire e scheduling.
18. Dichiarati effetto atteso, trasmissione, frame inviati e target.
19. Imposti invarianti `raw_payload_persisted=false` e
    `flight_approval=false`.
20. Collegati nove test a CTest e documentata la procedura di banco sicura.

### Evidenza software

- Suite replay: 9/9 test PASS; suite EDP complessiva: 26/26 PASS.
- `py_compile`: PASS.
- CLI `clean` da quattro frame: `DRY_RUN`, zero frame trasmessi e manifesto
  coerente.
- La prima regressione ha rilevato e fatto correggere gap collaterali negli
  scenari duplicato/inversione; i fault risultano ora indipendenti.

### Hardware reale e aperto

- Nessun datagramma è stato inviato sulla rete avionica e nessun nodo è stato
  modificato.
- Replay su EMS RPi4, osservazione DATA LINK, journal, cattura passiva e
  assessment restano da eseguire in una finestra di banco isolata.
- EMS-002 e BBX-003 restano `PARTIAL`; il replay non è approvazione al volo.

## Incremento qualificazione indipendente evidenza EDP — venti passi

1. I limiti di accettazione applicati dalla cattura sono inclusi nel report.
2. Il documento sorgente è limitato a 1 MiB prima del parsing.
3. Radice JSON e schema EDP V1 sono verificati in modo stretto.
4. È ammessa esclusivamente la modalità passiva `AF_PACKET`.
5. La persistenza del payload grezzo deve risultare disabilitata.
6. `flight_approval` deve essere presente e sempre `false`.
7. Endpoint IPv4 e nome interfaccia sono validati sintatticamente.
8. Il timestamp deve essere ISO 8601, UTC e dotato di fuso esplicito.
9. Contatori e misure devono essere finiti e non negativi.
10. Totali pacchetti, byte ed errori di decodifica sono riconciliati.
11. Le identità degli stream devono essere univoche.
12. Ogni stream accettato deve rispettare l'autorità Blackbox `source=47`.
13. Contatori e intervallo di sequenza degli stream sono riconciliati.
14. I restart dichiarati devono coincidere con gli stream non correnti.
15. I campioni temporali devono essere coerenti con i datagrammi accettati.
16. La percentuale di gap viene ricalcolata dai contatori elementari.
17. Chiavi dei limiti e dei check incorporati devono coincidere esattamente
    con il contratto noto, senza estensioni silenziose.
18. Tutti i check e lo stato vengono ricalcolati indipendentemente; un report
    che si auto-dichiara `PASS` non è considerato evidenza sufficiente.
19. L'assessment separa integrità, autorità, continuità e uniformità e
    classifica il feed `NO_DATA/SIM_ONLY/REAL_ONLY/MIXED`.
20. Il report di assessment include SHA-256 del sorgente, viene scritto
    atomicamente ed è registrato nella suite CTest e nella procedura EMS.

### Evidenza software

- Suite combinata cattura/assessment: 17/17 test PASS.
- `py_compile` di entrambi i tool e delle rispettive suite: PASS.
- Verificati alterazione dei check/stato, escalation privacy/approvazione,
  incoerenze contatori/stream, schema o policy ignoti, digest e output atomico.

### Hardware reale e aperto

- Nessuna cattura reale è stata acquisita e nessun nodo è stato modificato.
- Il classificatore produce esclusivamente qualifiche di banco
  `BENCH_SIM_ONLY`, `BENCH_REAL_ONLY`, `BENCH_MIXED` o `NOT_QUALIFIED`; non può
  concedere approvazione al volo.
- Restano necessari report passivo da 180 s, correlazione con DATA LINK e
  journal, fault fisico, recovery ed endurance. EMS-002 e BBX-003 restano
  `PARTIAL`.

## Incremento osservatore passivo EDP — dieci passi

1. Implementato parser EDP V1 indipendente per l'evidenza di banco.
2. Verificati magic, schema, identità, dimensione e CRC32C.
3. Verificati i 18 segnali, unicità, unità, source, validity e quality.
4. Applicato il gate dell'autorità Blackbox `source_node=47`.
5. Contati gap, duplicati, inversioni e restart per publisher instance.
6. Misurati frequenza, massimo e percentile 95 degli intervalli.
7. Usata cattura Linux `AF_PACKET` passiva, senza bind UDP o return path.
8. Prodotto report JSON atomico, aggregato e privo di payload grezzi.
9. Aggiunti gate fail-closed e otto test deterministici, inclusa alterazione
   con CRC ricalcolato.
10. Collegato il test a CTest e documentato il comando di banco RPi4.

### Evidenza software

- Suite `test_capture_edp_observer`: 8/8 PASS.
- `py_compile` del tool e dei test: PASS.
- Il vettore noto CRC32C `123456789` produce `0xE3069283`.
- Riconfigurazione CMake e build MSVC del target `efis_edp_v1_tests`: PASS.
  L'avvio locale dell'eseguibile tramite CTest resta bloccato dalla restrizione
  applicativa Windows già osservata; la regressione eseguibile è demandata alla
  EFIS Core CI Linux e non viene dichiarata localmente.

### Hardware reale e aperto

- Nessuna cattura è stata eseguita su `eth0` del nodo EMS in questo incremento.
- Il tool è predisposto per `10.77.10.2 → 10.77.10.12:49003`, ma il report reale
  da 180 s e la correlazione con pagina DATA LINK/journal restano da acquisire.
- Nessun requisito viene promosso a `DONE`; EMS-002 e BBX-003 restano
  `PARTIAL`.
