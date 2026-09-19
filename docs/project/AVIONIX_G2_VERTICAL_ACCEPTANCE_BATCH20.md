# AVIONIX G2 — contratto verticale live, batch 20

Data: 2026-09-18

Stato: **PREPARED-SW / NOT-RUN / NOT-FLIGHT-APPROVED**

## Scopo

Questo batch prepara l'accettazione fail-closed del verticale
sensore→RPi4 acquisizione→Blackbox→PFD/MFD/EMS→recorder/replay. Non esegue la
prova, non distribuisce configurazioni e non autorizza scritture o comandi.

La specifica normativa è
`config/network/avionix-g2-vertical-acceptance-v1.json`; il validator è
`tools/network/validate_g2_vertical_acceptance.py`.

## Venti controlli

1. `G2-V20-01` — identità e revisione del contratto chiuse.
2. `G2-V20-02` — stato obbligatorio `PREPARED_SW_NOT_RUN`, esecuzione e flight
   approval false.
3. `G2-V20-03` — collegamento al freeze ICD `c331a9d` e ai due manifesti
   normativi.
4. `G2-V20-04` — sette stadi del verticale ordinati e completi.
5. `G2-V20-05` — trasporto RPi4 acquisizione→Blackbox dichiarato
   `UNRESOLVED`, bloccato da G1 e non eseguibile.
6. `G2-V20-06` — Blackbox unico publisher autorevole verso i display.
7. `G2-V20-07` — flusso DDP V2 Blackbox→PFD su UDP/49002.
8. `G2-V20-08` — flusso DDP V2 Blackbox→MFD su UDP/49002.
9. `G2-V20-09` — flusso EDP V1 Blackbox→EMS su UDP/49003.
10. `G2-V20-10` — UDP unicast one-way e display receive-only.
11. `G2-V20-11` — nessun percorso di comando, attuazione o scrittura Pixhawk.
12. `G2-V20-12` — provenienza SIM sempre strutturale e visibile.
13. `G2-V20-13` — big-endian, CRC32C, identità sorgente e schema fail-closed.
14. `G2-V20-14` — freshness su clock monotono consumer, STALE 250 ms e LOST
    1000 ms.
15. `G2-V20-15` — duplicati/riordino rifiutati e sessione publisher distinta.
16. `G2-V20-16` — stessa registrazione e stessi esiti nel replay di tutti i
    consumer.
17. `G2-V20-17` — osservatore passivo, senza bind né percorso di ritorno;
    smoke 60 s ed endurance 3600 s.
18. `G2-V20-18` — dodici fault obbligatori, inclusi storage pieno, regressione
    clock e restart dei nodi.
19. `G2-V20-19` — artifact, identità operatore, UTC e SHA-256 obbligatori.
20. `G2-V20-20` — NO-GO e rollback: zero restart/deploy ammessi dal solo
    contratto, baseline e procedura di ripristino obbligatorie.

## Blocco rilevato

Il piano rete contiene Blackbox→PFD/MFD/EMS, ma non congela ancora il
trasporto autorevole dall'adapter di acquisizione RPi4 alla Blackbox. Non viene
inventato un endpoint o un protocollo. Prima della prova live servono:

- `AVX-G1-01`: identità del nodo RPi4 ASSO5;
- `AVX-G1-02`: inventario/revisione/indirizzi delle Sequent;
- `AVX-G1-03`: adapter real/SIM, health e contratto verso Blackbox.

Finché queste dipendenze non sono soddisfatte, il validator restituisce
`blocking_dependency=AVX-G1-01/02/03`, `execution_authorized=false` e
`next_gate=USER_APPROVAL_AFTER_G1`.

## Verifica software

```text
python tools/network/validate_g2_vertical_acceptance.py
python -m unittest tests.test_g2_vertical_acceptance -v
```

Esito corrente: 20/20 controlli PASS e 10/10 test PASS. Questa evidenza
qualifica soltanto il contratto; nessun nodo, servizio, pacchetto live o
hardware è stato esercitato.
