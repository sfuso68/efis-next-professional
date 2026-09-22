# Checkpoint — Synthetic Vision C12 LOD/projection

Data: 2026-09-22

Branch: `fix/synthetic-vision-lod-projection-v1`

Baseline: `origin/main` `195e730cb9785002dc2e6c29057d4e315338a53b`

Requisiti: `AVX-G3-01`, `SV-001`, `PFD-001`, `MFD-001`

## Implementato

- introdotto un modello C++20 unico per proiezione locale, impronta tile,
  isteresi LOD e pre-caricamento;
- collegato il modello al renderer Qt tramite adapter read-only;
- sostituita nel Quick 3D la distanza dal centro con la distanza dall'impronta;
- preservato high-detail su entrambi i lati del confine N45/N46;
- aggiunto evento runtime con contratto e soglie;
- aggiunti test deterministici del caso che causava la degradazione.

## Verificato software

- configurazione MSVC 19.51 + Qt 6.11.1: PASS;
- build completa (220 step incrementali): PASS;
- `efis-pfd-qt` e QML AOT: PASS;
- `efis_terrain_projection_tests`: PASS;
- C9+C10+C11+C12 mirati: 4/4 PASS;
- avvio/cattura offscreen 1024×600: PASS soltanto come smoke test;
- `git diff --check`: PASS.

Il CTest completo Windows ha prodotto 54/64 PASS. Dieci suite sono state
bloccate da Smart App Control o dal caricamento DLL Qt del runner locale, già
documentati nel progetto; nessuna è una suite Synthetic Vision. Questo dato non
viene trasformato in un PASS complessivo.

## Non verificato

- nessun accesso, deployment o riavvio su PFD `.45` o MFD `.49`;
- nessuna misura FPS, p95, frame massimo, temperatura o throttling ARM64;
- nessuna ispezione diretta della texture, seam, popping o leggibilità;
- nessun fault missing-tile reale e nessuna firma operatore;
- nessuna approvazione al volo.

## Stato e prossimo gate

C12 è `VERIFIED_SW / VERIFY_HW`. `AVX-G3-01` resta `OPEN`, `SV-001` resta
`PARTIAL` e `flight_approval=false`. Dopo review/merge, il commit installato va
validato con la pipeline C10→C11→C9 su entrambi gli RPi5 senza cambiare le
soglie congelate.
