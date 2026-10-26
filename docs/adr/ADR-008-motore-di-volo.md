# ADR-008 — Il motore di volo e lo strato dei sensori

Stato: accettata, 26 ottobre 2026
Riferimenti: ADR-002 (monolite modulare), ADR-003 (ingestione e trasporto),
ADR-004 (il dato plausibile ma falso), `docs/design/FLIGHT_MODEL_ENGINE_V1.md`,
`docs/SYSTEM-SIMULATOR.md`

## Contesto

Il progetto possedeva già una sorgente simulata e un simulatore di sistema a
scenari: dieci minuti di copione che percorrono accensione, rullaggio, decollo,
crociera, discesa, atterraggio e spegnimento, con quattro varianti di guasto. È
servito e resta: una regressione deve essere ripetibile al campione, e un
copione lo è per costruzione.

Ma un copione non lega le grandezze fra loro. A un certo istante la quota vale
tanto e la velocità tanto, perché così è scritto. Da questo discendono tre cose
che il banco non poteva fare, e sono esattamente quelle che ADR-004 ha reso
necessarie.

**Primo: le grandezze confrontabili non erano confrontabili.** Velocità indicata
e velocità al suolo, rateo di salita barometrico e satellitare, rateo di virata
e inclinazione sono tutte conseguenze dello stesso moto. Scritte a mano una per
una possono essere coerenti solo per caso. ADR-004 ha introdotto il confronto
incrociato fra catene fisicamente indipendenti; tararlo su dati incoerenti per
costruzione non significa nulla.

**Secondo: non esisteva una verità contro cui giudicare.** Il copione produceva
un solo numero per grandezza, che era insieme la verità e la misura. La domanda
«lo strumento ha sbagliato?» non era formulabile, perché non c'era niente
rispetto a cui sbagliare.

**Terzo: i guasti erano valori, non condizioni.** `InjectedFault` sostituiva un
numero o lo rendeva indisponibile. Il pitot ostruito vero non fa questo:
intrappola la pressione totale, e in salita la velocità indicata **cresce**
mentre quella vera cala, con il sensore che continua a dichiararsi sano. È il
caso principale di ADR-004, e il banco non lo produceva.

A questo si aggiunge il motivo esterno: il progetto deve servire due mondi, LSA
e professionale, e un banco provato su una sola classe di macchina nasconde
proprio i problemi dell'altra. Il nastro delle velocità basse e l'allarme di
stallo si provano su un ultraleggero; la differenza fra velocità calibrata e
vera, il numero di Mach e la catena motore a turbina si provano a sedicimila
piedi.

Flight Simulator 2024 è installato su questa macchina con il proprio SDK, ed è
stato analizzato — intestazioni, schemi, comportamenti XML pubblicati — per
capire come un motore maturo organizzi queste cose. L'analisi è in
`docs/design/FLIGHT_MODEL_ENGINE_V1.md`; qui conta il suo esito: il simulatore
espone un dizionario di variabili per nome e unità, e le sue variabili **sono la
verità del modello**. Non ha uno strato di sensori; gli errori degli strumenti
sono bit di una maschera di realismo. Per chi vola è la scelta giusta. Per
provare ADR-004 è precisamente ciò che non serve.

## Decisione

Si introduce `efis_fdm`: un motore di volo a sei gradi di libertà con uno strato
di sensori separato, scritto in questo progetto, in C++20, senza dipendenze oltre
la libreria standard e il nucleo. Cinque scelte, ciascuna con il proprio perché.

### 1. Si integra il moto invece di descriverlo

Sei gradi di libertà, forze e momenti da coefficienti adimensionali con derivate
di stabilità, propulsione da relazioni canoniche, contatto al suolo a tre punti.

È l'unico modo di ottenere vere **insieme** tutte le grandezze che l'EFIS deve
confrontare, perché diventano conseguenze dello stesso moto e non valori
indipendenti. Il costo è che il modello va tarato e può sbagliare; il guadagno è
che non può essere incoerente con se stesso. Un test lo verifica per la via più
severa possibile: il rateo di virata prodotto dall'integrazione concorda entro
mezzo grado al secondo con quello che `coordinated_turn_rate_deg_s` del nucleo
calcola da inclinazione e velocità, per una via che non contiene quella formula.

Il modello a coefficienti, e non a elementi di superficie, perché si tara con i
numeri che un costruttore LSA possiede davvero — superficie alare, apertura,
pesi, velocità di stallo misurate — e perché si legge riga per riga. Un modello a
elementi richiede una geometria che qui non esiste e produrrebbe numeri non
verificabili.

### 2. La verità e la misura sono due cose, e il confine è un modulo

`VehicleState` è la verità: cosa fa l'aeroplano. `IndicatedState` è ciò che
l'EFIS riceve: la verità più errore di posizione, ritardo pneumatico, rumore,
risoluzione, e il guasto se è stato iniettato. Fra i due c'è `SensorSuite`, che
è la sola parte del motore autorizzata a sporcare un numero.

La conseguenza che vale da sola il confine: in ogni istante il banco conosce
entrambe le versioni della stessa grandezza, e la loro differenza è l'errore
dello strumento, **misurato invece che supposto**. Un criterio di prova che non
possa calcolarla non può dire se l'EFIS ha sbagliato.

### 3. Il rumore è quello misurato sul banco reale

Le deviazioni standard dello strato sensori discendono dal rumore misurato il
21 agosto 2026 sulla registrazione del banco a 10 Hz: 0,44 kt e 2,00 ft al 99°
percentile fra campioni consecutivi.

Non è realismo decorativo. Le risoluzioni dello `StasisMonitor` sono tarate su
quei numeri, e la taratura ha un verso: la risoluzione deve stare **sopra** il
rumore, altrimenti il ballo del sensore passa per movimento e il guasto non viene
dichiarato mai. Un banco più silenzioso del vero farebbe passare una taratura
inerte. Un test fissa le due disuguaglianze: il rumore del banco sta sotto le
risoluzioni dichiarate e sopra lo zero.

Da qui anche la regola che governa i guasti: **sotto guasto il rumore
continua.** Un tubo ostruito non ferma l'elettronica; il sensore continua a
leggere la pressione intrappolata con il proprio rumore. Se si fermasse, la
stasi si rileverebbe dall'immobilità del bit meno significativo invece che dal
contenuto, e il banco confermerebbe il rilevatore invece di metterlo alla prova.

### 4. I guasti si iniettano sulla condizione, non sul valore

Nove guasti di sensore e cinque di propulsore, tutti applicati al modello
fisico. Il pitot ostruito congela la pressione totale **dell'istante in cui si
ostruisce**, e la velocità indicata cresce in salita da sola. La perdita d'olio
fa cadere la pressione e salire la temperatura, nell'ordine in cui succede. La
pompa del carburante guasta fa cadere la pressione, e dalla pressione la
potenza, e dalla potenza le temperature.

Iniettare il numero avrebbe prodotto il valore senza le conseguenze, cioè
proprio il dato plausibile ma falso che ADR-004 vuole togliere di mezzo, con
l'aggravante di essere generato da noi.

Un'eccezione, e va dichiarata come tale: il satellitare perso non produce un
valore sbagliato, produce un'**assenza**. `gnss_valid` falso, zero satelliti, e
nel modello dati `Validity::unavailable`. Uno zero sarebbe peggio del guasto,
perché zero e zero sono le coordinate di un punto nel golfo di Guinea.

### 5. Il bus delle variabili prende la forma di SimConnect, non il suo contenuto

Accesso per nome e unità dichiarata, indice `:1` per la molteplicità, nomi
compatibili dove sono noti. Lo stesso consumatore può quindi essere alimentato
da questo banco su Linux o da Flight Simulator attraverso SimConnect: un ponte
futuro è lavoro di trasporto, non di semantica.

Con due differenze deliberate. I nomi compatibili portano il valore **misurato**,
non la verità, perché è ciò che un avionico vedrebbe davvero. E la verità è
pubblicata a parte, sotto nomi che cominciano con `BENCH TRUTH` e che nessun
simulatore commerciale ha: non è per il display, è per chi giudica il display.

Ogni variabile dichiara anche la **provenienza del proprio nome** — trovato
nell'SDK installato, documentato, oppure estensione del banco. È
un'informazione di verificabilità: un nome confermato si può controllare aprendo
l'SDK, uno di documentazione no.

## Conseguenze

Il banco può ora produrre, a comando e in modo ripetibile, le condizioni per cui
le sorveglianze di ADR-004 sono state scritte. Quattro delle prove di
`tests/fdm_tests.cpp` sono esattamente quei casi, e prima non erano scrivibili:
il pitot ostruito in salita, la statica ostruita, l'inerziale latchato che
conserva il proprio rumore, il satellitare dichiarato assente.

Il simulatore di sistema **resta** e non è stato toccato. Non è una duplicazione:
sono due strumenti per due domande diverse. Il copione risponde a «il percorso
dati si comporta come l'altra volta?» e lo fa al campione; il motore risponde a
«lo strumento dice la verità in questa condizione fisica?» e lo fa con una
verità nota. Chi scrive una regressione di trasporto usa il primo; chi tara una
soglia usa il secondo.

Tre cose restano aperte, e la prima non si chiude scrivendo codice.

1. **La taratura delle cellule.** I coefficienti sono scalati su macchine di
   classe nota, non misurati sulle macchine vere, e il documento di progetto li
   marca `DEVELOPMENT` uno per uno. Servono a far volare un aeroplano plausibile
   per provare un EFIS, non a stimare prestazioni, e nessun numero prodotto dal
   banco va usato per calcolare una prestazione o una autonomia. Si chiude con
   una registrazione di volo vera.
2. **La verifica con GCC severo.** La build è pulita con `/W4 /permissive-` su
   MSVC; `-Wall -Wextra -Wpedantic -Wconversion` su GCC aarch64 appartiene al
   ciclo del Raspberry Pi 5 ed è più severa.
3. **Il ponte verso FS 2024.** È il seguito naturale e il bus è stato costruito
   per renderlo possibile, ma non esiste ancora.

Un limite di metodo va registrato qui e non soltanto nel documento di progetto:
il motore è stato **validato contro se stesso e contro relazioni analitiche**,
non contro dati di volo. Concorda con la ISO 2533 per l'atmosfera, con la
relazione della virata corretta per la virata, con la definizione di LSA per lo
stallo con flap, con le costanti di targa di un motore di classe Rotax 912 per il
consumo. Nessuna di queste è una registrazione di volo. È una validazione
onesta e insufficiente, e sapere quale delle due cose sia conta più del fatto che
le prove siano verdi.

## Alternative considerate

**Estendere il simulatore di sistema con più grandezze legate fra loro.** È la
strada che sembra più economica e non lo è: legare quattro grandezze a mano
significa scrivere a mano le relazioni della meccanica del volo, cioè fare la
stessa cosa in modo peggiore, senza il vincolo di coerenza che l'integrazione
impone gratis. E non produrrebbe mai la verità separata dalla misura.

**Usare Flight Simulator 2024 via SimConnect come unica sorgente.** Scartata
come *unica*, adottata come possibile *aggiunta*. Le sue variabili sono la verità
del modello: non esiste rumore con una densità dichiarata, né ritardo
pneumatico, né ostruzione di presa, né latch di inerziale. Un EFIS provato contro
quelle variabili non può essere messo alla prova su ciò che ADR-004 chiede, che è
accorgersi di un sensore che mente. Inoltre richiede Windows e una licenza
accanto a ogni banco, mentre i nodi del progetto sono Raspberry sotto Linux.
Resta il seguito del paragrafo 11 del documento di progetto: come **secondo**
fornitore dello stesso bus, per il mondo fuori dall'aeroplano — terreno,
aeroporti, meteo — che questo motore non ha e non avrà.

**Integrare JSBSim o FlightGear.** È l'alternativa seria, e va detto: JSBSim è
un motore di dinamica del volo maturo, validato su dati di volo veri, usato anche
in ambito di ricerca, e scrivere un motore nuovo accanto a uno validato è una
cosa che richiede una giustificazione. Tre ragioni, nessuna delle quali è «lo
facciamo meglio».

La prima è che il valore di questo lavoro non è la dinamica: è lo strato dei
sensori, che JSBSim non fornisce. Adottare JSBSim avrebbe risolto la parte già
risolta dalla meccanica del volo e lasciato intatta quella che riguarda ADR-004.

La seconda è il vincolo di ADR-001: le API pubbliche del nucleo non dipendono da
librerie esterne, e i nodi sono Raspberry dove ogni dipendenza va costruita,
versionata e mantenuta su cinque macchine.

La terza è la leggibilità, che in questo progetto non è una preferenza estetica:
ogni soglia e ogni costante del banco deve poter essere discussa riga per riga,
perché il banco è ciò con cui si tarano le soglie dell'EFIS. Un numero che arriva
da un modello XML di terzi non si discute, si accetta.

La decisione è però costruita per non essere definitiva: `Vehicle` e
`SensorSuite` sono separati, e un'implementazione alternativa della dinamica —
JSBSim incluso — può stare dietro la stessa interfaccia senza toccare né lo
strato sensori, né il bus, né il ponte verso l'EFIS. Se un giorno servirà una
dinamica validata su dati di volo, il confine per metterla c'è già.

**Modellare i guasti come una maschera di realismo, come fa FS.** Scartata: un
bit che accende la «deriva del giroscopio» non dice di quanto, da quando, né con
quale rumore residuo, e sono esattamente i tre numeri su cui si tara una soglia.

## Verifica

`tests/fdm_tests.cpp`, eseguibile `efis_fdm_tests`, 17 gruppi di prove. Le tre
che portano il peso sono dichiarate nell'intestazione del file: le velocità
dichiarate contro quelle prodotte, la virata contro la relazione del nucleo, il
rumore dei sensori contro le soglie di ADR-004.

Risultati misurati il 26 ottobre 2026: stallo a 48,1 kt calibrati contro i 50
dichiarati, al coefficiente di portanza massimo del modello; virata stabilizzata
a 25° con rateo entro 0,5 °/s dalla relazione del nucleo, accelerazione laterale
sotto 0,01 g e fattore di carico 1,10; crociera dell'LSA che tiene 3000 ft a
108 kt con 5126 giri e 14,4 kg/h; turboelica che tiene 16 000 ft a Mach 0,41.

Cinque suite eseguite insieme con `ctest` — nucleo, sicurezza, simulatore di
sistema, motore di volo, sezione motore — tutte verdi. Build pulita con
`/W4 /permissive-` su MSVC 19.5x.
