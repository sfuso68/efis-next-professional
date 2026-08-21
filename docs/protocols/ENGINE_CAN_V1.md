# Engine CAN V1

Stato: **DEVELOPMENT / NON-CERT**. Versione wire `1`.

## Scopo

Trasportare le misure del nodo motore, oggi un ESP32-S3, verso il nodo di
ingestion. È il bus del motore, distinto dal CAN1 del Pixhawk su cui parla il
sensore aria in DroneCAN: quel bus non va toccato da questo protocollo.

Il nodo motore **misura e trasmette**. Non riceve comandi, non ha un canale di
scrittura, e nulla in questa specifica ne definisce uno. Vedi paragrafo 25
dell'handoff.

## Perché non DroneCAN

DroneCAN è già in uso sul bus del Pixhawk e sarebbe stata la scelta naturale.
Non viene adottata qui per tre ragioni concrete: richiede uno stack di dimensioni
sproporzionate su un ESP32-S3 che deve soltanto leggere sonde; introduce
allocazione dinamica e frammentazione dove servono frame singoli a periodo fisso;
e rende difficile verificare a occhio, con `candump`, che una sonda stia
trasmettendo. Un formato di poche righe, leggibile su un terminale, si diagnostica
in officina senza strumenti.

La decisione vale per il bus motore. Il sensore aria resta DroneCAN, perché è il
firmware Matek a deciderlo.

## Identificativo

CAN 2.0A, identificatore standard a 11 bit, nessun frame esteso.

```
ID = (node << 7) | message
     node    4 bit, 1..15   identita' del nodo che trasmette
     message 7 bit, 1..127  contenuto del frame
```

Il nodo `0` non è valido: uno zero significa "non dichiarato" in tutto il
progetto, mai "il primo nodo". Un frame con nodo zero viene scartato e contato.

L'identificativo più basso ha priorità più alta sul bus, quindi i messaggi che
contano di più portano il numero più basso: la pressione dell'olio arriva prima
della quantità di carburante.

## Formato dei frame

Tutti i valori sono **little-endian**, come il resto del progetto.

Il primo byte di ogni frame è un contatore di sequenza a 8 bit che il nodo
incrementa a ogni trasmissione **dello stesso messaggio** e che si azzera dopo
255. Serve a distinguere un nodo fermo che ripete l'ultimo frame da un nodo che
sta misurando: senza, un guasto che blocca la trasmissione su un valore plausibile
resterebbe invisibile.

| ID | Nome | Periodo | Payload |
|---:|---|---:|---|
| `0x01` | `ENGINE_PRIMARY` | 100 ms | `seq` u8, `rpm` u16, `manifold` u16, `oil_pressure` u16 |
| `0x02` | `ENGINE_FLUIDS` | 200 ms | `seq` u8, `oil_temperature` i16, `fuel_pressure` u16, `fuel_flow` u16 |
| `0x03` | `ENGINE_FUEL` | 500 ms | `seq` u8, `fuel_left` u16, `fuel_right` u16 |
| `0x04` | `ENGINE_ELECTRICAL` | 200 ms | `seq` u8, `bus_voltage` u16, `alternator_current` i16 |
| `0x05` | `CHT_1_2` | 200 ms | `seq` u8, `cht_1` i16, `cht_2` i16 |
| `0x06` | `CHT_3_4` | 200 ms | `seq` u8, `cht_3` i16, `cht_4` i16 |
| `0x07` | `EGT_1_2` | 200 ms | `seq` u8, `egt_1` i16, `egt_2` i16 |
| `0x08` | `EGT_3_4` | 200 ms | `seq` u8, `egt_3` i16, `egt_4` i16 |
| `0x7F` | `NODE_HEARTBEAT` | 1000 ms | `seq` u8, `uptime_s` u32, `flags` u8 |

I cilindri sono divisi in due frame da due perché quattro valori a 16 bit più il
contatore non entrano negli otto byte di un frame CAN. Dividerli a coppie mantiene
il formato regolare e lascia spazio per motori a sei cilindri, che useranno
`0x09` e `0x0A`.

## Fattori di scala

Il bus trasporta interi; la conversione avviene nell'adapter, e il valore
trasmesso resta conservato come valore grezzo del campione.

| Grandezza | Unità | Scala | Offset |
|---|---|---:|---:|
| `rpm` | rpm | 1 | 0 |
| `manifold` | inHg | 0.01 | 0 |
| `oil_pressure` | psi | 0.1 | 0 |
| `oil_temperature` | °C | 0.1 | 0 |
| `fuel_pressure` | psi | 0.01 | 0 |
| `fuel_flow` | gal/h | 0.01 | 0 |
| `fuel_left`, `fuel_right` | gal | 0.01 | 0 |
| `bus_voltage` | V | 0.01 | 0 |
| `alternator_current` | A | 0.01 | 0 |
| `cht_*`, `egt_*` | °C | 0.1 | 0 |

Le grandezze che non possono essere negative usano `u16`; la temperatura
dell'olio e le temperature dei cilindri usano `i16` perché un motore freddo
d'inverno sta sotto zero, e la corrente dell'alternatore perché durante la
ricarica scorre nei due versi.

## Valore non disponibile

Un sensore che il nodo non riesce a leggere trasmette `0xFFFF` per un campo `u16`
e `0x8000` per un campo `i16`. L'adapter li traduce in `Validity::unavailable` e
non in uno zero.

Non è una raffinatezza. Uno zero è un valore, e un valore si mostra: una pressione
dell'olio a zero è un allarme, mentre una sonda scollegata è un'altra cosa e
richiede un'altra azione. Confonderle è precisamente il dato plausibile ma falso
che l'`ADR-004` vieta.

## Flag del battito

`NODE_HEARTBEAT.flags`, un bit per condizione:

| Bit | Significato |
|---:|---|
| 0 | taratura caricata e valida |
| 1 | almeno una sonda dichiarata guasta dal nodo |
| 2 | alimentazione del nodo fuori tolleranza |
| 3 | il nodo è stato riavviato da meno di dieci secondi |

Il bit 3 esiste perché un riavvio azzera il contatore di sequenza: senza,
l'ingestion leggerebbe una regressione e la chiamerebbe guasto.

## Regole per l'adapter

1. Un frame la cui lunghezza non è **esattamente** quella del messaggio viene
   scartato e contato, in difetto come in eccesso. A differenza di MAVLink, qui
   non esiste il taglio degli zeri finali: la lunghezza è fissa.

   Il controllo in eccesso serve più di quanto sembri. Su un bus condiviso passa
   traffico di altri protocolli che, per come è composto l'identificatore, ricade
   su un nodo e un messaggio validi: un frame CANopen `0x601` da otto byte si
   legge come `ENGINE_PRIMARY` del nodo 12. La lunghezza è ciò che li distingue,
   ed è stata la prova su `vcan0` a mostrarlo, non il ragionamento.
2. Un identificativo di messaggio non in tabella viene contato e ignorato. Non è
   un errore: è un nodo che dice più di quanto ci serva.
3. Il contatore di sequenza va esteso a 64 bit prima di darlo al monitor dei
   nodi, perché si azzera ogni 256 frame.
4. Un valore fuori dai limiti dichiarati nel profilo motore non viene corretto e
   non viene nascosto: resta con `Validity::out_of_range`, perché è un dato che
   il pilota deve vedere.
5. Nessun valore viene inventato per un campo mai ricevuto.

## Prova senza hardware

Il nodo black box ha `vcan0`. Un frame si inietta con `cansend` e si osserva con
`candump`, quindi l'adapter si verifica per intero prima che esista una scheda.

```sh
sudo ip link add dev vcan0 type vcan 2>/dev/null || true
sudo ip link set up vcan0
cansend vcan0 081#0C6009B60302
candump -L vcan0 > cattura.log
```

## Limiti correnti

I periodi in tabella sono un punto di partenza ragionevole, non una misura: vanno
confermati sul bus reale con il carico di tutti i nodi presenti.

La taratura di ogni sonda e i limiti del motore restano da verificare sul manuale
del propulsore, come impone il paragrafo 5 dell'handoff. Finché non lo sono,
nessun limite di questo documento può diventare operativo.
