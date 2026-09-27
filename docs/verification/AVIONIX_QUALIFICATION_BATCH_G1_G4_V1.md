# AVIONIX NEXT — Qualification batch G1–G4 V1

**Stato:** `PREPARED_SW_NOT_RUN`

**Decisione:** `NO_GO_PENDING_INTERACTIVE_HARDWARE_EVIDENCE`

**Uso in volo:** vietato (`flight_approval=false`)

## Scopo

Questo batch rende verificabili i cinque incrementi operativi successivi al
piano gemello PFD/MFD senza simulare risultati di banco. L'autorità macchina è
`config/qualification/avionix-qualification-batch-v1.json`; il validatore
fail-closed è `tools/qualification/validate_qualification_batch.py`.

Il batch congela 50 controlli in cinque pacchetti. Non abilita deployment,
scritture Pixhawk/ASSO5, modifiche DIP/cablaggio, fault distruttivi sullo
storage di produzione, interruzioni di alimentazione o autorità di comando.

## 1. QB-G1-ASSO5 — mapping e calibrazione

Preserva le quattro schede attese (`SM-I-018` ×1, `SM-I-011` ×2,
`SM-I-019` ×1) e i 18 segnali già registrati. Canali fisici, indirizzi e
coefficienti rimangono `UNRESOLVED`; nessun valore è dedotto dal modello della
scheda. Per chiudere il pacchetto servono inventario 4/4, strumento campione
approvato, coppie raw/riferimento, condizioni ambientali, revisione della
calibrazione, operatore/UTC/SHA-256 e rollback.

## 2. QB-G2-VERTICAL — verticale e replay

Il trasporto sorgente→Blackbox resta irrisolto e quindi blocca l'esecuzione.
Blackbox è l'unico publisher normalizzato; PFD, MFD ed EMS restano consumer
receive-only. La prova dovrà usare la stessa registrazione per tutti i
consumer, conservare il digest e dimostrare parità degli esiti sui dodici fault
del contratto G2 esistente.

## 3. QB-G3-AIRDATA — Matek ASPD-DLVR

La qualifica è soltanto passiva oppure offline: nessuna richiesta di stream,
scrittura parametri o attivazione output è autorizzata. Il banco deve misurare
zero, segno, linearità, ripetibilità, effetto temperatura, perdita/recupero e
provenance. È vietato sostituire la IAS con la ground speed. Coefficienti e
compensazione restano irrisolti fino a evidenza tracciabile.

## 4. QB-G4-BLACKBOX — endurance e recovery

Durata minima: 3600 secondi. Occorre registrare identità release, CPU, memoria,
temperatura, storage, continuità recorder, restart/recovery, digest degli
artefatti e parità replay. Storage-full è ammesso esclusivamente su fixture
isolata. Questo contratto non autorizza fault distruttivi sul volume di
produzione né power cut.

## 5. QB-G4-NAV-TAWS — comportamento fail-closed

Terrain mancante o scaduto deve produrre `TAWS UNAVAILABLE`, mai una
condizione safe. NAV mancante invalida CDI/BRG/DTK; meteo stale non entra in
decisioni safety. Provenance e freshness sono obbligatorie, la simulazione
deve essere evidente e il modulo non ha autorità di comando o attuazione.

## Verifica software

```text
python tools/qualification/validate_qualification_batch.py
python -m unittest tests.test_qualification_batch -v
```

Il risultato atteso è `PASS`, `packages=5`, `controls_total=50`,
`execution_status=NOT_RUN`, `execution_authorized=false` e
`flight_approval=false`.

## Sequenza interattiva futura

1. acquisire e revisionare le dipendenze fisiche senza cambiare configurazioni;
2. autorizzare separatamente ogni finestra di banco;
3. creare una directory evidence nuova e non sovrascrivibile;
4. eseguire un solo pacchetto alla volta e fermarsi a ogni stop condition;
5. validare digest, operatore e UTC prima di aggiornare la Master Matrix;
6. mantenere `NO_GO` se manca una sola evidenza obbligatoria.

## Evidenza corrente

Verificato solo software: struttura, riferimenti, confini di sicurezza, ordine
dei cinque pacchetti e 50 controlli fail-closed. Verificato su hardware reale:
nulla in questo incremento. Tutti i pacchetti restano `NOT_RUN`.
