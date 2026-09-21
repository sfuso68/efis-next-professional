# Checkpoint — Simulation alert provenance

Data: 2026-09-19  
Branch: `safety/simulation-alert-provenance-v1`  
Requisiti: `CORE-002`, `VER-001`

## Difetto riprodotto

`AlertEngine` riconosceva la simulazione soltanto quando
`validity == simulated`. Un campione con `origin.simulated=true` e validity
normalizzata a `valid` perdeva quindi l'annuncio `SIMULATION ACTIVE`, pur
restando simulato secondo il contratto canonico di `Sample`.

Il test è stato introdotto prima della correzione e ha fallito con:

```text
FAILED: origin.simulated must raise SIMULATION ACTIVE even when validity is valid
```

## Correzione

Il controllo locale usa ora `Sample::is_simulated()`, che unifica validity e
provenienza senza cambiare soglie, priorità o autorità degli allarmi.

## Evidenza software

```text
efis_core_tests.exe
All EFIS core tests passed
```

Compilatore: MSVC, C++20. Nessuna evidenza hardware è stata prodotta.

## Stato residuo

- verifica CI/ARM64: aperta;
- visualizzazione su PFD reale: aperta e subordinata ad autorizzazione di
  deployment;
- nessun merge o deployment eseguito da questo branch.
