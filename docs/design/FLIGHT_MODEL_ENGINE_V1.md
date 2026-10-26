# Motore di simulazione del volo — analisi di FS 2024 e specifica V1

Stato: implementato, 26 ottobre 2026
Codice: `include/efis/fdm/`, `src/fdm/`, `src/apps/efis_fdm_main.cpp`, `tests/fdm_tests.cpp`
Decisione: ADR-008

## 1. Obiettivo

Un motore di simulazione del volo che gira su Linux, dentro questo progetto,
per sviluppare e mettere alla prova sistemi EFIS: il mondo LSA e quello
professionale. Non un simulatore per volare: un banco. La differenza non è di
ambizione, è di scopo, e decide ogni scelta che segue. Un simulatore serve a far
fare esperienza a una persona; un banco serve a produrre una condizione nota con
cui giudicare uno strumento.

Da questa differenza discende la proprietà che il banco ha e un simulatore
commerciale non ha: in ogni istante esistono **due** versioni di ogni grandezza,
quella vera del modello e quella misurata dai sensori simulati, e la loro
differenza è l'errore dello strumento, misurato invece che supposto.

## 2. Che cosa è stato analizzato di Flight Simulator 2024, e come

Materiale esaminato, tutto pubblico e installato su questa macchina:

| Fonte | Percorso | Dimensione |
|---|---|---|
| SDK ufficiale, versione 1.8.1 (build 2603316) | `D:\MSFS 2024 SDK` | 1,7 GB |
| Interfaccia client SimConnect | `SimConnect SDK/include/SimConnect.h` | 1139 righe |
| API delle variabili per i moduli WASM | `WASM/include/MSFS/MSFS_Vars.h` | 233 righe |
| Strato legacy delle strumentazioni | `WASM/include/MSFS/Legacy/gauges.h` | 5866 righe |
| Enumerazione degli eventi | `WASM/include/MSFS/Types/MSFS_EventsEnum.h` | 2161 righe |
| Comportamenti dei modelli di Asobo | `ModelBehaviorDefs/` (XML) | centinaia di file |
| Dati utente del simulatore installato | `%APPDATA%\Microsoft Flight Simulator 2024` | — |

Metodo, dichiarato perché conta: si sono letti **intestazioni, schemi e file XML
pubblicati nell'SDK**, cioè il materiale che Microsoft distribuisce proprio
perché i terzi scrivano aeromobili e strumenti. Non si è decompilato nessun
binario, non si è aggirata nessuna protezione, non si è copiato nessun dato di
aeromobile: i file di configurazione delle cellule, nell'installazione 2024, non
sono nemmeno su disco — il contenuto è in streaming e `SimObjects/Airplanes`
contiene soltanto `STATE.CFG` per ciascun velivolo. Il risultato dell'analisi è
quindi un'**architettura** e un **contratto di interfaccia**, non un travaso di
parametri. Il modello fisico di questo progetto è scritto dalla meccanica del
volo, non estratto da un prodotto altrui.

## 3. Che cosa emerge

### 3.1 Il simulatore non espone un modello dati: espone un dizionario

È il risultato principale, e si legge direttamente in `MSFS_Vars.h`. Una
grandezza non è un campo di una struttura: è un nome, con un'unità richiesta a
parte e un indice opzionale.

```c
FsAVarId   fsVarsGetAVarId(const char* AVarName);
FsUnitId   fsVarsGetUnitId(const char* unitName);
FsVarError fsVarsAVarGet(FsAVarId AVar, FsUnitId unit, FsVarParamArray param,
                         double* result, FsObjectId target);
```

Sette famiglie di variabili, distinte da un prefisso e da chi le possiede:
`A:` lo stato della simulazione (sola lettura concettuale), `B:` gli eventi di
ingresso, `E:` l'ambiente, `L:` le variabili locali del velivolo, `I:`, `O:`,
`Z:` quelle degli oggetti. L'unità fa parte della **richiesta**, non della
variabile: lo stesso `A:AIRSPEED INDICATED` si legge in nodi o in metri al
secondo, e la conversione è del motore.

Tre conseguenze pratiche, tutte adottate nel banco:

- un consumatore scritto contro i nomi non sa nulla di come il valore sia
  calcolato, e può quindi essere alimentato da sorgenti diverse;
- l'indice (`:1`, `:2`) è il modo in cui si esprime la molteplicità — motori,
  sbarre elettriche, cilindri — senza moltiplicare i nomi;
- dichiarare l'unità a ogni accesso elimina la classe di errori più silenziosa
  dell'avionica, quella in cui un numero giusto viaggia nell'unità sbagliata.
  È la stessa ragione per cui `efis::Sample` porta `origin.unit`.

### 3.2 Lo strato legacy mostra come il motore pensa l'aeromobile

`Legacy/gauges.h` è l'eredità, ancora presente e ancora funzionante, dell'API di
Flight Simulator X. Non serve a scrivere nulla di nuovo, ma dice molto su come
il motore rappresenta la macchina:

- i comandi sono **quantizzati a 14 bit**: `FLAPS_POS_OFF 0`,
  `FLAPS_POS_FULL 16383`, `GEAR_POS_UP 0`, `GEAR_POS_DOWN 16383`,
  `SPOILERS_POS_FULL 16383`. Una posizione di comando è un intero, non una
  frazione;
- i serbatoi sono un'**enumerazione fissa** (`TANK_MAIN_LEFT`, `TANK_AUX_RIGHT`,
  `TANK_TIP_LEFT`, `TANK_CENTER`…), non una lista configurabile;
- il carrello è un **tipo** (`GEAR_TYPE_FIXED`, `RETRACTABLE`, `SKIDS`,
  `FLOATS`, `SKIS`), e i punti di contatto sono enumerati
  (`GSN_CENTER_GEAR`, `GSN_LEFT_GEAR`, `GSN_RIGHT_WING_SCRAPE`…);
- l'avviamento dei motori è una **posizione di selettore**, diversa fra alternativi
  e turbine: `ENG_STARTER_MAGNETO_BOTH`, `ENG_STARTER_MAGNETO_START` da un lato,
  `ENG_STARTER_STARTER_GEN` dall'altro;
- il suolo è un'enumerazione di tipo e di condizione (`SURFACE_TYPE`,
  `SURFACE_CONDITION`);
- e soprattutto: gli **errori degli strumenti sono opzioni**, non fisica.
  `REALISM_SYSTEMS_GYRO_DRIFT`, `REALISM_SYSTEMS_BARO_DRIFT`,
  `REALISM_SYSTEMS_ELEV_TRIM`, `REALISM_SYSTEMS_LIGHT_BURN` sono bit di una
  maschera di realismo che si accende o si spegne.

Quest'ultimo punto è il più importante per noi, e lo riprende il paragrafo 3.4.

### 3.3 SimConnect è il contratto esterno, a sottoscrizione

`SimConnect.h` descrive un'interfaccia a messaggi: il client definisce una
*data definition* elencando nome e unità per ciascun campo, la sottoscrive con
un periodo (`SIMCONNECT_PERIOD`), e riceve strutture binarie
(`SIMCONNECT_RECV_SIMOBJECT_DATA`). Accanto ci sono gli eventi di sistema e di
fotogramma (`RECV_ID_EVENT_FRAME`), i dati delle facility aeroportuali
(`RECV_ID_FACILITY_DATA`), le aree di dati client, gli input event, il bus di
comunicazione fra moduli (`RECV_ID_COMM_BUS`). Nulla di tutto questo è
avionica: è trasporto.

La forma a sottoscrizione con periodo dichiarato separa due cadenze — quella con
cui il motore integra e quella con cui il client riceve — e questa separazione
si è rivelata necessaria anche qui, per una ragione misurata, al paragrafo 5.5.

### 3.4 Non esiste uno strato di sensori, e questo è il limite che ci riguarda

Nel simulatore le variabili `A:` **sono** la verità del modello. La velocità
indicata è la calibrata corretta per un errore di posizione dichiarato nella
configurazione del velivolo; la deriva del giroscopio e quella del barometro
sono due bit di una maschera di realismo; non esistono rumore con una densità
dichiarata, ritardo pneumatico, risoluzione di convertitore, ostruzione di presa
statica, latch di un inerziale.

Per un simulatore è la scelta giusta: a chi vola interessa che lo strumento sia
credibile, non che sia guasto in un modo particolare. Per questo progetto no, e
il motivo è scritto in ADR-004. Quell'ADR ha introdotto due sorveglianze — la
stasi e il confronto incrociato — che esistono per accorgersi di un sensore che
**mente**: un pitot ostruito che continua a trasmettere, un inerziale che si
latcha, un barometro che si pianta. Provarle richiede un sensore che mente, e un
motore le cui variabili sono la verità non lo produce mai.

Il banco quindi aggiunge ciò che il simulatore non ha, e lo tiene in un modulo
separato: `efis/fdm/sensors.hpp`.

### 3.5 I sistemi di bordo vivono nei comportamenti XML

`ModelBehaviorDefs/` contiene le logiche di Asobo in XML: ogni interruttore,
strumento, animazione è un *template* che legge variabili `A:` e scrive
variabili `L:`. Ne è stato estratto l'inventario dei nomi effettivamente usati —
`GROUND VELOCITY` (67 occorrenze), `AIRSPEED TRUE` (22), `SIM ON GROUND` (19),
`G FORCE` (19), `GENERAL ENG COMBUSTION:1` (17), `ELECTRICAL MAIN BUS VOLTAGE:1`
(6), `TURN INDICATOR RATE`, `KOHLSMAN SETTING`, `INCIDENCE ALPHA`,
`RECIP ENG CYLINDER HEAD TEMPERATURE`, `ELECTRICAL GENALT BUS AMPS` e altri. È
servito a una cosa concreta: distinguere, nel bus del banco, i nomi
**verificabili aprendo l'SDK** da quelli che vengono dalla documentazione. La
distinzione è pubblicata nel bus stesso (`NameOrigin`) e visibile con
`efis-fdm --list-vars`.

## 4. Che cosa si è ripreso, che cosa si è rifiutato

| Dell'architettura di FS 2024 | Scelta | Perché |
|---|---|---|
| Accesso per nome e unità dichiarata | **Ripreso** | Lo stesso consumatore può essere alimentato dal banco o da FS via SimConnect |
| Indice `:1` per la molteplicità | **Ripreso** | Motori, sbarre e cilindri senza moltiplicare i nomi |
| Nomi delle variabili dove noti | **Ripresi** | Un ponte verso FS diventa una traduzione di trasporto, non di semantica |
| Separazione fra cadenza di integrazione e di pubblicazione | **Ripresa** | Necessaria: vedi 5.5 |
| Variabili come verità del modello | **Rifiutato** | Rende improvabile ADR-004: i nomi compatibili portano il valore **misurato** |
| Errori degli strumenti come maschera di realismo | **Rifiutato** | Sostituito da modelli fisici di sensore con guasti iniettabili |
| Comandi quantizzati a 14 bit | **Rifiutato** | I comandi del banco sono frazioni di escursione in doppia precisione; la quantizzazione appartiene al sensore, non al comando |
| Serbatoi e punti di contatto enumerati | **Rifiutato in parte** | Tre punti di contatto, parametrici; un solo serbatoio, dichiarato come limite |
| Nome `PLANE PITCH DEGREES` letto in radianti | **Rifiutato** | Il bus dichiara l'unità in cui il valore è espresso davvero |

## 5. Architettura del motore su Linux

Libreria propria, `efis_fdm`, che dipende dal nucleo e da cui il nucleo non
dipende. La direzione è il confine che ADR-002 protegge: il display e il nodo EMS
non hanno nulla da fare con un modello aerodinamico.

```
efis_fdm
├── atmosphere   ISA, scostamenti, relazioni del tubo di Pitot
├── airframe     geometria, masse, derivate, flap, limiti, due cellule
├── propulsion   pistoni ed elica, turbina, termiche, elettrico, guasti
├── dynamics     sei gradi di libertà, vento, contatto al suolo
├── sensors      errore, ritardo, rumore, risoluzione, guasti
├── sim_vars     bus a nome e unità, compatibile e dichiarato
├── scenario_pilot  tiene il modello su un profilo
└── flight_bench    FlightSource + EngineSource verso l'EFIS esistente
```

### 5.1 Atmosfera e dati aria

ISA della troposfera e della bassa stratosfera, con due scostamenti che in volo
esistono sempre: pressione al livello del mare diversa dalla standard e
temperatura diversa. Senza il primo non si prova la regolazione barometrica;
senza il secondo non si prova la differenza fra quota di pressione e quota vera,
che è la ragione per cui un aeroplano si schianta contro una montagna in giornata
fredda.

Una scelta merita di essere dichiarata: **lo scostamento di temperatura non entra
nel profilo di pressione**. L'altimetro misura pressione e la converte con la
ISA, quindi in una giornata fredda la quota indicata resta quella che la ISA
associa a quella pressione — ed è esattamente l'errore di temperatura che si
vuole riprodurre. Un profilo di pressione che seguisse la temperatura vera lo
farebbe sparire dal modello.

Le relazioni del Pitot sono **compressibili**, non incomprimibili: la forma
`½ρV²` sbaglia di circa un nodo già a 200 kt al livello del mare e di più in
quota. Il banco genera la pressione d'impatto con la stessa relazione che lo
strumento invertirà, altrimenti si tara contro un errore proprio.

### 5.2 Cellula e aerodinamica

Modello a coefficienti adimensionali con derivate di stabilità: la forma
classica dei testi e dei rapporti di galleria. Non è la sola possibile — un
simulatore moderno integra le forze su elementi di superficie — ma è quella che
si può tarare con i numeri che un costruttore LSA possiede davvero (superficie
alare, apertura, pesi, velocità di stallo misurate) e che si può discutere riga
per riga.

Due proprietà del modello di portanza sono state scelte, non ereditate:

- il coefficiente massimo **non è un parametro**: discende dalla pendenza e
  dall'incidenza di stallo. Dichiararlo a parte darebbe due numeri capaci di
  contraddirsi, con il risultato che la velocità scritta sul nastro non sarebbe
  quella a cui l'aeroplano stalla;
- oltre lo stallo la portanza **cade**, raccordandosi al comportamento di lastra
  piana in dieci gradi. Troncare al massimo darebbe un aeroplano che resta
  appeso al proprio massimo e continua a volare, e l'allarme di stallo non si
  potrebbe provare.

Le derivate rispetto ai comandi sono **per escursione piena** e non per
radiante, come invece sono quelle rispetto a derapata e ratei. Il fattore è
l'escursione angolare della superficie, circa 0,44 rad per alettoni ed
equilibratore. Trascurarlo è stato un errore reale di questo lavoro, due volte:
un equilibratore due volte e mezzo troppo potente faceva cabrare a quaranta
gradi nello scenario di stallo, e un'imbardata inversa quattro volte troppo
grande dava alla rollata un rateo di prua di quattordici gradi al secondo dove
la virata corretta ne prevede tre e mezzo.

### 5.3 Propulsione

Relazioni canoniche, costanti di targa.

- Caduta di potenza del motore aspirato con la densità secondo la relazione di
  **Gagg e Ferrar**, `(σ − 0,117)/0,883`: l'attrito interno non diminuisce con
  la quota, quindi la potenza non è proporzionale alla densità.
- Regime del motore a pistoni dal **bilancio fra potenza disponibile e potenza
  assorbita dall'elica**, integrato sull'inerzia rotante. Non è pedanteria: ne
  escono gratis il regime che cala quando l'elica si carica in salita e quello
  che sale in picchiata, che una tabella dovrebbe elencare.
- Elica tarata al **punto di progetto in volo**, non da ferma. Tararla sulla
  condizione statica dà un passo troppo piccolo e in crociera il motore supera
  il regime massimo: errore che si vede subito, perché il contagiri lo mostra.
- Spinta dalla **teoria del disco attuatore in volo avanzato**,
  `P = T·V + T^{3/2}/√(2ρA)`, risolta per la spinta con quattro iterazioni di
  Newton. Vale da ferma, dove dà la spinta statica `(2ρAP²)^{1/3}`, e in
  crociera, dove tende a `ηP/V`. Non serve né una tabella di rendimento né un
  raccordo fra i due regimi, e un raccordo arbitrario avrebbe prodotto una
  discontinuità di accelerazione proprio durante la corsa di decollo.
- Termiche a primo ordine con **tre costanti di tempo diverse** perché tre masse
  diverse: scarico 4 s, teste 25 s, olio 120 s. Sono quelle che rendono
  riconoscibile un guasto: una temperatura di scarico che crolla in un secondo è
  un cilindro che ha smesso di bruciare, la stessa caduta in due minuti è il
  motore che si raffredda in discesa.
- Elettrico: la sbarra **cade subito** alla tensione della batteria quando
  l'alternatore esce di servizio, e la batteria si scarica lentamente. Sono due
  fenomeni e vanno modellati come due, altrimenti dopo dieci minuti di guasto la
  tensione è ancora 12,8 V, un valore che nessun allarme considera anomalo.

Guasti iniettabili sul **modello fisico**, non sul valore pubblicato: pompa del
carburante, perdita d'olio, alternatore, cilindro muto, spegnimento in volo.
Iniettare il numero avrebbe prodotto il valore senza le conseguenze, cioè
proprio il dato plausibile ma falso che ADR-004 vuole togliere di mezzo.

### 5.4 Dinamica

Sei gradi di libertà in assi corpo, angoli di Eulero, matrice d'inerzia
diagonale, terra sferica localmente piatta. Contatto al suolo a **tre punti** con
molla, smorzatore, attrito di rotolamento, freni, resistenza laterale degli
pneumatici e sterzo del ruotino: tre e non uno perché è la coppia fra ruotino e
principali a far alzare il muso, e senza di essa la rotazione al decollo non si
può provare. La corsa di decollo serve anche a un'altra cosa: è il momento in cui
il cancello sulla velocità al suolo di ADR-004 apre la sorveglianza della stasi.

Limiti dichiarati: oltre 85° di beccheggio gli angoli di Eulero degenerano e il
modello si ferma lì; nessun effetto di scia dell'elica sul piano di coda e
nessuna coppia di reazione, quindi l'aeroplano non imbarda dando motore, e un
monomotore vero lo fa; nessun prodotto d'inerzia; quota del terreno costante.

### 5.5 Due cadenze, non una

La cadenza di pubblicazione è del chiamante; quella di integrazione è del
modello, 100 Hz; quella del controllo è un terzo numero, 50 Hz. La separazione è
costata una riscrittura ed è il risultato più utile di questo lavoro per chi
scriverà scenari: **chi pubblica piano non deve per questo pilotare piano.** Un
comando tenuto fermo per due decimi di secondo introduce, sul modo di corto
periodo di un LSA, abbastanza ritardo di fase da invertire il segno della
correzione; la prima versione del banco, a 5 Hz, oscillava da fondo corsa a
fondo corsa e la crociera perdeva quattrocento piedi al minuto. Con le tre
cadenze separate la stessa prova a 5 Hz e a 50 Hz dà la stessa traiettoria.

### 5.6 Lo strato sensori

È la parte che giustifica il banco. Dalla verità del modello a ciò che l'EFIS
riceve:

- **errore di posizione della presa statica**, costante più termine
  proporzionale all'incidenza: è il motivo per cui la velocità indicata non
  coincide con la calibrata e per cui la differenza cresce avvicinandosi allo
  stallo, dove conta di più;
- **ritardo pneumatico** di Pitot e statica, modellato sulle pressioni e non
  sulle grandezze indicate, perché è lì che avviene: a ritardare è il tubo;
- **rumore** con deviazione standard ricavata da quello **misurato sul banco
  reale** il 21 agosto 2026 (`tools/tuning/measure_signal_noise.py`): 0,44 kt e
  2,00 ft al 99° percentile fra campioni consecutivi, cioè σ di 0,13 kt e
  0,60 ft. Non è realismo decorativo: le risoluzioni dello `StasisMonitor` sono
  tarate su quei numeri, e un banco più silenzioso del vero farebbe passare una
  taratura che in volo non scatterebbe;
- **risoluzione** di pubblicazione, distinta dalla soglia di decisione del
  nucleo;
- **satellitare** a 5 Hz con ritardo, dispersione, HDOP e conteggio satelliti:
  fra due aggiornamenti il valore resta quello, perché un ricevitore non
  interpola, ed è questo a rendere non banale il confronto fra velocità al suolo
  e velocità indicata in accelerazione;
- **temperatura totale** con fattore di recupero: una sonda immersa nel flusso
  misura la totale, non la statica, e a 250 kt la differenza vale quasi dieci
  gradi.

Nove guasti iniettabili, scelti perché sono quelli che l'EFIS deve riconoscere:
`pitot-blocked`, `pitot-leak`, `static-blocked`, `ahrs-latched`, `ahrs-drift`,
`gnss-outage`, `gnss-degraded`, `magnetometer-bias`, `oat-stuck`.

Un dettaglio che vale l'intero strato: **sotto guasto il rumore continua.** Un
tubo ostruito non ferma l'elettronica; il sensore continua a leggere la pressione
intrappolata con il proprio rumore. Se il rumore si fermasse, la stasi di
ADR-004 si rileverebbe dall'immobilità del bit meno significativo invece che dal
contenuto, e il banco confermerebbe il rilevatore invece di metterlo alla prova.
Due prove in `tests/fdm_tests.cpp` fissano l'invariante: il valore resta fermo
entro la risoluzione **e** continua a cambiare.

L'eccezione è il satellitare perso, che è dichiarato **assente**: `gnss_valid`
falso, zero satelliti, e nel modello dati `Validity::unavailable`. Uno zero
sarebbe peggio del guasto, perché zero e zero sono le coordinate di un punto nel
golfo di Guinea.

## 6. Le due cellule di riferimento

Due e non una perché i due mondi che l'EFIS deve servire pongono problemi
diversi, e un motore provato su una sola classe nasconde proprio quelli
dell'altra.

| | LSA | Professionale |
|---|---|---|
| Massa massima | 600 kg | 4740 kg |
| Superficie alare | 9,9 m² | 25,8 m² |
| Apertura | 8,4 m | 16,2 m |
| Propulsore | pistoni, 73,5 kW, riduttore 2,43 | turboelica, 895 kW, elica a giri costanti |
| Carrello | fisso | retrattile |
| Vs1 / Vs0 (dichiarate e prodotte) | 50 / 42 kt | 84 / 67 kt |
| VNO / VNE | 120 / 150 kt | 236 / 240 kt |
| Limite di Mach | non applicabile | 0,48 |
| Crociera di riferimento | 3000 ft | 16 000 ft |

L'LSA vola lento e vicino allo stallo: mette alla prova il nastro delle velocità
basse, l'allarme di stallo, l'incidenza. La velocità di stallo con flap estesi
resta entro i 45 kt calibrati, come la definizione di LSA impone, e un test lo
verifica: non è una coincidenza da controllare a mano, è un invariante.

La turboelica vola alta e veloce: mette alla prova la differenza fra calibrata e
vera (a 16 000 ft la vera supera l'indicata di oltre 40 kt), il numero di Mach,
la quota di densità, la catena motore a turbina con N1, ITT e coppia.

## 7. Il bus delle variabili

76 variabili, con il nome e l'unità dichiarati, e la **provenienza del nome**
pubblicata accanto: `SDK` se il nome compare nei `ModelBehaviorDefs` dell'SDK
installato, `DOC` se appartiene all'elenco documentato, `BENCH` se è
un'estensione del banco che in nessun simulatore commerciale esiste.

```
$ efis-fdm --list-vars
SDK   AIRSPEED INDICATED             knots                    108.300
SDK   AIRSPEED TRUE                  knots                    112.622
SDK   INDICATED ALTITUDE             feet                    3005.000
SDK   PLANE PITCH DEGREES            degrees                    0.412
SDK   TURN INDICATOR RATE            degrees per second         0.014
...
BENCH BENCH TRUTH AIRSPEED CALIBRATED knots                   108.016
BENCH BENCH TRUTH STALL SPEED        knots                     47.688
```

Tre regole del bus:

1. **I nomi compatibili portano il valore misurato.** È la differenza
   deliberata rispetto a FS: ciò che un avionico vedrebbe davvero.
2. **La verità sta sotto nomi propri**, con il prefisso `BENCH TRUTH`. Non è per
   il display: è per chi giudica il display. Un criterio di prova che non può
   calcolare la differenza fra misura e verità non può dire se l'EFIS ha
   sbagliato.
3. **L'assenza non è lo zero.** Una variabile indisponibile — posizione senza
   fix, turbina su un motore a pistoni, teste su una turbina — è dichiarata
   assente, e `find_sim_var` restituisce il nulla.

E una quarta regola, che è una correzione: `PLANE PITCH DEGREES` in Flight
Simulator si legge in **radianti**, perché il nome ricorda l'unità di un
simulatore di trent'anni fa e il valore no. Qui l'unità dichiarata è quella in
cui il valore è espresso davvero. Una trappola nota da trent'anni resta una
trappola.

## 8. Uso

### Riga di comando

```sh
# Crociera, dieci minuti, venti hertz, su file
efis-fdm --scenario cruise --duration-s 600 --rate-hz 20 --output crociera.jsonl

# Decollo dal campo, con vento al traverso e raffica
efis-fdm --scenario takeoff --wind 300/12/5 --duration-s 180

# Pitot ostruito al trentesimo secondo, durante la salita
efis-fdm --scenario profile --fail pitot-blocked --fail-at 30

# Turboelica in quota, giornata calda, QNH del giorno
efis-fdm --aircraft turboprop --scenario cruise --qnh 1003 --isa-offset 12

# Perdita d'olio in crociera
efis-fdm --scenario cruise --fail-engine oil-loss --fail-at 60

# Inventario del bus
efis-fdm --list-vars
```

Quattro profili: `cruise`, `takeoff` (cold and dark, avviamento, corsa,
rotazione, salita, livellamento), `profile` (salita, due virate, discesa),
`stall` (decelerazione a quota costante fino al cedimento, poi recupero).

Lo schema delle righe è `efis-next.fdm.v1`, una riga per campione, con quattro
oggetti: `truth`, `indicated`, `engine`, `controls`. Ogni riga porta
`"simulated":true`.

### Dentro il programma

```cpp
auto bench = std::make_shared<efis::fdm::FlightBench>(
    efis::fdm::light_sport_airframe(), environment);
bench->vehicle().place_in_flight(45.65, 12.19, 3000.0, 40.0, 100.0);
bench->align_sensors();

router.set_simulation(std::make_shared<efis::fdm::BenchFlightSource>(bench));
engine_router.set_simulation(std::make_shared<efis::fdm::BenchEngineSource>(bench));
```

Il banco si presenta come una sorgente qualunque, `FlightSource` e
`EngineSource`, e il display non lo distingue dalle altre: lo riconosce soltanto
dal marcatore che i suoi campioni portano, come vuole ADR-003. Le sorgenti
**leggono** e non fanno avanzare il banco: il tempo appartiene a chi possiede il
ciclo. Due sorgenti registrate sullo stesso banco lo farebbero avanzare due
volte per fotogramma, e l'aeroplano volerebbe al doppio della velocità
dichiarata.

Ogni campione porta `Validity::simulated` e `origin.simulated`, e un test lo
verifica su tutti i 32 canali. Questo motore rende quel vincolo **più**
importante di prima, non meno, perché i suoi numeri sono plausibili in un modo
che un copione non riesce a essere.

## 9. Verifica

`tests/fdm_tests.cpp`, eseguibile `efis_fdm_tests`, 17 gruppi. Tre contano più
degli altri.

**Le velocità dichiarate contro quelle prodotte.** La cellula dichiara una
velocità di stallo che finisce sul nastro; il modello ne produce una integrando
il moto. Nella prova di stallo il cedimento avviene a 48,1 kt calibrati contro i
50 dichiarati, al coefficiente di portanza massimo del modello: la differenza è
il fattore di carico poco sotto uno della discesa incipiente, non un errore di
taratura.

**La virata contro la relazione del nucleo.** `coordinated_turn_rate_deg_s`
calcola il rateo da inclinazione e velocità; qui il rateo arriva
dall'integrazione del moto, per una via che non contiene quella formula. In
virata stabilizzata a 25° le due concordano entro 0,5 °/s, con accelerazione
laterale sotto 0,01 g e fattore di carico 1,10 — che è `1/cos 25°`. Due vie
indipendenti che danno lo stesso numero descrivono la stessa fisica.

**Il rumore dei sensori contro le soglie di ADR-004.** Misurato a 10 Hz come il
banco reale, il 99° percentile delle differenze fra campioni consecutivi sta
sotto le risoluzioni dichiarate in `FlightModelPolicy` (1 kt, 5 ft) e sopra lo
zero. Le due disuguaglianze insieme sono la ragione per cui questo banco può
mettere alla prova ADR-004.

Altri numeri misurati, per riferimento: la crociera dell'LSA tiene 3000 ft con
scarto sotto il piede dopo l'assestamento, a 108 kt, con 5126 giri e 14,4 kg/h
di consumo; il decollo stacca a 67 kt e sale a 700–900 ft/min; la turboelica
tiene 16 000 ft a Mach 0,41 con l'elica a 1700 giri e ITT 646 °C.

Build pulita con `/W4 /permissive-` su MSVC 19.5x. Cinque suite di test
eseguite insieme (`efis_core_tests`, `efis_safety_tests`,
`efis_system_simulator_tests`, `efis_fdm_tests`, `efis_engine_tests`): tutte
verdi. La verifica con GCC `-Wall -Wextra -Wpedantic -Wconversion` sul
Raspberry Pi 5 resta da fare: è più severa di MSVC e appartiene al ciclo di
quel nodo.

## 10. Che cosa questo motore non fa

Dichiarato perché si sappia prima di incontrarlo, non dopo.

- Un solo serbatoio: lo sbilanciamento fra ali non si può provare.
- Nessun pilota automatico e nessuna radio: guida e navigazione restano
  indisponibili nello snapshot, invece di portare valori inventati.
- Nessun terreno: la quota di campo è una costante, e non esiste un modello di
  collisione oltre al suolo piatto.
- Nessuna coppia di reazione dell'elica, nessuna scia sul piano di coda.
- Nessun ghiaccio, nessun carico asimmetrico, nessun guasto strutturale.
- Il vento è uniforme con raffica deterministica: nessuna stratificazione,
  nessun gradiente al suolo, nessun wind shear.
- La turbina non è appiattita in coppia (flat rating): sotto i 10 000 piedi dà
  più potenza di una PT6 vera.
- Il ritardo del satellitare è un filtro del primo ordine, non una coda di
  campioni: sposta il valore nel tempo giusto ma lo addolcisce. Per un confronto
  fra catene è la parte che conta; per misurare una risposta a gradino no.

## 11. Seguito possibile

1. **Ponte SimConnect.** Un piccolo client Windows che sottoscrive le stesse
   variabili e le trasmette su UDP al banco Linux, che le pubblica sul proprio
   bus. Il lavoro è di trasporto, non di semantica, perché i nomi sono già gli
   stessi: è la ragione per cui il bus è stato costruito così. Da quel momento
   FS 2024 e questo motore diventano due sorgenti della stessa interfaccia, e la
   scelta fra i due è una questione di quale macchina si ha sotto mano — con
   l'avvertenza che FS porta la verità e non la misura, quindi i guasti dei
   sensori restano del banco.
2. **Taratura sulle cellule vere.** I coefficienti sono scalati su macchine di
   classe nota, non misurati. Una registrazione di volo con dati aria e inerziale
   permetterebbe di tarare pendenza di portanza, resistenza parassita e incidenza
   di stallo su una macchina precisa, e di far corrispondere il nastro al suo
   aeroplano.
3. **Secondo serbatoio e travaso**, per le prove di sbilanciamento.
4. **Due motori**, per la piantata asimmetrica, che è la pagina EMS più difficile
   da provare e la più importante.
5. **Stratificazione del vento e wind shear**, per l'allarme di gradiente in
   finale.
