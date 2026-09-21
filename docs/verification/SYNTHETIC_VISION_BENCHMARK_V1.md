# AVIONIX NEXT — Synthetic Vision benchmark V1

Stato: `PREPARED_SW_NOT_RUN`  
Attività: `AVX-G3-01`, `SV-001`, `PFD-001`, `MFD-001`  
Prodotto: DEVELOPMENT / NON-CERT / `flight_approval=false`

## Scopo

Il gate rende ripetibile la valutazione della proiezione e del LOD lungo la
rotta di sviluppo Treviso–Belluno. Non corregge da solo il renderer e non
trasforma immagini sintetiche in evidenza hardware. Congela invece scenario,
checkpoint, soglie e artefatti richiesti, così un miglioramento non può essere
dichiarato sulla base di una singola cattura scelta a posteriori.

La baseline è in
`config/terrain/avionix-synthetic-vision-benchmark-v1.json`; il validatore è
`tools/terrain/validate_synthetic_vision_benchmark.py`.

## Tracciato congelato

Il test parte da LIPH e termina a LIDB, attraversando `N45E012` e `N46E012`.
Richiede cinque checkpoint ordinati:

1. decollo LIPH;
2. pianura veneta;
3. ingresso Prealpi;
4. avvicinamento LIDB;
5. finale LIDB.

Ogni checkpoint deve essere osservato in DAY e NIGHT. I dati restano marcati
`DEVELOPMENT_NON_CERT`, datum `EGM2008`, non utilizzabili per navigazione o
separazione ostacoli.

## Soglie bloccanti

- durata minima 60 minuti;
- almeno 40 FPS e p95 frame-time non superiore a 25 ms;
- frame massimo non superiore a 100 ms;
- `get_throttled=0x0`;
- attraversamento reale del confine `N45E012` → `N46E012`;
- discontinuità del bordo non superiore a 1 m e zero campioni mancanti;
- proiezione finita, primo piano leggibile e orizzonte continuo;
- fault missing-tile con fallback esplicito e zero fallback non etichettati;
- cache fredda e calda, PFD e MFD su RPi5 AArch64;
- firma dell'operatore e sei famiglie di artefatti con SHA-256.

La temperatura massima viene registrata, ma il contratto non inventa un limite
ambientale di prodotto non ancora qualificato.

## Uso

Validare la baseline software:

```text
python3 tools/terrain/validate_synthetic_vision_benchmark.py
```

Validare un run già acquisito:

```text
python3 tools/terrain/validate_synthetic_vision_benchmark.py \
  --results <sessione>/results.json \
  --artifact-root <sessione>/artifacts
```

Il secondo comando accetta soltanto percorsi relativi confinati, file regolari
non vuoti, digest coincidenti, commit sorgente completo, release nominata,
tutte le misure e tutti i checkpoint. Symlink, escape, omissioni e soglie
allentate falliscono chiusi.

## Evidenze richieste

1. `FRAME_TIMING` — serie completa FPS/frame-time;
2. `THERMAL` — temperatura e registro throttling;
3. `TILE_TRANSITION` — attraversamento e continuità del bordo;
4. `MISSING_TILE_FAULT` — stato visibile e recovery;
5. `CHECKPOINT_SCREENSHOTS` — DAY/NIGHT per i cinque checkpoint;
6. `OPERATOR_SIGNOFF` — valutazione leggibilità, deformazione e orizzonte.

## Limite di questo incremento

I test repository usano artefatti sintetici soltanto per verificare il
comportamento fail-closed del validatore. Nessun RPi5 o display è stato
contattato; non sono stati misurati frame-rate, temperatura o qualità visiva.
`AVX-G3-01` e `SV-001` restano aperti/partial fino alla correzione del renderer
e alla prova completa su PFD e MFD reali.
