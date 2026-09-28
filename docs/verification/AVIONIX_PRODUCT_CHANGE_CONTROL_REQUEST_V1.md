# AVIONIX NEXT — Product change-control request V1

Data: 2026-09-28

## Obiettivo

Registrare una richiesta di modifica completa e tracciabile dopo una
disposizione positiva, senza confondere l'intake con una decisione CCB,
l'autorizzazione all'implementazione, il deployment o l'uso in volo.

## Cinque controlli

1. **Richiesta per dominio:** motori, SKU/release, FPV, voce/cabina e HIL
   hanno un template `NOT_RUN` vincolato al relativo gate disposition.
2. **Analisi d'impatto:** safety, architettura, configurazione,
   cybersecurity, human factors, operazioni e verifica sono obbligatori,
   insieme a scope, esclusioni, rollback e piano di verifica.
3. **Separazione autorità:** tre autorità di intake devono essere distinte fra
   loro, dai reviewer dell'evidenza e dalle autorità di disposizione.
4. **Record immutabile:** richiesta e record finale sono vincolati da SHA-256;
   overwrite, path traversal, digest stale e tamper sono rifiutati.
5. **Decisione CCB separata:** il massimo esito è
   `NO_GO_IMPLEMENTATION_NOT_AUTHORIZED`, con prossima azione
   `CONVENE_SEPARATE_CCB_DECISION`.

## Comandi

```text
python tools/product/manage_product_change_control.py validate-templates

python tools/product/manage_product_change_control.py init \
  --session-dir <directory>

python tools/product/manage_product_change_control.py validate \
  --session-dir <directory>

python tools/product/manage_product_change_control.py finalize \
  --session-dir <directory>

python tools/product/manage_product_change_control.py verify-record \
  --session-dir <directory>
```

`init` accetta esclusivamente un `gate-disposition-record.json` integro con
disposizione `ACCEPT_FOR_CHANGE_CONTROL`. `finalize` congela il record, ma non
crea una decisione CCB e non assegna autorità operativa.

## Evidenza di verifica

I test sintetici coprono tutti i domini, analisi incomplete, requisiti o path
non validi, firme e digest alterati, identità duplicate o sovrapposte,
tentativi di autorizzare implementazione/deployment/volo e tamper del record.
Nessun hardware è stato contattato.
