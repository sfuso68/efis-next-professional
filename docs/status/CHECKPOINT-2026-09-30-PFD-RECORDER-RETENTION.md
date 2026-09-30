# EFIS NEXT — Checkpoint retention recorder PFD

Data: 2026-09-30

Stato: **IMPLEMENTED-SW / PFD READ-ONLY OBSERVED / NOT DEPLOYED**

## Implementato

- quota sui byte dei segmenti completati;
- riserva minima del filesystem;
- protezione di un numero minimo di segmenti recenti;
- preflight che impedisce cancellazioni se il target non è raggiungibile;
- selezione conservativa dei soli file regolari `flight-*.csv`;
- status recorder V3 con telemetria retention;
- parametri espliciti nella unità release PFD/recorder;
- test unitari, process-level e contratto release.

## Verificato solo software

- build MSVC completa: PASS;
- `efis_core_tests`: PASS;
- `efis_recorder_storage_failure_tests`: PASS;
- `efis_release_supervision_asset_tests`: PASS;
- rimozione oldest-first e conservazione file estranei: PASS;
- caso target impossibile senza cancellazioni: PASS;
- compatibilità lettura status V1/V2 e nuova V3: PASS.

La suite Windows ha superato gli altri test avviati. Due eseguibili non
correlati (`efis_bench_tests`, `efis_can_source_tests`) sono stati bloccati da
Application Control locale; `efis_operational_profile_model_tests`, già non
pertinente alla modifica, non ha terminato nella finestra e il run complessivo
è stato interrotto. Questi esiti non vengono dichiarati PASS.

## Verificato su hardware reale

Solo audit read-only PFD:

- recorder `active/running`, PID `969557`, `NRestarts=0`;
- directory attiva: 9.120 file e 32 GiB;
- root: 84%, 9,2 GiB disponibili;
- segmento completato nominale: circa 3,65 MB ogni minuto;
- serverha: 146 GiB disponibili.

Nessun binario, unità o record PFD è stato modificato in questa fase.

## Aperto

- autorizzazione esplicita alla copia dei 32 GiB PFD verso serverha;
- verifica checksum della copia;
- build ARM64, release immutabile e rollback;
- prima potatura controllata;
- reboot acceptance e soak >=24 h;
- definizione formale del requisito di conservazione operativo.

Decisione: **NO-GO DEPLOYMENT finché copia e verifica non sono autorizzate e
completate**. Il prodotto resta DEVELOPMENT/NON-CERT e `flight_approval=false`.
