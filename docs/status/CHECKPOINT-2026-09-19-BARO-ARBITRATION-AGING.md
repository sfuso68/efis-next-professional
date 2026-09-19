# CHECKPOINT — Catena barometrica, arbitraggio e aging V1

Data: 2026-09-19

Branch: `safety/baro-arbitration-aging-v1`

Baseline: `eae4d5b31f4548cb5affdf07970f42db01ce7a31`

## Scopo

Chiudere il secondo lotto safety dell'audit indipendente: evitare che QNH e
quota indicata vengano persi nell'arbitraggio o ricostruiti con una standard
implicita presentata come valida.

## Implementato

- Aggiunti `baro_setting` e `indicated_altitude` ai segnali arbitrati.
- Il provider di simulazione pubblica entrambi i campioni con marcatura
  `simulated` esplicita.
- La standard automatica 1013,25 hPa è `degraded/poor`, non una selezione
  valida del pilota.
- QNH esplicito scaduto conserva valore e provenienza e non ricade sulla
  standard.
- QNH fuori intervallo è `out_of_range`, invalida la quota indicata e viene
  contato nella salute del sistema.
- La quota indicata trasportata via DDP resta disponibile quando la quota di
  pressione locale manca; quando pressione e QNH sono utilizzabili, prevale il
  calcolo locale riproducibile.
- Aging applicato a QNH e quota indicata.

## Evidenza software

- Build core MSVC: PASS.
- Test mirati core/safety/arbitration: 3/3 PASS.
- Suite core completa: 47/47 CTest PASS.
- Build Qt/QML completa `build-hmi`: PASS (42 step incrementali), inclusi
  `efis-pfd-qt`, `efis-instruments-qt` ed `efis-ems-qt`.

## Evidenza hardware

Nessuna. Il branch non è stato distribuito su PFD, MFD, EMS o Blackbox.

## Resta aperto

- Review e CI del branch.
- Inserimento QNH e propagazione end-to-end su banco.
- Perdita, aging e recupero della sorgente barometrica reale.
- Verifica visuale di valore fuori scala e coerenza quota Pixhawk/PFD.
- Classificazione completa `NO DATA` per tutti i campi mai pubblicati.

Stato: `VERIFIED-SW / NOT-VERIFIED-HW / NON-CERT`.
