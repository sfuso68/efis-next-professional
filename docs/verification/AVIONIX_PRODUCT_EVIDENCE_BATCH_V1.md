# AVIONIX NEXT — Product Evidence Intake Batch V1

Data: 2026-09-27  
Stato: `PREPARED-SW / NOT-RUN / NO-GO`

## Scopo

Questo batch aggiunge i cinque moduli di acquisizione che devono precedere
qualunque promozione dei registri prodotto introdotti dal batch precedente.
Non acquisisce hardware, non approva dati e non autorizza prove.

1. `engine-source-intake-v1.json`: targhetta, manuale applicabile, revisione,
   pagine dei limiti e firma di review per 912 UL e 912 ULS.
2. `sku-approval-intake-v1.json`: review capability, allocazione nodi, manifest
   release e firma per Standard e Professional.
3. `fpv-benchmark-intake-v1.json`: quattro feed e otto misure su fixture VIDEO
   isolata, senza bridge o Wi-Fi operativo.
4. `phraseology-approval-intake-v1.json`: fonte, revisione fraseologia, clip
   critiche, TTS offline e intelligibilità per italiano e inglese.
5. `hil-run-intake-v1.json`: fixture isolata, digest input e artefatti per i sei
   scenari deterministici, senza reti/storage production né attuatori.

Ogni intake è legato con SHA-256 al registro che governa. Il validatore fallisce
in caso di drift, evidenza prematura, approvazione anticipata o indebolimento di
un confine safety.

## Verifica software

```text
python tools/product/validate_product_evidence_batch.py
python -m unittest tests.test_product_evidence_batch -v
```

L'esito `PASS` significa soltanto che i cinque template `NOT_RUN` sono coerenti
e fail-closed. Non equivale a verifica hardware, approvazione operativa,
autorizzazione di esecuzione o approvazione al volo.
