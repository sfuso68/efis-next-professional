# Checkpoint — Health unavailable count

Data: 2026-09-19  
Branch: `safety/health-unavailable-count-v1`  
Requisito: `CORE-004`

## Riproduzione

Il test sullo snapshot senza sorgenti è stato esteso per verificare il
conteggio, non soltanto `HealthLevel::warning`:

```text
FAILED: an uninitialised flight model must count all ten required flight fields
```

Il codice precedente contava soltanto la quota indicata resa esplicita dalla
normalizzazione barometrica; gli altri campi vuoti venivano saltati perché il
loro SignalId era `unknown`.

## Correzione

Prima dell'aging vengono dichiarati i nove ingressi primari attesi; la quota
indicata derivata costituisce il decimo campo. I canali opzionali non promessi
non vengono trasformati in allarmi permanenti.

## Evidenza software

```text
efis_core_tests.exe
All EFIS core tests passed

CTest: 47/47 PASS
```

Nessuna verifica hardware, PR, merge o distribuzione è inclusa in questo
checkpoint.
