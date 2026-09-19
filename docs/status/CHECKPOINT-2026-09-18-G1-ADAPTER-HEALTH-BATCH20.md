# Checkpoint — G1 adapter health, venti punti — 2026-09-18

## Implementato

- Macchina a stati C++20 indipendente da bus e UI.
- Configurazione esplicita REAL/SIM e identità canale/calibrazione.
- Sequenze, gap, tempo monotono, freshness e contatori diagnostici.
- Cinque fault sensore/adapter più regressione clock e restart.
- Recovery con dwell configurabile e mapping fail-closed a `Validity`.
- Target CTest dedicato.

## Verificato solo software

- MSVC Debug: build PASS.
- Test adapter: 20/20 PASS.
- Regressione `efis_engine_tests`: PASS.
- Regressione `efis_core_tests`: PASS.
- Suite core completa: 21/21 target CTest PASS.

## Verificato su hardware reale

- Nulla. Nessun RPi4 o modulo Sequent interrogato.

## Non implementato / aperto

- driver o CLI Sequent;
- channel map elettrica e calibrazioni;
- soglie temporali qualificate sul banco;
- `SourceId` ASSO5 nell'ICD;
- adapter che converte raw→engineering unit;
- recorder locale e publisher verso Blackbox;
- servizio systemd e soak di un'ora.

`AVX-G1-03` è `IN_PROGRESS` soltanto per questo kernel software. Non è un
adapter reale e non abilita `AVX-G2-02`.
