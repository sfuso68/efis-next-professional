# AVIONIX NEXT — Product evidence packagers V1

Data: 2026-09-27
Perimetro: `EMS-004`, `PROD-001`, `FPV-002`, `VOI-003`, `VER-002`

## Scopo

Questa fase rende ripetibile il confezionamento delle evidenze raccolte per i
cinque gate prodotto. Non esegue prove, non approva i risultati e non abilita
hardware, deployment o volo.

Sono disponibili cinque template `NOT_RUN`:

- fonti motore Rotax 912 UL e 912 ULS;
- profili Standard e Professional;
- benchmark FPV dei quattro feed;
- fraseologia e catena voce/cabina `it-IT` e `en-GB`;
- sei scenari HIL deterministici.

## Flusso controllato

1. Copiare fuori da `config/` il template del dominio interessato.
2. Acquisire gli artefatti nella stessa directory o in una sua sottodirectory.
3. Impostare `READY_FOR_PACKAGING` soltanto dopo la conferma esplicita
   dell'operatore, indicandone identità e timestamp UTC.
4. Mantenere `hardware_evidence_claimed=false`, `flight_approval=false` e
   `decision=NO_GO_PENDING_INDEPENDENT_REVIEW`.
5. Eseguire:

   ```text
   python tools/product/package_product_evidence.py \
     --results <risultati.json> --output <bundle-immutabile.json>
   ```

Il comando rifiuta output già esistenti, path assoluti o uscenti dalla
directory dei risultati, symlink, file mancanti o vuoti, set incompleti,
digest sorgente non più validi e qualunque pretesa prematura di approvazione.

## Output e limite di autorità

Il bundle contiene dimensione e SHA-256 di ogni artefatto e resta nello stato
`PACKAGED_PENDING_REVIEW`. I reviewer sono vuoti; `hardware_verified`,
`deployment_allowed` e `flight_approval` restano falsi. Il bundle non aggiorna
automaticamente il gate di revisione e non costituisce evidenza approvata.

## Verifica software

La suite `tests.test_package_product_evidence` controlla tutti i cinque domini,
immutabilità, provenienza, completezza, path traversal, file mancanti o vuoti,
digest drift, identità operatore e divieti safety. Il caso symlink viene
eseguito sulle piattaforme che ne consentono la creazione al processo di test.

## Stato hardware

Nessun nodo, motore, pannello, camera, interfono o banco HIL è stato contattato
da questa modifica. Tutti i requisiti rimangono `TODO` e `NO-GO` fino alle
sessioni interattive e alle revisioni indipendenti previste.
