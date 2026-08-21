# ADR-005: source arbitration

- Stato: accettata per il core iniziale
- Data: 2026-08-17

## Decisione

Ogni grandezza critica usa un arbitrator tipizzato con candidati ordinati per
priorità. La perdita della sorgente attiva causa failover immediato verso la
prima sorgente utilizzabile. Il ritorno a una sorgente di priorità superiore
richiede un dwell time configurato.

Ogni selezione iniziale, failover e reversione genera un `SourceChangeEvent` con
sorgente precedente, nuova sorgente, ragione e timestamp monotono. Il renderer
non effettua arbitraggio.

## Limiti correnti

Soglie di disagreement e hysteresis numerica saranno definite per ogni grandezza
dopo l'acquisizione dei log reali Pixhawk/DLVR. Il recorder V1 è intenzionalmente
semplice e deterministico; non sostituisce ancora la black box resistente a
perdita di alimentazione.
