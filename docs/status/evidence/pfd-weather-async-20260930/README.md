# PFD weather simulation async writer — evidence index

Data: 2026-09-30

Classificazione: **SOFTWARE PASS / HARDWARE NOT RUN**

## Problema osservato

Durante l'acceptance della release PFD retention e' stato registrato uno stall
di 10,1 secondi nel callback GUI `weather_simulation`. La precedente
implementazione generava, comprimeva in PNG e committava atomicamente il frame
radar simulato nello stesso callback del thread grafico.

## Modifica implementata

- aggiornamento di scenario, METAR/TAF simulati e segnali QML immediato;
- writer dedicato su `QThread` per compressione PNG e `QSaveFile::commit()`;
- una sola scrittura in volo, con back-pressure e coalescenza dell'ultimo
  refresh richiesto;
- pubblicazione del nuovo URL radar soltanto dopo commit riuscito;
- errore esplicito se il worker non accetta il job o la scrittura fallisce;
- telemetria distinta per callback GUI lento, worker lento e frame differito;
- directory simulazione configurabile con
  `EFIS_WEATHER_SIMULATION_DIR` per test isolati;
- ritardo diagnostico limitato a 5 secondi tramite
  `EFIS_WEATHER_SIMULATION_IO_DELAY_MS`.

## Verifica software

- configurazione CMake Qt/MSVC: PASS;
- build `efis_weather_service_tests`: PASS;
- build completa `efis-pfd-qt`: PASS;
- CTest `efis_weather_service_tests`: PASS in 0,46 s;
- il test inietta 400 ms di latenza nel writer e richiede che
  `setSimulation(true)` ritorni entro 150 ms;
- il test verifica inoltre pubblicazione asincrona del PNG, stato
  `SIMULATED`, fase crescente e distruzione ordinata del worker.

Dopo la sola formattazione dei sorgenti e il relink finale, build test e build
PFD sono nuovamente PASS; l'esecuzione del nuovo hash del test e' stata
bloccata da Windows Device Guard. Non viene quindi dichiarato un secondo PASS
post-format. Il PASS da 0,46 s riguarda la stessa logica e lo stesso test prima
della formattazione, mentre l'artefatto finale e' qualificato soltanto dalla
build locale.

## Limiti e prossimo gate

La modifica non e' stata compilata ne' distribuita su ARM64 e non e' stata
eseguita sul PFD reale. Non chiude quindi `PFD-001`, non dimostra frame-rate,
termica o assenza di stall sul pannello e non modifica la release attiva
`pfdrec-7051a47-retention-v1`. Dopo review e merge servono build ARM64,
release immutabile/rollback e una sessione reale con meteo SIM attivo e
monitoraggio degli eventi `callback_stall`.

Il prodotto resta **DEVELOPMENT/NON-CERT** con `flight_approval=false`.
