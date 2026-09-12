# ADR-013 — Assegnazione delle piattaforme RPi4/RPi5

- Stato: accettata
- Data: 2026-09-12
- Riferimenti: ADR-002, ADR-003, ADR-006, ADR-010, ADR-011

## Decisione

EFIS Next usa una sola codebase, ma non assegna liberamente le applicazioni ai
nodi. La piattaforma standard è:

- **EMS dedicato:** Raspberry Pi 4;
- **FPV / VIDEO-CORE:** un secondo Raspberry Pi 4 fisicamente distinto;
- **PFD, MFD/Canvas, navigazione, synthetic vision, setup e applicazioni di
  visualizzazione generali:** Raspberry Pi 5;
- **Blackbox/broker/recorder:** Raspberry Pi 5;
- Pixhawk, ESP32-S3, camere e computer di manutenzione restano classi hardware
  esterne e non sono trasformati artificialmente in applicazioni Raspberry Pi.

Il RPi4 FPV esegue acquisizione, decodifica, mosaico, selezione e controllo dei
quattro stream. I display RPi5 possono mostrare il flusso selezionato e i suoi
stati di salute, ma non diventano il decoder primario. Analogamente, il PFD/MFD
può mostrare una banda EMS compatta, mentre misura, logica di pagina motore e
fallback del nodo dedicato restano sull'RPi4 EMS.

## Separazione obbligatoria EMS/FPV

EMS e VIDEO-CORE non possono condividere lo stesso RPi4. Video e motore hanno
carichi, reti e failure mode differenti. Un blocco decoder, uno stream corrotto
o la saturazione del nodo video non deve togliere CPU, memoria, DRM o
alimentazione alla presentazione motore.

La rete VIDEO resta fisicamente separata da FLIGHT-DATA. Il VIDEO-CORE può avere
una seconda interfaccia verso il dominio display, senza fare bridge, NAT o
forwarding IP fra le reti.

## Regola di codebase

La codebase e i componenti visuali rimangono condivisi. Il vincolo riguarda il
profilo di build e deployment, non autorizza copie divergenti dei file QML. Le
funzioni comuni di setup devono essere compilate sia nel renderer RPi5 sia nel
nodo EMS RPi4 quando sono pertinenti al motore.

Ogni nodo del piano di rete dichiara `application`, `platform` e
`physical_host`. Il validatore rifiuta:

- EMS o FPV non assegnati a RPi4;
- PFD, MFD o Blackbox non assegnati a RPi5;
- EMS e FPV collocati sullo stesso host fisico;
- omissione dell'identità di piattaforma/host.

## Conseguenze operative

- Il secondo RPi5 `pi5` diventa il candidato MFD/SV, non il nodo FPV.
- Il renderer legacy sul secondo RPi5 resta il rollback finché EFIS Next non
  supera alimentazione, frame-rate, leggibilità e soak test.
- Serve acquisire un RPi4 separato per VIDEO-CORE; l'RPi4 EMS esistente non può
  essere riutilizzato anche per le quattro camere.
- Il Waveshare a cinque porte resta insufficiente per la configurazione finale;
  VIDEO richiede il proprio switch e FLIGHT-DATA deve crescere oltre il banco a
  cinque porte.

## Criterio di revisione

La scelta potrà cambiare soltanto con una nuova ADR supportata da misure reali
di latenza video, carico decoder, frame rate, temperatura, alimentazione e fault
isolation. Una semplice maggiore potenza disponibile non è sufficiente a
riunire EMS e FPV.
