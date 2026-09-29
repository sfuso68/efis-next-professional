# FUSO AVIONICS NEXT — checkpoint accesso restore NAS

Data: 2026-09-29  
Ambito: `PRJ-001`, `AVX-G0-02`, `AVX-G6-01`  
Esito: **VERIFIED-HOST / RESTORE-PASS**
Approvazione al volo: **no**

## Scopo

Verificare, senza modificare repository operativi o runtime avionici, se
`serverha` dispone già della credenziale separata e sola lettura richiesta dal
runbook `GIT_NAS_RESTORE_DRILL.md` per eseguire il clone isolato dal NAS.

## Evidenza osservata

- GitHub `main`, letto direttamente con `git ls-remote`, è
  `7aa45b66fc71824eb0dd4b1e6ffe898ecfcdb897`.
- Il bare repository di replica su `serverha`,
  `/home/sfuso/git/efis-next-professional.git`, espone `main` allo stesso SHA.
- `efis-git-nas-sync.timer` risulta `active`; l'ultima esecuzione osservata di
  `efis-git-nas-sync.service` riporta `Result=success` ed `ExecMainStatus=0`.
- Nella directory `/home/sfuso/.ssh/` è presente la sola identità
  `efis_nas_replica_ed25519`, configurata dal job per il push verso il NAS.
- Un `git ls-remote` batch da `serverha` verso
  `sfuso@192.168.1.22:/volume1/homes/sfuso/git/efis-next-professional.git` è
  stato rifiutato con `Permission denied`.

Il riferimento `origin/main` della working copy
`/home/sfuso/work/efis-next-professional` è stato osservato a `dbce986...` e
non è stato usato come autorità. Nessun fetch o aggiornamento è stato eseguito
su quella working copy.

## Credenziale dedicata

È stata creata su `serverha` l'identità separata
`efis_nas_restore_ed25519`, fingerprint
`SHA256:dZJ4aHFArcMEgkUvJVfWMDJAN5zrXQgcoJ4l5329Hgo`. Sul NAS la relativa chiave
pubblica è limitata al comando:

```text
git-upload-pack '/volume1/homes/sfuso/git/efis-next-professional.git'
```

Sono disabilitati agent forwarding, port forwarding, PTY, user rc e X11. Il
test shell non ha eseguito `uname` e ha prodotto esclusivamente il protocollo
Git del comando forzato. La chiave write-only di replica non è stata modificata.

## Restore reale

I-10 è stato eseguito su `serverha` con exit code `0`. Il clone isolato è
conservato in
`/home/sfuso/restore-drills/20260929T180632Z/repository`; nessuna working copy
operativa è stata sostituita. Tutti i controlli sono PASS:

- clone isolato senza hardlink;
- `git fsck --full --strict`;
- `main` ripristinato uguale allo SHA GitHub atteso;
- oggetti raggiungibili presenti (`5919` voci);
- tag richiesti presenti; il repository non dichiara tag (`tag_count=0`);
- file minimi presenti;
- Project Control `--check` PASS.

Il report controllato è conservato in
`evidence/nas-restore-drill-20260929T180632Z/evidence.json`, SHA-256
`134d08da0b1a3b30f0e236839e026344fffd53030698f593816edcb8458a1abc`.

## Classificazione

- verificato software: tool e test sintetici già registrati nel checkpoint I-10;
- verificato su host reale: credenziale confinata, isolamento shell, lettura
  NAS, clone, integrità, SHA, oggetti, file minimi e Project Control;
- non verificato: protezione fuori sito e ripristino da un secondo supporto;
- stato requisito: `PRJ-001` resta `PARTIAL` per le dipendenze governance
  residue, mentre il restore NAS→serverha è `PASS`.
