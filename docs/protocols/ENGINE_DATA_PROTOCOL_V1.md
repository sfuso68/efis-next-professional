# Engine Data Protocol V1 (EDP V1)

Stato: contratto wire congelato software da `AVIONIX_DATA_ICD_V1.md`. Per
offset, dimensioni complete, enumerazioni, vettori e livelli di evidenza fa
fede l'ICD sovraordinato.

EDP V1 distribuisce uno `EngineSnapshot` normalizzato dalla Blackbox al nodo
EMS via UDP unicast, porta predefinita `49003`.

## Envelope di rete

Tutti gli interi sono big-endian e i `double` sono IEEE-754 a 64 bit.

| Campo | Byte |
|---|---:|
| magic `EDP1` | 4 |
| schema version (`1`) | 2 |
| source node | 4 |
| publisher instance | 8 |
| sequence | 8 |
| monotonic microseconds | 8 |
| payload length | 4 |
| payload CRC32C | 4 |

L'envelope misura 42 byte. Il payload canonico misura 652 byte (header da 4
byte e 18 record da 36 byte), quindi il datagramma completo misura 694 byte.

Il payload contiene health, eventuale fault iniettato, conteggio campioni e 18
campioni obbligatori: RPM, MAP, pressioni/temperature olio e carburante, flusso,
due quantità carburante, tensione, corrente alternatore, quattro CHT e quattro
EGT. Un segnale assente, duplicato o sconosciuto invalida l'intero datagramma.

Ogni campione serializza: signal id, valore ingegneristico, source id, validity,
quality, unità, età in millisecondi, simulated, origin node, origin channel,
calibration revision e raw value.

## Politica consumer

- `LIVE`: ultimo pacchetto ricevuto da non oltre 250 ms;
- `STALE`: oltre 250 ms e fino a 1000 ms, campioni marcati stale;
- `LOST`: oltre 1000 ms, campioni non disponibili;
- identità inattesa, CRC errato, duplicato e sequenza inversa sono scartati.

Le soglie sono configurabili nel codice del consumer. EDP non fornisce comandi,
ACK o scritture verso il bus sensori.
