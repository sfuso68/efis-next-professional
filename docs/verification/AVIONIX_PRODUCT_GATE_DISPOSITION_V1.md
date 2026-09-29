# AVIONIX NEXT — Product gate disposition V1

Data: 2026-09-28

## Obiettivo

Registrare la decisione successiva alla review indipendente senza trasformarla
in un'autorizzazione implicita al deployment, all'hardware o al volo.

## Cinque controlli

1. **Template per dominio:** motori, SKU/release, FPV, voce/cabina e HIL hanno
   un record `NOT_RUN` legato al rispettivo review gate.
2. **Outcome vincolante:** la disposizione usa soltanto un
   `review-outcome.json` integro e dello stesso dominio.
3. **Separazione autorità:** tre autorità con identità, timestamp e firme
   distinte devono essere diverse anche dai reviewer dell'evidenza.
4. **Record immutabile:** l'esito viene congelato con digest in
   `gate-disposition-record.json`; overwrite e tamper sono rifiutati.
5. **Change control separato:** l'accettazione produce soltanto l'idoneità a
   creare una richiesta successiva. `change_control_id` deve restare nullo.

## Comandi

```text
python tools/product/manage_product_gate_disposition.py init \
  --session-dir <directory>

python tools/product/manage_product_gate_disposition.py validate \
  --session-dir <directory>

python tools/product/manage_product_gate_disposition.py finalize \
  --session-dir <directory>

python tools/product/manage_product_gate_disposition.py verify-record \
  --session-dir <directory>
```

`init` crea `gate-disposition.json` come `PREPARED_NOT_RUN`. Dopo la
compilazione da parte delle autorità, `finalize` produce il record immutabile.

## Decisioni consentite

- review positiva + disposizione `ACCEPT_FOR_CHANGE_CONTROL`:
  `ELIGIBLE_FOR_SEPARATE_CHANGE_CONTROL_REQUEST`, quindi record finale
  `NO_GO_CHANGE_CONTROL_NOT_CREATED`;
- review negativa o disposizione `REJECT`: `NO_GO_GATE_REJECTED`.

Non esiste una decisione che abiliti direttamente deployment o volo.

## Evidenza di verifica

I test sintetici coprono i cinque domini, gate e outcome alterati, firme
errate, autorità duplicate o coincidenti con reviewer, accettazione di review
negativa, tentativi di inserire un change-control ID, claim operativi,
immutabilità e tamper del record. Nessun hardware è stato contattato.
