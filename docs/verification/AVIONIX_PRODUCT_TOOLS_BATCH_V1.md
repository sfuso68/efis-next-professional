# AVIONIX NEXT — Product readiness tools batch V1

**Stato:** `IMPLEMENTED_SW_NOT_RUN`

Questo batch rende operativi, come registri macchina e non come deployment, i
cinque contratti della fase precedente:

1. registry fonti motore Rotax 912 UL/ULS;
2. manifest capability Standard/Professional;
3. piano capacità FPV a quattro feed;
4. registry pacchetti fraseologia e asset voce naturale;
5. catalogo scenari HIL deterministici.

Il validatore `tools/product/validate_product_tools_batch.py` incrocia i cinque
registri con il contratto prodotto, la configurazione video e la configurazione
voce già esistenti. Fallisce se un registro non revisionato diventa attivabile,
se viene dichiarata una misura mai eseguita o se compare un percorso verso
produzione, radio, Pixhawk o attuatori.

## Verifica

```text
python tools/product/validate_product_tools_batch.py
python -m unittest tests.test_product_tools_batch -v
```

Il risultato atteso resta `execution_status=NOT_RUN`, con due motori senza
fonti applicabili, due SKU non approvate, quattro feed non misurati, zero
pacchetti fraseologia approvati e sei scenari HIL senza input digest.

## Condizioni per avanzare

- fonti motore applicabili revisionate e citate a pagina;
- capability e manifesti per nodo approvati;
- hardware VIDEO identificato e misure ripetibili;
- fonti fraseologia e asset audio revisionati;
- fixture HIL isolata e input con SHA-256 congelati.

Non sono stati contattati nodi e non è stata prodotta evidenza hardware.
