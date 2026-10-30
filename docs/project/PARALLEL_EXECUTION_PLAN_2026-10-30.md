# FUSO AVIONICS NEXT — piano operativo concentrato

Data: 2026-10-30. Autorizzazione: «PROCEDIAMO» dopo proposta sei macroblocchi
e tre filoni. Baseline di ingresso: `9a0171ea51b5e02602179a15923fbd72e56d2e58`,
branch `fix/ems-responsive-header-r108-v1`, PR #59 ancora draft e impilata.
Non è autorizzazione a merge, deploy o modifiche hardware.

## Obiettivo e limite

Completare sviluppo e prove per campagne aggregate, non moltiplicare piccoli
checkpoint. Ogni sottoprova mantiene un esito proprio: un PASS globale non
può occultare NOT_RUN, FAIL o gate hardware. Standard e Professional usano
una codebase e capacità esplicite; la completezza documentale non prova
completezza del prodotto. I 71 requisiti e le loro evidenze restano nella
Master Matrix; nessuna percentuale misura sicurezza o prontezza al volo.

## Sei macroblocchi e uscite verificabili

| Blocco | Lavoro | Campagna aggregata / uscita | Dipendenze e stato iniziale |
|---|---|---|---|
| M1 Baseline | contratti, capability matrix, ownership, inventario delle dipendenze PR | generatori, gate stato, regressioni, delta e conflitti separati | In corso; integrazione main non autorizzata da questo piano |
| M2 SIL distribuito | controllo pilota live, lifecycle, epoch, fanout isolato, impianti e scenari | comandi+revoca+pausa, fault+replay, timing+durabilità e parità multi-consumer | P03 parziale; nuovo inbox R1-30 è componente, non console |
| M3 EMS | profili UL/ULS, unità, canali, calibrazione, fuel, elettrico e ore | parametri validi/invalidi, cambio unità, stale/loss, allarmi e layout | Limiti reali e sonde richiedono manuali e banco |
| M4 NAV/SV/WX | cataloghi, georeferenziazione, offline/cache, piano di volo, restore | stessa rotta Treviso–Belluno, buchi dati, scadenza meteo, disconnessione e restore | Licenze, datum, coverage e provider da qualificare |
| M5 Funzioni accessorie | FPV RPi4 dedicato, voce/PTT, fraseologia, assistenza emergenze consultiva | carico combinato, perdita camera/audio/dati, priorità CAS e fallback visivo | Nessun comando di volo o servo abilitato; procedure da approvare |
| M6 Prodotto | packaging, aggiornamento/rollback, endurance, HIL, accettazione | install+rollback+reboot, registrazione+restore, fault e accettazione pannello | Power MFD/Blackbox, Windows policy e collaudi fisici restano gate |

M1 e le parti pure software di M2–M5 possono avanzare insieme. M6 raccoglie
evidenze, ma non promuove le prove mancanti. La seconda SSD già autorizzata
serve solo test SIL; archivio mappe e servizi di produzione non sono scratch.

## Tre filoni, un integratore

| Filone | Responsabile proposto | Ownership | Consegna |
|---|---|---|---|
| A SIL e integrazione | Codex principale | `tools/simulation/`, relativi test; unico autore contratti e registri canonici | commit piccoli coerenti, log e criteri di uscita |
| B EMS/config/HMI | Claude Code, da avviare dall'utente | primo incarico ristretto nel handoff collegato; checkout separato | patch/branch e report, senza deploy/merge |
| C NAV/WX/dati e review | agente indipendente | inizialmente sola lettura; successiva scrittura solo con file assegnati | finding con evidenza e test proposti |

Questa sessione ha usato A e un revisore read-only. Claude non è stato avviato.
Tre filoni non significa tre writer sullo stesso checkout. Per attivare B/C
in scrittura servono checkout/branch separati e una lista di file esclusivi.
Non sovrapporre build pesanti e misure timing sul server. Più agenti prima
di stabilizzare i contratti aumenterebbero i conflitti, non la capacità utile.

L'integratore aggiorna `EFIS_NEXT_STATE.md`, Master Matrix, backlog e output
generati. Gli altri consegnano proposte di aggiornamento, senza modificare
quei file condivisi. Niente add indiscriminato di output o credenziali.
Revisionare branch base delle PR impilate prima di qualsiasi futuro merge.

## Campagne di prova, meno sessioni con più controlli

1. **C-SW, autonoma:** sintassi/schema, unità, confini e replay, golden
   software, stato generato. Esiti distinti per suite e piattaforma.
2. **C-SIL, autonoma isolata:** fisica JSBSim, lifecycle e comando, audit,
   recorder/timing, cache/NAV/WX e fault, quindi restore. Prima fixture
   deterministica, poi interattività e fanout; non chiamare la fixture console.
3. **C-BENCH, interattiva aggregata:** alimentazione, touch/layout, cablaggio,
   sonde/calibrazione, camera/audio/PTT, reboot/rollback e perdita collegamenti
   nella stessa finestra; checklist distinta per nodo e sottosistema.

Ogni evidenza indica commit, config/hash, ambiente, durata, soglie, esito,
limiti e dati non provati. Rinviare l'interazione non elimina il relativo gate.
Non stimare date finali senza inventario dei residui e primi tempi di campagna.

## Prossima sequenza concreta

1. Consolidare R1-30 inbox/outcome con test di lease, revoca, contesa e retry.
2. Integrare il servizio owner durante pausa e prima del tick; progettare
   lifecycle/resume separato dai comandi con tick futuro. Epoch e audit live
   devono essere definiti prima di una console pilotabile.
3. Avviare Claude con `CLAUDE_EMS_HANDOFF_2026-10-30.md` su checkout separato.
4. Assegnare review NAV/WX/cataloghi con un elenco di gap verificabile,
   poi scegliere implementazione software non sovrapposta.
5. Eseguire C-SW/C-SIL sui commit candidati, integrare solo esiti comprovati;
   presentare una sola checklist C-BENCH quando l'utente è ai nodi.

R1-30 non completa M2: endpoint autenticato, executor live, pausa/epoch,
durabilità degli eventi interattivi e consumer simultanei restano aperti.
