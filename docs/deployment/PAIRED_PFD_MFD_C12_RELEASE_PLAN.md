# FUSO AVIONICS NEXT — piano release gemella PFD/MFD C12

Stato: **PREPARED-SW / NOT-RUN / NO-GO**  
Ambito: `AVX-G3-01`, `AVX-G3-03`, `SV-001`, `PFD-001`, `MFD-001`  
Classificazione: DEVELOPMENT / NON-CERT / `flight_approval=false`

## 1. Obiettivo e limite

Questo incremento coordina due piani di transizione immutabile V1 già validati,
uno PFD e uno MFD, in un unico piano non eseguibile. Serve a impedire che il
benchmark Synthetic Vision C9–C12 parta con commit, policy o rollback diversi.

Il piano **non** installa, attiva, riavvia o arresta servizi; non contatta nodi
e non contiene comandi mutanti. `execution_authorized=false` e
`flight_approval=false` sono invarianti. Deployment, reboot, rollback drill e
sessione hardware richiedono un'autorizzazione separata.

## 2. Cinque controlli del blocco

1. I piani PFD e MFD devono superare indipendentemente il contratto
   `efis-next.release-transition-plan.v1`.
2. Entrambi devono dichiarare lo stesso `source_commit` completo e lo stesso
   digest della policy nodi.
3. Ruoli, hostname, piattaforma, servizi, scope e link attivi devono coincidere
   con `node-release-policy-v1.json`.
4. L'ordine è MFD prima e PFD dopo; il rollback è inverso, per mantenere il PFD
   primario disponibile il più a lungo possibile.
5. Il piano risultante lega i due input tramite SHA-256 canonico e conserva
   dodici gate e otto evidenze minime senza autorizzare l'esecuzione.

## 3. Preparazione offline

Creare prima i due piani singoli con `prepare_release_transition.py`, usando i
manifesti delle release candidate e rollback reali ma senza eseguire la
transizione. Quindi:

```sh
python3 tools/deployment/prepare_paired_display_release.py \
  --pfd-plan output/release-plan/pfd-transition.json \
  --mfd-plan output/release-plan/mfd-transition.json \
  --output output/release-plan/paired-pfd-mfd-c12.json
```

L'output non viene sovrascritto. Una differenza di commit, policy, identità,
piattaforma, ruolo, approvazione, comandi o release ID produce exit code 1 e un
record `FAIL` fail-closed.

## 4. Ordine operativo futuro

Quando una finestra hardware sarà autorizzata, la sequenza prevista è:

1. preflight read-only e validazione manifesti/rollback su entrambi i nodi;
2. attivazione e acceptance MFD, mantenendo il PFD invariato;
3. arresto immediato e rollback MFD se uno dei gate fallisce;
4. solo dopo acceptance MFD, attivazione e acceptance PFD;
5. paired reboot acceptance;
6. paired rollback drill, prima PFD e poi MFD;
7. creazione delle sessioni C10 dal commit realmente installato;
8. raccolta C11 e benchmark C9 secondo
   `HMI_SYNTHETIC_VISION_NODE_RUNBOOK.md`.

Questa sezione è una sequenza di controllo, non un'autorizzazione operativa.

## 5. Stop condition

La sessione resta `NO-GO` se manca anche una sola evidenza, se i commit
differiscono, se un manifesto/rollback non valida, se DDP non è `LIVE` con
provenance corretta, se compare throttling, se il PFD perde continuità o se il
dataset C12 differisce. Non si riducono le soglie C9 per ottenere un PASS.

## 6. Verifica disponibile

Il generatore/validatore e lo schema sono coperti da otto test fail-closed:
percorso nominale, commit e policy divergenti, autorizzazioni/comandi vietati,
identità/piattaforma/link errati, release ID duplicati, digest anti-mutation e
scrittura atomica senza overwrite.

Nessun PFD, MFD, servizio, runtime, cache, tile o rete è stato contattato o
modificato. `AVX-G3-01` resta `OPEN`; `PFD-001`, `MFD-001` e `SV-001` restano
`PARTIAL`/`VERIFY-HW` secondo la Master Matrix.
