# EFIS NEXT — Project Control

Questa directory contiene i documenti ufficiali di controllo del progetto.

**Repository autorevole:** `sfuso68/efis-next-professional`. Il repository
`sfuso68/efis-evolved` resta il progetto storico separato e non è una base di
merge per EFIS NEXT.

## 1. `EFIS_NEXT_STATE.md`

**Single Source of Truth sintetica.**

Serve per sapere rapidamente:
- architettura corrente;
- stato dei nodi RPi4, RPi5, black box, server, portatile e avionica;
- principali decisioni consolidate;
- roadmap e rischi aperti.

## 2. `EFIS_NEXT_MASTER_MATRIX.md`

**Registro operativo riga-per-riga.**

Ogni requisito/sottosistema ha:
- ID stabile;
- nodo;
- stato;
- evidenza;
- dipendenze/rischi;
- prossimo passo;
- criterio di chiusura.

Stati ammessi:
`DONE`, `PARTIAL`, `TODO`, `BLOCKED`, `VERIFY-HW`, `LEGACY`.

## Regola obbligatoria

Ogni branch/commit/PR che modifica una funzione censita deve aggiornare nello stesso lavoro la relativa riga della Master Matrix oppure dichiarare esplicitamente perché lo stato non cambia.

Una voce non può passare a `DONE` senza evidenza verificabile.

## Regola per Claude Code e Codex

All'inizio di ogni attività:
1. leggere `EFIS_NEXT_STATE.md`;
2. leggere `EFIS_NEXT_MASTER_MATRIX.md`;
3. dichiarare gli ID coinvolti;
4. lavorare su branch dedicato;
5. eseguire test e verifiche;
6. aggiornare gli stessi ID prima del push.

## Automazione prevista

Prossimo step: introdurre CI e uno script `tools/project_status/generate_status.py` per:
- calcolare conteggi e avanzamento dalla matrice;
- mostrare gate bloccanti;
- fallire una PR quando cambia un sottosistema senza aggiornamento dello stato;
- generare un report Markdown/JSON senza duplicare manualmente le informazioni.

I documenti storici `CHECKPOINT-*`, handoff, ADR e specifiche restano evidenze; non sostituiscono questi due documenti di controllo.
