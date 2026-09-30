# EFIS NEXT — Checkpoint retention recorder PFD

Data: 2026-09-30

Stato: **ACTIVE VERIFY-HW / ROLLBACK+REBOOT PASS / SOAK OPEN**

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

- copia stabile di 9.208 file/33.777.901.735 byte su serverha;
- secondo checksum rsync a zero differenze;
- build ARM64 nativa e tre test mirati PASS;
- release immutabile `pfdrec-7051a47-retention-v1` attivata;
- prima potatura oldest-first: 2.222 segmenti/8.008.825.406 byte;
- rollback reale alla release precedente e ritorno alla candidata: PASS;
- reboot acceptance: PASS;
- seconda potatura autonoma post-reboot: 12 segmenti/43.890.672 byte;
- post-reboot PFD/recorder dai binari manifestati, status V3 crescente, DDP
  LIVE ~20 Hz, root 71%/17,4 GB liberi e throttled `0x0`.

## Aperto

- soak >=24 h con `NRestarts=0`, status V3 crescente e almeno 12 GiB liberi;
- spiegazione definitiva dello startup retry PFD post-boot;
- correzione/qualifica dello stall `weather_simulation` di 10,1 s;
- definizione formale del requisito di conservazione operativo;
- acceptance visuale/touch/terrain e 27 gap tile.

Decisione: **GO LIMITATO AL SOAK DI BANCO; NO-GO FLIGHT**. Il prodotto resta
DEVELOPMENT/NON-CERT e `flight_approval=false`.
