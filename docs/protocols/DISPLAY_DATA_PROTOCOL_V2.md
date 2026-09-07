# Display Data Protocol V2

Stato: codec, session layer e adapter UDP unicast implementati e verificati
software; trasporto su hardware non ancora verificato, versione wire `2.0`.

## Scopo

Trasportare verso renderer e strumenti solo stato normalizzato. Il protocollo non
trasporta oggetti Qt/LVGL né autorizza comandi di volo.

## Envelope

Ogni messaggio contiene:

| Campo | Tipo | Regola |
|---|---|---|
| `schema` | `u16` | valore `2` |
| `profile` | enum | `standard`, `professional`, `degraded` |
| `source_node` | `u32` | identità configurata |
| `publisher_instance` | `u64` | casuale/univoco per ogni avvio del publisher |
| `sequence` | `u64` | crescente per publisher |
| `monotonic_us` | `u64` | tempo monotono publisher |
| `payload_type` | enum | snapshot, alert, heartbeat, config |
| `payload_length` | `u32` | limite da definire per trasporto |
| `payload_crc32c` | `u32` | obbligatorio su datagrammi/CAN |

Il wire format usa ordine di byte di rete (big endian), magic `EFN2` e un
envelope fisso di 44 byte. `publisher_instance` impedisce che il riavvio della
blackbox, con sequenza nuovamente bassa, venga interpretato come replay di un
pacchetto vecchio. Deve cambiare a ogni avvio e restare stabile per tutta
l'esecuzione.

## Flight display view

Il PFD riceve valori già convertiti nell'unità visualizzata con:

- valore;
- validità;
- source id;
- age;
- failure flag esplicito.

Campi minimi: roll, pitch, heading, IAS, altitude, VSI, ground speed, baro,
flight mode, selected altitude, selected course e health.

Il payload snapshot implementato contiene lo stato health e una lista di record
campione. Ogni record trasporta `SignalId`, valore `f64`, `SourceId`, validità,
qualità, unità, età in millisecondi e flag simulazione. I booleani sono codificati
come `0.0`/`1.0` con unità `boolean`; non sono dedotti dal nome del segnale.

La guida di volo include stato FD, roll/pitch comandati e valori selezionati. Gli
allarmi sono ordinati dal core per severità e tempo; il renderer non ne cambia la
priorità e non genera autonomamente condizioni avioniche.

## Regole

1. Un consumer rifiuta schema maggiore non supportato.
2. Sequenze duplicate o arretrate vengono registrate e scartate.
3. Timeout locale porta il campo a failure; l'ultimo valore non resta verde.
4. `simulated` e `degraded` restano visibili.
5. Il renderer non arbitra le sorgenti.
6. La serializzazione binaria è definita dalla ADR-008 e implementata in
   `efis::protocol::ddp`; il trasporto resta separato.
7. Payload oltre 64 KiB vengono rifiutati prima dell'allocazione.
8. CRC errato, lunghezza incoerente, schema sconosciuto e payload malformato
   causano il rifiuto dell'intero messaggio.
9. Duplicati e sequenze arretrate sono scartati per coppia
   `source_node/publisher_instance`.
10. Nodo zero, istanza publisher zero ed enum sconosciuti sono rifiutati: non
    vengono reinterpretati come valori predefiniti.
11. Il publisher possiede la sequenza wire: non riutilizza quella eventualmente
    presente nello snapshot del dominio.
12. Il consumer può essere vincolato al `source_node` della blackbox installata;
    un nodo differente viene rifiutato e contato.
13. `LIVE`, `STALE` e `LOST` derivano dal tempo di ricezione locale. Il tempo
    monotono del publisher non viene confrontato direttamente con quello del
    display, perché appartengono a clock diversi.
14. Il payload snapshot V2 contiene esattamente i 21 campioni numerici di volo
    e i due booleani. Segnali duplicati, booleani diversi da `0`/`1` o con unità
    errata causano il rifiuto dell'intero datagramma.
15. Il trasporto iniziale è UDP unicast IPv4, secondo ADR-009. La porta PFD
    predefinita di installazione è `49002`; indirizzi e identità sono
    configurazione di deployment, non costanti del codec.
16. La sorgente UDP del display è esclusivamente ricevente. Al timeout marca i
    campioni `stale` e poi `unavailable`; non mantiene verde l'ultimo valore.

## Evidenza corrente

- round-trip di tutti i campioni di volo, inclusi flag e unità;
- vettore standard CRC32C Castagnoli `123456789`;
- rifiuto di corruzione payload e schema futuro;
- rifiuto duplicati e sequenze arretrate;
- ripartenza sequenza ammessa soltanto con nuova istanza publisher.
- publisher con sequenza propria e ricostruzione dello snapshot lato consumer;
- conservazione di source, unità, qualità, validità, age e marcatore SIM;
- timeout deterministici `WAITING/LIVE/STALE/LOST`;
- metriche per pacchetti accettati, CRC, identità, duplicati, riordino, gap e
  reboot del publisher;
- ultimo snapshot valido non sostituito da datagrammi corrotti o fuori ordine.
- trasferimento socket UDP loopback reale tra publisher e `FlightSource` PFD;
- invalidazione `LIVE → STALE → LOST` e rifiuto di un publisher con nodo diverso;
- build del PFD con receiver configurabile e metriche nel log `render_health`.
- processo Blackbox non simulato che collega la pipeline normalizzata al
  publisher DDP, con ingressi MAVLink UDP e SocketCAN monitor-only;
- test end-to-end MAVLink V2 → pipeline → UDP DDP → consumer display, con
  conservazione della provenienza Pixhawk.
- fan-out unicast verso più display con identità coerente, sequenze per endpoint
  e metriche individuali; la scomparsa di un consumer non ferma gli altri né il
  recorder locale.

Queste sono verifiche software e loopback locale. Mancano ancora installazione
su Blackbox/RPi5, ingresso dal Pixhawk e dal CAN fisici, perdita/riordino
pacchetti su Ethernet reale e replay da registrazione.
