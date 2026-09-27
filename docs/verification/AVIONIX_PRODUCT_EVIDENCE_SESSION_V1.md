# AVIONIX NEXT — Product evidence session V1

Data: 2026-09-27

## Scopo

Il session manager riduce gli errori manuali durante acquisizione, packaging e
review delle evidenze prodotto. Non acquisisce sensori, non comanda hardware e
non modifica i gate canonici.

## Cinque passi guidati

1. **Inizializzazione**

   ```text
   python tools/product/manage_product_evidence_session.py init \
     --domain engine --session-dir <directory>
   ```

   Sono creati `results.json` e `artifacts/`. Una directory esistente viene
   sempre rifiutata.

2. **Controllo completezza**

   ```text
   python tools/product/manage_product_evidence_session.py status \
     --session-dir <directory>
   ```

   Il report elenca artefatti mancanti o non sicuri senza promuovere lo stato.

3. **Finalizzazione evidenza**

   Dopo la conferma esplicita dell'operatore, `finalize-evidence` produce una
   sola volta `evidence-bundle.json`, validato byte-per-byte.

4. **Apertura review**

   `init-review` genera `independent-review.json` nello stato
   `PREPARED_NOT_RUN`, già legato al digest del bundle ma privo di reviewer o
   finding.

5. **Finalizzazione review**

   `finalize-review` accetta soltanto una review completa e crea una sola volta
   `review-outcome.json`. `verify-review-outcome` ne verifica successivamente
   digest e coerenza.

## Stati e autorità

La catena è:

```text
NOT_RUN → CAPTURING → READY_FOR_PACKAGING
→ PACKAGED_PENDING_REVIEW → PREPARED_NOT_RUN
→ REVIEWED_PENDING_GATE_DISPOSITION
```

L'ultimo stato non autorizza il gate. Anche con tutti i finding positivi la
decisione rimane `NO_GO_PENDING_GATE_DISPOSITION`; hardware, deployment e volo
restano falsi.

## Verifica software

I test percorrono tutti i cinque domini, sessioni incomplete, artefatti non
sicuri, doppia inizializzazione, doppia finalizzazione, review positiva e
negativa, alterazione del record e verifica successiva. Nessun nodo reale è
stato contattato.
