# ADR-003: ingestion dei sensori e confini di trasporto

- Stato: accettata per il backbone dati P1
- Data: 2026-08-21

## Decisione

L'ingestion dei bus sensore avviene su un solo nodo, la black box. È l'unico
punto del sistema che conosce MAVLink, DroneCAN e i protocolli dei moduli
motore. I display ricevono stato già normalizzato e arbitrato, mai frame grezzi.

La catena è: Pixhawk → black box → display.

### Direzioni di dipendenza

Sono ammesse soltanto queste, e sono verificabili leggendo gli include:

- il **core** (`efis::`) non dipende da alcun trasporto, da alcun protocollo di
  bus e da Qt. Definisce campioni, salute dei nodi, arbitrato e snapshot;
- gli **adapter** (`efis::mavlink`, in futuro CAN ed ESP32) dipendono dal core e
  dal proprio protocollo. Producono campioni, non li mostrano e non li scelgono;
- il **trasporto** (`efis::io`) dipende dal core e non conosce alcun protocollo:
  muove byte e basta;
- il **renderer** dipende dal core e da Qt. Non conosce alcun adapter, alcun
  trasporto e alcun bus.

Nessuna freccia punta all'indietro. Un adapter che includesse un'intestazione del
renderer, o un renderer che includesse un adapter, è un errore di progetto, non
una scorciatoia.

### Perché il display non parla MAVLink

Non è una preferenza estetica. Un display che analizza i frame del controllo di
volo diventa responsabile di validità, timeout e arbitrato mentre sta anche
disegnando; e ogni display in più ripete quella responsabilità, con la
possibilità di arrivare a conclusioni diverse sugli stessi dati. Lo stato deve
essere deciso una volta sola, dove viene anche registrato.

### Il trasporto è astratto, e il replay non è un caso particolare

`efis::io::ByteSource` fornisce byte senza dichiarare da dove vengano. Seriale,
rete e replay da registrazione sono la stessa cosa per chi legge.

Il replay non è uno strumento di prova aggiunto in coda: è un trasporto di prima
classe, richiesto dal paragrafo 17.3 dell'handoff, ed è la ragione per cui si
implementa prima della seriale. Una registrazione reale rende ogni prova
successiva riproducibile, mentre un collegamento fisico no.

## Vincolo

Il percorso è di sola lettura. Nessun componente di questa catena trasmette verso
il controllo di volo: né comandi, né richieste di stream, né scritture di
parametri. La configurazione del Pixhawk resta un'attività separata, manuale e
tracciata, come prescrive il paragrafo 25 dell'handoff.

## Limiti correnti

Il formato sul filo fra black box e display non è deciso qui: il Display Data
Protocol V2 ne definisce l'envelope e le regole, e la serializzazione richiede
prove su CAN ed Ethernet. Sarà oggetto di una ADR separata.

Al 21 agosto 2026 il Pixhawk è collegato al portatile di terra e non alla black
box: la topologia descritta è quella di destinazione. Finché il collegamento
fisico non esiste, l'ingestion è alimentata dal replay, che non richiede di
anticipare alcuna scelta di cablaggio.
