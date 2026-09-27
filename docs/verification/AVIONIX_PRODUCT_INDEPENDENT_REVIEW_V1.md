# AVIONIX NEXT — Independent product evidence review V1

Data: 2026-09-27

## Obiettivo

Chiudere il tratto fra creazione del bundle e decisione del gate senza
confondere integrità dell'evidenza, verifica hardware e autorizzazione
operativa. Il processo copre motori, SKU/release, FPV, voce/cabina e HIL.

## Quattro controlli introdotti

1. `product-evidence-bundle-v1.schema.json` definisce uno schema chiuso per il
   bundle `PACKAGED_PENDING_REVIEW`.
2. `validate_product_evidence_bundle.py` verifica sorgenti, set dei soggetti,
   path, dimensione e SHA-256 di ogni artefatto.
3. Cinque template `NOT_RUN` congelano ruoli e finding richiesti dal rispettivo
   review gate.
4. `validate_product_independent_review.py` richiede tre reviewer distinti,
   timestamp UTC, firma con digest e un esito motivato per ogni finding.

## Procedura

Il coordinatore copia il template del dominio nella directory del bundle,
compila `evidence_bundle_path` e relativo SHA-256, registra reviewer, firme e
finding, quindi imposta `READY_FOR_REVIEW_VALIDATION`. Il comando è:

```text
python tools/product/validate_product_independent_review.py \
  --review <review.json>
```

Un finding `FAIL` obbliga `NO_GO_REVIEW_FINDINGS`. Tutti i finding `PASS`
consentono esclusivamente `EVIDENCE_ACCEPTED_FOR_NEXT_GATE`.

## Confine di autorità

La review non aggiorna il gate sorgente e mantiene sempre:

- `hardware_verified=false`;
- `deployment_allowed=false`;
- `flight_approval=false`.

Una revisione positiva attesta integrità e completezza del pacchetto per il
gate successivo; non certifica il prodotto, non abilita limiti motore e non
autorizza prove o uso in volo.

## Verifica eseguita

I test sintetici coprono tutti i cinque domini, schema chiuso, digest sorgente,
tamper degli artefatti, bundle appartenente a dominio errato, firme mancanti o
alterate, reviewer duplicati, finding negativi e tentativi di acquisire
autorità operativa. Nessun hardware reale viene contattato.
