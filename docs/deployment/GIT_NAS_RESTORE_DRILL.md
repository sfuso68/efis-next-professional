# EFIS NEXT — Restore drill Git da NAS

Stato: **VERIFIED-HOST / PASS (2026-09-29)**

Ambito: `PRJ-001`, `AVX-G0-02`, `AVX-G6-01`

Approvazione al volo: **no**

## Scopo

Provare che la replica Git sul NAS sia realmente recuperabile senza sostituire
la working copy di `serverha`, senza toccare runtime avionici e senza cancellare
alcun dato. GitHub resta l'autorità condivisa; il NAS è una replica archivio.

## Prerequisiti

1. Il timer `efis-git-nas-sync.timer` ha completato l'ultima replica.
2. L'operatore legge da GitHub il commit completo corrente di `main`.
3. È disponibile una chiave di restore separata, limitata sul NAS al solo
   `git-upload-pack '/volume1/homes/sfuso/git/efis-next-professional.git'`.
   La chiave di replica esistente è limitata a `git-receive-pack` e non deve
   essere ampliata o riutilizzata per il restore.
4. La directory scelta sotto `/home/sfuso/restore-drills/` non esiste.
5. Nessun comando viene eseguito da PFD, MFD, EMS o Blackbox.

## Procedura su serverha

Sostituire `COMMIT_GITHUB` con lo SHA completo letto da GitHub e usare un
identificatore nuovo per ogni prova:

```sh
cd /home/sfuso/work/efis-next-professional
COMMIT_GITHUB=COMMIT_GITHUB
DRILL_ID=$(date -u +%Y%m%dT%H%M%SZ)

python3 tools/recovery/git_restore_drill.py \
  --expected-main "$COMMIT_GITHUB" \
  --destination "/home/sfuso/restore-drills/$DRILL_ID/repository" \
  --report "/home/sfuso/restore-drills/$DRILL_ID/evidence.json" \
  --project-check
```

Il comando deve terminare con codice `0`, stampare `"status": "PASS"` e
conservare sia la copia ripristinata sia il report. Il tool esegue:

- clone isolato con hardlink vietati;
- `git fsck --full --strict`;
- confronto di `origin/main` con lo SHA autorevole;
- fetch e inventario dei tag;
- enumerazione degli oggetti raggiungibili;
- verifica dei file minimi di progetto;
- controllo dello stato generato da Project Control.

Se una qualunque verifica fallisce, l'esito è `FAIL`. La directory viene
conservata per la diagnosi e non viene ripulita automaticamente.

## Evidenza da riportare

- SHA GitHub usato come aspettativa;
- SHA ripristinato;
- report JSON integro;
- exit code;
- hostname e timestamp serverha;
- eventuali tag obbligatori passati con `--required-tag TAG`;
- conferma che working copy e servizi esistenti non sono cambiati.

Solo dopo una prova reale PASS la Master Matrix potrà registrare il restore
come verificato. Questo documento e i test sintetici non costituiscono una
prova NAS, una verifica hardware avionica o un'autorizzazione a ritirare la
copia temporanea Blackbox.

## Ultima esecuzione controllata

Il 2026-09-29 è stata installata una credenziale separata limitata al solo
`git-upload-pack` del bare professionale; la chiave write-only di replica non è
stata modificata. I-10 è stato eseguito da `serverha` contro il NAS con SHA
GitHub atteso `7aa45b66fc71824eb0dd4b1e6ffe898ecfcdb897`.

Esito: `PASS`, exit code `0`. Il clone e il report sono conservati sotto
`/home/sfuso/restore-drills/20260929T180632Z/`; una copia del report è in
`docs/status/evidence/nas-restore-drill-20260929T180632Z/evidence.json` con
SHA-256 `134d08da0b1a3b30f0e236839e026344fffd53030698f593816edcb8458a1abc`.
La prova non costituisce autorizzazione al volo e non chiude la protezione
fuori sito.
