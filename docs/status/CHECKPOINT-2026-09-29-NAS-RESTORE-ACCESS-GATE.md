# FUSO AVIONICS NEXT — checkpoint accesso restore NAS

Data: 2026-09-29  
Ambito: `PRJ-001`, `AVX-G0-02`, `AVX-G6-01`  
Esito: **BLOCKED-CREDENTIAL / RESTORE-NOT-RUN**  
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

## Decisione fail-closed

Il restore drill I-10 **non è stato avviato**: non è stato creato alcun clone,
report o albero sotto `/home/sfuso/restore-drills/`. La chiave write-only
esistente, limitata al flusso di replica, non è stata ampliata né riutilizzata.

Per sbloccare il gate serve una nuova credenziale NAS distinta, limitata al
solo `git-upload-pack` del bare repository professionale, installata e provata
in una finestra interattiva. Solo dopo quel passaggio si potrà eseguire il
runbook con lo SHA GitHub corrente e conservare l'evidence JSON reale.

## Classificazione

- verificato software: tool e test sintetici già registrati nel checkpoint I-10;
- verificato su host reale: identità GitHub/bare serverha, stato del timer e
  rifiuto batch del percorso NAS;
- non verificato: contenuto del mirror NAS tramite lettura, clone, `git fsck`,
  tag, oggetti, file minimi e Project Control del restore;
- stato requisito: invariato, `PRJ-001 PARTIAL` e restore `NOT-RUN`.

