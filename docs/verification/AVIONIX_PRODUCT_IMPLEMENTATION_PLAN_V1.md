# AVIONIX NEXT — Product implementation plan V1

Data: 2026-09-29

## Obiettivo

Congelare un piano di implementazione verificabile dopo una decisione CCB
positiva, senza trasformare il piano in autorizzazione a modificare software,
hardware, nodi installati o configurazioni operative.

## Cinque controlli

1. **Template per dominio:** motori, SKU/release, FPV, voce/cabina e HIL hanno
   un piano `NOT_RUN` vincolato al relativo template CCB.
2. **Baseline e scope:** branch, commit, requisiti, artefatti, nodi e attività
   escluse sono obbligatori e validati fail-closed.
3. **Esecuzione controllata:** ogni work package dichiara artefatti, verifica,
   evidenza ed exit criteria; rischi, rollback e deployment restano separati.
4. **Review indipendente:** tre reviewer devono essere distinti fra loro e da
   tutte le persone coinvolte nelle fasi precedenti.
5. **Record immutabile:** il piano viene congelato tramite SHA-256 e termina
   `NO_GO_IMPLEMENTATION_AUTHORIZATION_NOT_CREATED`.

## Comandi

```text
python tools/product/manage_product_implementation_plan.py validate-templates

python tools/product/manage_product_implementation_plan.py init \
  --session-dir <directory>

python tools/product/manage_product_implementation_plan.py validate \
  --session-dir <directory>

python tools/product/manage_product_implementation_plan.py finalize \
  --session-dir <directory>

python tools/product/manage_product_implementation_plan.py verify-record \
  --session-dir <directory>
```

`init` accetta soltanto una decisione CCB integra
`APPROVE_IMPLEMENTATION_PLANNING`. Il record finale richiede una successiva
review di autorizzazione e non consente implementazione, deployment o volo.

## Evidenza di verifica

I test sintetici coprono cinque domini, baseline, scope, path traversal,
pacchetti incompleti, rollback, firme, indipendenza, claim operativi,
immutabilità e tamper. Nessun hardware è stato contattato.
