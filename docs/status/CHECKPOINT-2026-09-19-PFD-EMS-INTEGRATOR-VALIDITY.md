# Checkpoint — PFD/EMS integrator validity V1

Data: 2026-09-19

Branch: `safety/pfd-integrator-validity-v1`

Requisiti: `PFD-001`, `EMS-002`

## Modifica

- Ore motore e consumo ignorano campioni RPM/fuel-flow non usabili o non
  finiti; i contributi simulati restano separati dai contatori reali.
- Il trend IAS si azzera su dato non valido e riparte senza picco dalla nuova
  baseline valida.
- La liveness del recorder usa un osservatore monotono del token pubblicato e
  non può essere falsata da correzioni all'indietro dell'orologio civile.

## Evidenza software

- Riproduzione pre-fix: FAIL su RPM `UNAVAILABLE` che alimentava le ore.
- Test mirato post-fix: PASS.
- Build completa PFD/EMS Qt 6.11.1 MSVC e QML AOT: PASS.
- Suite CTest: 22/23 PASS in due esecuzioni; il test Qt profilo termina con
  `0xc0000135` solo sotto CTest ma passa eseguito direttamente nello stesso
  ambiente. Il gate complessivo resta aperto alla CI.

## Evidenza hardware

Nessuna. Il branch non è stato distribuito e non modifica nodi reali.

## Stato

`PARTIAL`, verificato software; non `DONE` e non flight approved.
