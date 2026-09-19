# Checkpoint — G1 node capture, dieci punti — 2026-09-18

## Implementato

- Acquisitore Python read-only con allowlist immutabile di dieci comandi.
- Report JSON con output grezzi, identità, risorse, rete, I²C, health e safety.
- Dieci controlli fail-closed `G1-C10-01`…`G1-C10-10`.
- Test registrato in CTest e Project Control CI.

## Verificato solo software

- 7/7 test Python PASS.
- 10/10 mutazioni dei controlli rifiutate.
- Nessun comando arbitrario, probing I²C, shell, `sudo` o scrittura ammesso.
- Suite CTest completa: 42/42 PASS.

## Verificato su hardware reale

- Esecuzione sul RPi4 `ems-sensor`: PASS 10/10.
- Report persistente di 13.728 byte sul nodo.
- SHA-256:
  `f7cb1da79623c37891e662ec966f8d64be743a8d471663970993e4eac93f5e94`.

## Aperto

- Alimentatore non documentato.
- Sequent verificate 0/4; foto e DIP mancanti.
- Nessuna scansione indirizzi, lettura vendor o calibrazione eseguita.
- `AVX-G1-01` resta `IN_PROGRESS`; `AVX-G1-02` resta `OPEN`.
