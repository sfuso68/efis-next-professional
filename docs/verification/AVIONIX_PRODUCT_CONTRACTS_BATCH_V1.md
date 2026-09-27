# AVIONIX NEXT — Product contracts batch V1

**Stato:** `PREPARED_SW_NOT_RUN`
**Decisione:** `NO_GO_PENDING_SOURCES_AND_INTERACTIVE_HARDWARE_EVIDENCE`

Questo incremento traduce in un contratto macchina cinque punti successivi del
piano, senza dichiarare disponibilità di prodotto o verifica hardware:

1. autorità e provenienza dei limiti EMS per Rotax 912 UL e 912 ULS;
2. profili Standard/Professional su una sola codebase;
3. FPV a quattro feed su RPi4 dedicato e rete VIDEO isolata;
4. voce naturale offline e fraseologia solo consultiva;
5. HIL/fault injection deterministici e confinati.

Il file autorevole è
`config/product/avionix-product-contracts-batch-v1.json`; il validatore
fail-closed è `tools/product/validate_product_contracts_batch.py`.

## Confini non negoziabili

- `execution_authorized=false` e `flight_approval=false`;
- i limiti correnti restano `INDICATIVE_UNVERIFIED` e non possono produrre uno
  stato CAS normale operativo;
- Standard e Professional condividono semantica di failure e codebase;
- EMS e FPV non condividono host, VIDEO non fa bridge verso FLIGHT-DATA;
- voce solo offline/PTT/grammatica chiusa, senza trasmissione o attuazione;
- HIL senza accesso a reti/storage di produzione o uscite attuatore.

## Verifica software

```text
python tools/product/validate_product_contracts_batch.py
python -m unittest tests.test_product_contracts_batch -v
```

Il successo dei test conferma forma e confini del contratto. Non dimostra
manuali motore applicabili, hardware video/audio, prestazioni, intelligibilità,
fault fisici o idoneità al volo.

## Evidenze ancora necessarie

- targhetta, seriale e revisione/pagine del manuale per ciascun motore;
- matrice capability approvata e manifesti reali Standard/Professional;
- RPi4 VIDEO, switch e quattro camere identificati e misurati;
- interfaccia audio isolata, PTT e fonti fraseologia approvate;
- fixture HIL isolata e soglie di accettazione approvate.

Tutti i cinque pacchetti terminano quindi con `NO_GO`.
