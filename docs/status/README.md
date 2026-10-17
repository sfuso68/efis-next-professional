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

Lo storico esteso è conservato in `archive/` e non deve essere usato per
dedurre quale release sia attiva oggi.

## 1a. `EFIS_NEXT_ACTIVE_RELEASES.md`

**Fonte canonica delle release osservate sui nodi.**

I checkpoint possono descrivere una sequenza di deployment, ma soltanto questo
registro distingue ultima release osservata, release attiva riconfermata e
stato da verificare.

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

Ogni branch/commit/PR che modifica una funzione censita deve aggiornare nello
stesso lavoro la relativa riga della Master Matrix. Se lo stato non cambia, la
riga deve comunque registrare l'evidenza o motivare esplicitamente la mancata
variazione; il solo aggiornamento di `EFIS_NEXT_STATE.md` non soddisfa il gate.

Una voce non può passare a `DONE` senza evidenza verificabile.

## Regola per Claude Code e Codex

All'inizio di ogni attività:
1. leggere `EFIS_NEXT_STATE.md`;
2. leggere `EFIS_NEXT_MASTER_MATRIX.md`;
3. dichiarare gli ID coinvolti;
4. lavorare su branch dedicato;
5. eseguire test e verifiche;
6. aggiornare gli stessi ID prima del push.

## Automazione attiva

La CI e `tools/project_status/generate_status.py`:
- calcolare conteggi e avanzamento dalla matrice;
- mostrare gate bloccanti;
- fallire una PR quando cambia un sottosistema senza aggiornamento della sua
  riga nella Master Matrix;
- generare un report Markdown/JSON senza duplicare manualmente le informazioni.

I documenti storici `CHECKPOINT-*`, handoff, ADR e specifiche restano evidenze; non sostituiscono questi due documenti di controllo.

## Programma AVIONIX NEXT

La baseline programmatica e il backlog per gate sono mantenuti in
`docs/project/`:

- `AVIONIX_NEXT_MASTER_PROJECT_BRIEF_CODEX_R0.md`;
- `AVIONIX_NEXT_DOCUMENT_INDEX.md`;
- `AVIONIX_NEXT_EXECUTION_BACKLOG.md`.

Il brief definisce obiettivi e gate, ma non sostituisce la matrice, lo stato
verificato o le ADR accettate. L'indice documentale stabilisce l'ordine di
autorità e registra i conflitti aperti.

Il quadro direzionale completo corrente, inclusi avanzamento, problemi,
attività interattive e planning operativo, è in
`docs/project/AVIONIX_NEXT_PROJECT_STATE_AND_PLAN_2026-09-22.md`.

Aggiornamento documentale del 2026-10-10:
`docs/project/FUSO_AVIONICS_NEXT_STATE_AND_PLAN_2026-10-10.md`.
Cruscotto offline read-only: `docs/status/PROJECT_DASHBOARD.html`.
Generare/verificare con `python -m tools.project_status.generate_dashboard`
e lo stesso comando con `--check`. Non è monitoraggio live dei nodi.

Serverha: viewer localhost:8766 + timer 5 minuti installati il 2026-10-10.
Accesso Windows: `ssh -i C:\Users\stefa\.ssh\codex_efis_pfd_ed25519 -N -L 8766:127.0.0.1:8766 sfuso@192.168.1.6`,
poi http://localhost:8766. Fonte attualmente mirror locale arretrato;
sincronizzazione GitHub bloccata dall'autorizzazione deploy key read-only.

Aggiornamento: deploy key autorizzata il 2026-10-10, refresh diretto di main
GitHub attivo in repository dedicato, stato `GITHUB_SYNC_OK`. In errore
conserva l'ultimo HTML ma segnala ERROR; oltre 15 minuti senza refresh segnala
monitor obsoleto. Non include lavoro non integrato in main.

Ripresa autonoma 2026-10-17: `docs/project/AUTONOMOUS_REVIEW_2026-10-17.md`.
Evidenze separate Windows/Linux e consolidamento in
`docs/status/evidence/autonomous-20261017/`. Il runner
`python -m tools.project_status.run_autonomous_checks --output output/checks.json`
non modifica hardware o stati; senza `--ctest` il gate nativo è NOT_RUN.
