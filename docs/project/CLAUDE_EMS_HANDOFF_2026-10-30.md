# Claude Code — primo incarico EMS parallelo, senza conflitti

Stato: PREPARATO, NON AVVIATO. Repository privato autorevole:
`sfuso68/efis-next-professional`. Baseline di ingresso del piano:
`9a0171ea51b5e02602179a15923fbd72e56d2e58` (PR #59 draft impilata).
Il commit che aggiunge questo documento è il checkpoint R1-30; registrare
il proprio HEAD esatto prima di iniziare. Non assumere che main includa #59.

## Preparazione e letture obbligatorie

Usare un checkout separato e pulito, branch proposto `work/ems-gap-review-r130`.
Non usare né ripulire il checkout Codex o copie operative sui nodi.
Leggere integralmente `docs/status/README.md`, `EFIS_NEXT_STATE.md`,
`EFIS_NEXT_MASTER_MATRIX.md`, `CLAUDE_CODE_HANDOFF_2026-08-21.md`,
`CHECKPOINT-2026-08-21.md`, le ADR pertinenti (almeno 004,006,011,012,013,017)
e `docs/project/PARALLEL_EXECUTION_PLAN_2026-10-30.md`.
Gli snapshot storici non attestano release oggi attive.

## Primo incarico eseguibile senza banco

Audit indipendente EMS per entrambi i motori Rotax 912 UL e ULS. Inventariare
file/API già presenti per unità, canali/validità, profili, fuel, impianto
elettrico, ore motore, log e calibrazione. Collegare ciascun gap agli ID
esistenti della matrice, senza inventare nuovi stati o percentuali.
Produrre `docs/reviews/CLAUDE_EMS_GAP_REVIEW_R130.md` e, se utili, nuovi test
isolati sotto `tests/claude_ems_review/` senza modificarne altri.

Per ciascun finding: file/riga, comportamento atteso, evidenza riproducibile,
priorità, test e file minimi da cambiare. Verificare almeno null/stale/lost,
unità alternate, cambio profilo, limiti non validati, overflow etichette e
consistenza tra EMS esteso e strumenti PFD/Canvas. Una cattura sintetica
non sostituisce acceptance sul display reale.

## Confini di ownership e sicurezza

Nel primo incarico non modificare codice runtime, schemi/protocolli,
`tools/simulation/`, CMake/CI, registri canonici o output generati.
Non introdurre limiti motore approvati da dati simili/simulati; mantenere
provenienza e UNVERIFIED. Nessun accesso a Pixhawk/CAN/I2C, credenziali,
hardware, servizi, release o alimentazione. Non fare deploy, merge, reset,
rebase, force push o pulizia di archivi. Niente asset proprietari MSFS.

Scrivere il report e i test nel solo branch assegnato, con commit esplicito
dei file propri; consegnare SHA e diff all'integratore. Il report include
una proposta testuale di aggiornamento stato/matrice che applicherà Codex,
non una modifica concorrente ai registri. Il lavoro di implementazione EMS
successivo richiede assegnazione di file runtime esclusivi dopo la review.

## Criterio di consegna

Report completo, test riproducibili con exit code e dipendenze, nessuna
modifica fuori ownership. Separare PASS/FAIL/NOT_RUN e software/hardware.
Segnalare immediatamente conflitti di baseline o contratto: non correggerli
silenziosamente. Non dichiarare il prodotto o i motori validati.
