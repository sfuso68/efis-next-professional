# Checkpoint — G1 ASSO5/Sequent, venti controlli software — 2026-09-18

## Implementato

- Contratto machine-readable V1 per la ricognizione read-only.
- Venti controlli `G1-D20-01`…`G1-D20-20` fail-closed.
- Inventario atteso conforme alla decisione D-007: 1×018, 2×011, 1×019.
- Validator CLI, test di mutazione e integrazione Project Control CI.

## Verificato solo software

- Struttura, allowlist e denylist della ricognizione.
- Divieto di promuovere identità, indirizzi, schede, DIP o trasporto senza
  osservazione reale.
- Divieto di I²C probing, scritture, installazioni, wiring e power changes.

## Verificato su hardware reale

- Nulla in questo batch: nodo 0/1, schede 0/4.

## Blocco / decisione

Serve la presenza del proprietario davanti al banco per indicare il nodo
fisico ASSO5 e fotografare le quattro schede. L'esecuzione richiede una nuova
autorizzazione esplicita; il contratto corrente resta `NOT_RUN` e `NO_GO`.

## Passo successivo dopo autorizzazione

1. preflight banco e unica alimentazione;
2. identità OS/arch/storage read-only;
3. elenco controller I²C senza address probing;
4. fotografie e lettura visuale di revisioni/DIP;
5. evidence bundle e riesame operatore;
6. solo successivamente progettazione adapter e health `AVX-G1-03`.

Nessuna configurazione, servizio, rete, scheda o cablaggio è stato modificato.
