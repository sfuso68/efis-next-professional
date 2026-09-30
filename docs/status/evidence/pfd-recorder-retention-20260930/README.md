# PFD recorder retention — evidence index

Data: 2026-09-30

Classificazione: **SOFTWARE PASS / HARDWARE READ-ONLY / NOT DEPLOYED**

Evidenza software osservata nella workspace:

- build di `efis-recorder`, `efis_core_tests` e dell'albero disponibile: PASS;
- `efis_core_tests`: PASS;
- `efis_recorder_storage_failure_tests`: PASS;
- `efis_release_supervision_asset_tests`: PASS.

Evidenza PFD read-only:

- unità recorder `active/running`, `NRestarts=0`;
- percorso attivo `/home/efis/.local/state/efis-next/records`;
- 9.120 file, 32 GiB;
- root 57 GiB, 46 GiB usati, 9,2 GiB disponibili, 84%;
- gli ultimi segmenti completati misurano circa 3,65 MB e ruotano ogni minuto.

Evidenza serverha read-only:

- filesystem da 221 GiB, 146 GiB disponibili;
- archivio legacy precedente presente (11 GiB).

Una chiave temporanea vincolata per la copia serverha→PFD è stata provata, ma
la copia dei record non è stata eseguita in assenza di autorizzazione esplicita
al trasferimento del payload. La chiave è stata quindi rimossa dal PFD, da
serverha e dalla workspace; non resta una credenziale temporanea attiva.

Non esiste evidenza di potatura, build ARM64, activation, rollback o soak: tali
attività restano aperte e non devono essere inferite da questo indice.
