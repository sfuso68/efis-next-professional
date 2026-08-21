# Protocollo CAN del nodo sensori ESP32 — V1

Stato: **DEVELOPMENT / NON-CERT**
Data: 21 agosto 2026

## Provenienza di questo documento

Il protocollo non è stato inventato qui. È stato **ricavato dal firmware reale**
del nodo sensori, `EFIS_Freenove_v4_8_.ino`, versione `FW_VERSION "4.0.0"`, che
al momento della stesura è l'unica descrizione autorevole esistente: il firmware
rimanda a un `PROTOCOLLO_CAN.md` che non è presente in nessuno dei repository.

Questo ha una conseguenza da tenere presente ogni volta che si legge questo
documento: **la fonte è il codice che trasmette, non una specifica concordata.**
Se il firmware cambia senza che cambi questo file, l'adapter continuerà a
interpretare i frame secondo una versione che non esiste più, e lo farà in
silenzio, perché il CAN non porta un numero di versione. Il paragrafo 1
dell'handoff chiede di recuperare dai sistemi precedenti ciò che si è dimostrato
valido, **dopo revisione**: questo documento è quella revisione.

Prima di un uso operativo il protocollo va concordato con il firmware e dotato di
un campo di versione, oggi assente.

## Trasporto

| Elemento | Valore |
|---|---|
| Bus | CAN 2.0A, identificatori a 11 bit |
| Velocità | 500 kbit/s |
| Transceiver | SN65HVD230, GPIO 17 TX e 18 RX sull'ESP32 |
| Ordine dei byte | little-endian, confermato leggendo `put_i16` |
| Lunghezza | 8 byte per tutti i frame |

Esiste anche una via UART a 115200 8N1 con payload testuale e CRC16, che il
firmware definisce esplicitamente di riserva rispetto al CAN. Non è coperta da
questa versione del documento.

## Cadenza

Il ciclo di comunicazione è `COMM_PERIOD_MS = 100`, cioè **10 Hz**. Assetto, dati
aria e salute sono inviati a ogni ciclo; posizione e diagnostica una volta ogni
dieci cicli, quindi a **1 Hz**.

Un commento nel firmware parla di 50 Hz e 5 Hz: è rimasto indietro rispetto alla
costante, e i numeri giusti sono quelli qui sopra. Vale la pena saperlo, perché
una soglia di scadenza tarata sul commento invece che sul codice dichiarerebbe
perduto un nodo perfettamente sano.

## Frame

### 0x100 — ATTITUDE, 10 Hz

| Byte | Campo | Tipo | Scala |
|---|---|---|---|
| 0–1 | pitch | int16 | 0,01 ° |
| 2–3 | roll | int16 | 0,01 ° |
| 4–5 | heading | int16 | 0,01 ° |
| 6 | stato sensori | bitmask | bit 0 IMU, bit 1 barometro, bit 2 pitot |
| 7 | calibrazione | 4 campi da 2 bit | bit 0–1 sistema, 2–3 magnetometro, 4–5 giroscopio, 6–7 accelerometro |

I quattro campi di calibrazione sono quelli del BNO055 e valgono da 0 a 3, dove 0
significa non calibrato e 3 pienamente calibrato. **Non sono decorativi**: un
assetto prodotto da una fusione non calibrata è un numero plausibile e privo di
significato, ed è esattamente il genere di dato che il paragrafo 22 dell'handoff
vieta di presentare come misura.

Nota sull'ordine: pitch precede roll, al contrario di quasi ogni altra
rappresentazione del progetto. È una fonte di errore silenzioso, perché
scambiarli produce due numeri entrambi plausibili.

### 0x101 — AIR_DATA, 10 Hz

| Byte | Campo | Tipo | Scala |
|---|---|---|---|
| 0–1 | velocità indicata | int16 | 0,1 kt |
| 2–3 | quota | int16 | 1 ft |
| 4–5 | pressione | int16 | (hPa − 1000) × 10 |
| 6–7 | pressione differenziale | int16 | 0,1 Pa |

L'offset di 1000 hPa sulla pressione serve a farla stare in un intero con segno.
Va rimosso in lettura: dimenticarlo produce una pressione di circa 13 hPa, che è
riconoscibile, oppure di 1013 hPa quando il valore è vicino a zero, che non lo è.

### 0x200 — HEALTH, 10 Hz

| Byte | Campo | Tipo | Scala |
|---|---|---|---|
| 0–1 | heartbeat | uint16 | contatore, avvolge a 65536 |
| 2–3 | fattore di carico | int16 | 0,01 g |
| 4–5 | temperatura | int16 | 0,1 °C |
| 6–7 | campo magnetico | int16 | 0,1 µT |

Il heartbeat è la sequenza su cui si basa la sorveglianza del nodo. Avvolge, e va
trattato di conseguenza: interpretare l'avvolgimento come una regressione
dichiarerebbe un guasto ogni 6553 secondi su un nodo che funziona.

### 0x300 — GPS_POS, 1 Hz

| Byte | Campo | Tipo | Scala |
|---|---|---|---|
| 0–3 | latitudine | int32 | 1e-7 ° |
| 4–7 | longitudine | int32 | 1e-7 ° |

### 0x301 — GPS_VEL, 1 Hz

| Byte | Campo | Tipo | Scala |
|---|---|---|---|
| 0–1 | velocità al suolo | int16 | 0,1 kt |
| 2–3 | rotta | int16 | 0,1 ° |
| 4–5 | quota GNSS | int16 | 1 m |
| 6–7 | riservato | — | sempre 0 |

### 0x302 — GPS_STATUS, 1 Hz

| Byte | Campo | Tipo | Scala |
|---|---|---|---|
| 0 | fix | uint8 | 0 assente, 1 presente |
| 1 | satelliti | uint8 | conteggio |
| 2–3 | HDOP | int16 | 0,1 |
| 4–7 | riservato | — | sempre 0 |

Il fix è **booleano**, non un tipo di fix come in MAVLink: da questo frame non si
distingue un fix bidimensionale da uno tridimensionale. È una perdita di
informazione rispetto al satellitare del Pixhawk, e va tenuta presente quando le
due sorgenti vengono confrontate.

### 0x201 — DIAG, 1 Hz

| Byte | Campo | Tipo |
|---|---|---|
| 0 | versione firmware, major | uint8 |
| 1 | versione firmware, minor | uint8 |
| 2 | causa del reset | uint8, 0 alimentazione, 1 software, 2 panic, 3 watchdog, 4 brownout, 5 altro |
| 3 | conteggio avvii | uint8, saturato a 255 |
| 4 | conteggio crash | uint8, saturato a 255 |
| 5–6 | heap libero | uint16, kB |
| 7 | riservato | — |

Il conteggio dei crash e la causa del reset sono informazioni operative vere, non
diagnostica di comodo: un nodo che si riavvia per watchdog mentre vola continua a
trasmettere valori corretti fra un riavvio e l'altro, e senza questo frame la
cosa resterebbe invisibile.

### 0x018 — CMD, in ingresso al nodo

Il firmware **riceve** comandi su questo identificativo. EFIS NEXT non lo
trasmette e non lo trasmetterà: l'adapter è di sola lettura, come tutto ciò che
sta al primo livello della scala del paragrafo 25 dell'handoff. L'identificativo
è documentato qui perché vada riconosciuto e ignorato, non perché vada usato.

## Regole di validità applicate dall'adapter

Il nodo dichiara il proprio stato e va creduto, ma non oltre quello che dice.

| Condizione | Effetto |
|---|---|
| bit IMU a zero | assetto non utilizzabile |
| calibrazione di sistema 0 | assetto non utilizzabile |
| calibrazione di sistema 1 o 2 | assetto degradato |
| calibrazione magnetometro 0 | prua non utilizzabile |
| calibrazione magnetometro 1 o 2 | prua degradata |
| bit barometro a zero | quota e pressione non utilizzabili |
| bit pitot a zero | velocità indicata e pressione differenziale non utilizzabili |
| fix assente | posizione, velocità al suolo e rotta non utilizzabili |

La distinzione fra prua e resto dell'assetto non è pedanteria: sul BNO055 la prua
dipende dalla calibrazione magnetica, mentre rollio e beccheggio no. Trattarli
insieme significherebbe o buttare via un assetto buono o presentare una prua che
non lo è.

## Che cosa questo nodo aggiunge

È una **catena indipendente** rispetto al Pixhawk per assetto, dati aria e
posizione. Non è una ridondanza formale: sensori diversi, alimentazione diversa,
bus diverso, firmware diverso. È la seconda opinione che rende utile il confronto
incrociato di `ADR-004`, e la ragione per cui l'arbitrato di `ADR-005` è per
grandezza e non per sorgente.
