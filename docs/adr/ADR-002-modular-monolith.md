# ADR-002: monolite modulare iniziale

- Stato: accettata
- Data: 2026-08-17

## Decisione

`efis-core` nasce come processo unico composto da moduli tipizzati. Black box,
video e renderer restano processi separati per contenere guasti e dipendenze.

## Vincolo

I confini interni devono poter diventare confini di processo senza cambiare il
significato dei modelli pubblici.
