# Blocco autonomo — 2026-10-12

Ambito: PRJ-001, PRJ-002, PRJ-003, VER-001. Nessuna chiusura hardware.

## Completato in questo incremento

1. Gate CI del viewer: output deterministico `--check` e test dashboard.
2. Guardie backup: conflitti tag server/NAS, preservazione branch, main errato.
3. Riserva disco backup: stop sotto 2 GiB, nessuna cancellazione automatica.
4. Poll GitHub backup limitato a main/tag: PR refs non provocano snapshot inutili.
5. Monitor backup read-only, stato/errori/stale oltre 45 minuti e data restore
   distinta da ultimo controllo; nessuna esposizione inventario completo refs.
6. Regression offline G1 discovery/inventario/readiness, G2 ICD/verticale e
   qualification: 57 test PASS. NON sono discovery o qualificazione hardware.

Test locali controllo/dashboard/backup: 16 PASS (temp directory nel workspace
per evitare i permessi del runtime sandbox Windows). Test serverha: 8 PASS.
Il workflow CI è aggiornato ma run GitHub non eseguita/verificata qui.

## Coda autonoma successiva, senza promozioni implicite

| Ordine | Lavoro | Confine / evidenza richiesta |
|---|---|---|
| A1 | Packaging installazione monitor/backup ripetibile, test HTTP e failure injection | fixture isolate; non modificare backup reali per simulare guasti |
| A2 | Restore da bundle preventivo e report confrontato | clone temporaneo, oggetti/digest, nessuna sostituzione copie correnti |
| A3 | Proposta backup branch e quota/retention/off-site | nessuna potatura automatica prima di approvazione policy |
| A4 | Review ICD/freshness e fixture multi-sorgente | SOLO SW; approvazione ERAP e bus restano separate |
| A5 | Test meteo/cache/time-skew e dataset coverage | dati fixture, nessuna credenziale/provenance inventata |
| A6 | Test profili 912 UL/ULS e unità | fonti non verificate restano UNRESOLVED, non limiti operativi |
| A7 | Fixture HMI risoluzioni/fail-state | golden software, acceptance fisica resta aperta |

Non «tutti i punti completati»: gate power, cablaggi/calibrazione, fonti
motore specifiche, fault fisici, latenza FPV/audio, HIL e approvazioni richiedono
interazione. Il lavoro autonomo prepara e prova software ma non li sostituisce.

Aggiornamento successivo: `AUTONOMOUS_REVIEW_2026-10-17.md` consolida la coda
A1–A7, le consegne verificate e i gate ancora aperti. Le righe precedenti
sono lo snapshot del 12 ottobre, non lo stato corrente di ciascuna consegna.
