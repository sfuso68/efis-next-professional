# ADR-008 — Wire format del Display Data Protocol V2

- Stato: accettata per il codec snapshot
- Data: 2026-09-06
- Riferimenti: ADR-003, `DISPLAY_DATA_PROTOCOL_V2.md`

## Decisione

Il collegamento blackbox → display usa un formato binario deterministico in
ordine di byte di rete, con envelope fisso, payload tipizzato e CRC32C
Castagnoli. Il codec appartiene al livello protocollo e dipende dal core; non
dipende da Qt, socket, MAVLink o DroneCAN.

L'envelope include magic, schema, profilo, tipo payload, nodo publisher,
identificatore dell'istanza di avvio, sequenza, tempo monotono, lunghezza e CRC.
Il limite iniziale del payload è 64 KiB.

Il payload snapshot trasporta soltanto stato già normalizzato e arbitrato. Ogni
campione conserva segnale, valore, sorgente, validità, qualità, unità, età e
marcatore di simulazione. Il renderer non sceglie una sorgente e non trasforma
un errore in un valore plausibile.

## Perché un'identità di istanza

Una sequenza crescente identifica duplicati e riordino finché il publisher
resta acceso. Dopo un riavvio la sequenza riparte: senza un'identità di istanza,
il display scarterebbe dati nuovi finché il contatore non superasse l'ultimo
valore visto. `publisher_instance` cambia a ogni avvio e rende esplicito il
nuovo ciclo.

## Alternative escluse

- Oggetti Qt/QDataStream: legherebbero il protocollo al renderer e alla versione Qt.
- JSON sul percorso primario: leggibile ma più voluminoso e meno deterministico;
  resta adatto a diagnostica e API, non allo snapshot di volo.
- Strutture C++ copiate sul socket: padding, endian e ABI non sono un protocollo.
- CRC32 IEEE: valido ma non coerente con l'obiettivo di usare CRC32C, comune nei
  trasporti e verificabile con vettori standard.

## Limiti aperti

Il codec non decide UDP/TCP/QUIC né la cadenza. Il session layer ora assegna la
sequenza publisher, ricostruisce lo snapshot consumer, applica timeout locali e
misura perdita/riordino/duplicati/reboot senza dipendere da socket o Qt. Mancano
gli adapter di rete e i processi reali, autenticazione del canale e test su
Ethernet avionica. L'implementazione non rende `BBX-003` DONE.
