# AVIONIX NEXT — Product CCB decision V1

Data: 2026-09-28

## Obiettivo

Registrare una decisione del Configuration Control Board successiva a una
richiesta change-control completa, mantenendo separati decisione, piano di
implementazione, modifica software, deployment e uso in volo.

## Cinque controlli

1. **Template per dominio:** motori, SKU/release, FPV, voce/cabina e HIL hanno
   un record `NOT_RUN` vincolato al template change-control applicabile.
2. **Prerequisito integro:** la sessione usa soltanto un
   `change-control-request-record.json` completo, locale e verificato tramite
   SHA-256.
3. **Quorum indipendente:** quattro membri distinti e unanimi devono essere
   diversi anche da reviewer, autorità di disposizione e autorità di intake.
4. **Decisione immutabile:** meeting, voto, firme, motivazione, condizioni e
   rischi sono congelati; overwrite e tamper sono rifiutati.
5. **Piano separato:** anche `APPROVE_IMPLEMENTATION_PLANNING` termina
   `NO_GO_IMPLEMENTATION_PLAN_NOT_CREATED`; non autorizza implementazione,
   deployment o volo.

## Comandi

```text
python tools/product/manage_product_ccb_decision.py validate-templates

python tools/product/manage_product_ccb_decision.py init \
  --session-dir <directory>

python tools/product/manage_product_ccb_decision.py validate \
  --session-dir <directory>

python tools/product/manage_product_ccb_decision.py finalize \
  --session-dir <directory>

python tools/product/manage_product_ccb_decision.py verify-record \
  --session-dir <directory>
```

## Decisioni consentite

- `APPROVE_IMPLEMENTATION_PLANNING`: consente esclusivamente di preparare un
  piano separato;
- `REJECT`: richiede la correzione della richiesta;
- `DEFER`: richiede la risoluzione delle condizioni di rinvio.

La versione V1 richiede voto unanime dei quattro membri. Nessun esito assegna
un `implementation_plan_id` o abilita l'implementazione.

## Evidenza di verifica

I test sintetici coprono i cinque domini, approve/reject/defer, quorum,
unanimità, indipendenza delle persone, firme e digest, claim operativi,
immutabilità e tamper. Nessun hardware è stato contattato.
