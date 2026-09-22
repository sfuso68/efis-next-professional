# AVIONIX NEXT — Synthetic Vision capture assembler V1

Stato: `VERIFIED_SW_NOT_RUN`  
Attività: `AVX-G3-01`, `SV-001`, `PFD-001`, `MFD-001`  
Prodotto: DEVELOPMENT / NON-CERT / `flight_approval=false`

## Scopo

Il blocco C11 chiude il passaggio manuale fra gli artefatti reali di una run e
il `results.json` C9. Il comando
`tools/terrain/assemble_synthetic_vision_run.py` legge sei artefatti nella
directory PFD o MFD preparata da C10, calcola le metriche riproducibili,
controlla immagini e firma e sostituisce atomicamente il solo template
`NOT_RUN`.

Non avvia il renderer, non cattura immagini, non altera cache o tile, non
inietta fault e non contatta i nodi. I file usati dai test sono sintetici e
non costituiscono evidenza hardware.

## I dieci controlli operativi

1. `frame-timing.csv` ha una riga per ogni secondo da zero, senza intervalli;
2. FPS minimo, massimo p95 e frame massimo sono derivati dal log completo;
3. `thermal.csv` copre la stessa durata e registra temperatura e throttling;
4. ogni campione di throttling deve essere esattamente `0x0`;
5. `tile-transition.json` prova bordo, continuità, campioni e proiezione;
6. `missing-tile-fault.json` prova fallback esplicito e recovery;
7. `checkpoint-screenshots.json` elenca DAY/NIGHT per cinque checkpoint;
8. ogni screenshot è confinato, regolare, non vuoto, non symlink e verificato
   con SHA-256; manifesto e dieci immagini sono congelati in un archivio il
   cui digest protegge l'intero insieme;
9. `operator-signoff.json` lega nome, UTC, giudizio visivo e conferma esatta;
10. il risultato viene accettato solo dal validatore C9 e non è
    sovrascrivibile.

I nomi e gli header dei file sono chiusi dal codice e dai test. Le serie
frame/termica devono coprire almeno 3600 secondi. Per il frame timing il p95
globale usato dal gate è il massimo dei p95 per-secondo, scelta conservativa.

## Uso durante una sessione hardware autorizzata

Dopo `manage_synthetic_vision_session.py init`, depositare i file reali in
`<bundle>/pfd/artifacts/` o `<bundle>/mfd/artifacts/`, quindi eseguire:

```text
python3 tools/terrain/assemble_synthetic_vision_run.py \
  --bundle <bundle> \
  --role PFD \
  --release-id <commit-completo-installato> \
  --operator "Nome Cognome" \
  --confirmation I_CONFIRM_REAL_SYNTHETIC_VISION_OBSERVATION
```

Ripetere per `MFD`. Solo dopo due assembly PASS è consentito usare il comando
`finalize` C10. Un risultato C11 PASS dimostra la conformità degli artefatti
al contratto C9; non concede approvazione al volo.

## Condizioni fail-closed

Header aperti, gap temporali, durata insufficiente, numeri non finiti,
throttling, soglie C9 non rispettate, file mancanti, traversal, symlink,
digest errati, checkpoint fuori ordine, firma divergente, release abbreviata,
commit di sessione alterato o risultato già eseguito impediscono l'assembly.

## Stato reale

Assembler, controlli e regressione C9/C10 sono verificati in software. Nessun
RPi5, display o renderer è stato eseguito in C11; nessuna metrica reale è
stata acquisita. `AVX-G3-01` resta `OPEN`; `SV-001`, `PFD-001` e `MFD-001`
restano `PARTIAL`.
