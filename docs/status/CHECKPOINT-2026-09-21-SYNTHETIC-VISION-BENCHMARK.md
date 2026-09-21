# CHECKPOINT — Synthetic Vision benchmark C9

Data: 2026-09-21  
Branch: `test/synthetic-vision-benchmark-contract-v1`  
ID: `AVX-G3-01`, `SV-001`, `PFD-001`, `MFD-001`  
Classificazione: implementazione e verifica software; hardware non eseguito

## Implementato

- contratto V1 chiuso per rotta Treviso–Belluno e due tile;
- cinque checkpoint DAY/NIGHT dalla pianura al finale LIDB;
- soglie prestazionali coerenti con il criterio G3 di almeno 40 FPS;
- controlli per p95, frame massimo, termica, throttling e bordo tile;
- gate espliciti contro geometria non finita, primo piano stirato/sfocato e
  discontinuità dell'orizzonte;
- fault missing-tile con fallback obbligatoriamente visibile;
- sei famiglie di evidence, digest SHA-256, confino percorsi e anti-symlink;
- verifica del commit completo, della release e della firma operatore;
- cross-check del contratto con lo scenario non certificato esistente;
- test registrato in CTest/CI.

## Verificato software

- baseline canonica validata;
- fixture completa accettata;
- mutazioni di stato, approvazione al volo, piattaforma, tile, checkpoint,
  durata, FPS e policy dati respinte;
- ogni soglia numerica e booleana verificata fail-closed;
- traversal, digest falso, symlink, osservazione incompleta e contratto falso
  respinti.

## Non verificato hardware

- nessun RPi5 o pannello contattato;
- nessun renderer eseguito;
- nessuna rotta simulata riprodotta;
- nessun FPS, frame-time, temperatura o throttling misurato;
- nessuna valutazione operatore acquisita.

## Stato

Il gate è `PREPARED_SW_NOT_RUN`. `AVX-G3-01` resta `OPEN`; `SV-001`,
`PFD-001` e `MFD-001` restano `PARTIAL`. Il passo successivo è usarlo per
misurare la baseline corrente, correggere proiezione/LOD sulla base delle
misure e ripetere lo stesso run senza cambiare le soglie.
