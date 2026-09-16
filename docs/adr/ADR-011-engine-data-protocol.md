# ADR-011 — Engine Data Protocol EDP V1

- Stato: accettata per integrazione di banco; sorgente motore reale ancora aperta
- Data: 2026-09-07
- Riferimenti: ADR-002, ADR-003, ADR-006, ADR-008, ADR-010

## Decisione

I dati motore Blackbox → EMS usano un protocollo UDP unicast distinto da DDP
V2, denominato **EDP V1**, sulla porta 49003. DDP resta limitato allo snapshot
di volo: estenderlo implicitamente avrebbe accoppiato due domini con cadenze,
validità e consumatori diversi.

EDP trasporta uno snapshot motore atomico di 18 campioni. Ogni campione conserva
valore, unità, sorgente, validità, qualità, età, marcatore di simulazione, nodo,
canale, revisione di calibrazione e valore grezzo. L'envelope contiene identità
del publisher, istanza di avvio, sequenza, tempo monotono, lunghezza e CRC32C.

L'EMS accetta solo il nodo Blackbox configurato, scarta corruzione, duplicati e
sequenze inverse e rende visibili gli stati `WAITING`, `LIVE`, `STALE` e `LOST`.
Dopo `LOST` può mostrare la simulazione di banco, ma soltanto con la dicitura
`SIM FALLBACK • LINK LOST`: un valore simulato non può presentarsi come reale.

## Stato della sorgente

Il primo publisher di banco usa `EngineSimulationProvider` sulla Blackbox e
richiede l'opzione esplicita `--engine-simulation`. Questa scelta verifica il
trasporto e l'interfaccia senza fingere che il decoder dei sensori reali sia
già collegato. Prima dell'uso operativo l'opzione deve essere rimossa e il
publisher deve ricevere lo snapshot arbitrato proveniente dal decoder CAN
motore verificato su hardware.

## Conseguenze

- EMS non riceve MAVLink o CAN grezzo e resta un display read-only.
- Un riavvio del publisher è riconosciuto tramite `publisher_instance`.
- UDP e CRC rilevano guasti accidentali, non autenticano il mittente; isolamento
  fisico/firewall restano obbligatori sul prototipo.
- La chiusura richiede fault fisico, reboot, endurance e confronto con sensori
  reali calibrati.
