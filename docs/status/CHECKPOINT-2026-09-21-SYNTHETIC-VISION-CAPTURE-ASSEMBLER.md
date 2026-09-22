# CHECKPOINT — Synthetic Vision capture assembler C11

Data: 2026-09-21  
Branch: `test/synthetic-vision-capture-assistant-v1`  
ID: `AVX-G3-01`, `SV-001`, `PFD-001`, `MFD-001`  
Classificazione: implementazione e verifica software; hardware non eseguito

## Implementato — dieci punti

1. schema chiuso del log frame-time per secondo;
2. calcolo conservativo di FPS minimo, p95 e frame massimo;
3. schema chiuso del log termico continuo;
4. controllo di temperatura numerica e throttling nullo;
5. import della transizione `N45E012_TO_N46E012`;
6. import del fault missing-tile e del recovery;
7. manifesto dei dieci screenshot DAY/NIGHT ordinati;
8. confino, anti-symlink e archivio digest-bound di manifesto più dieci
   screenshot, oltre agli SHA-256 dei sei artefatti;
9. firma UTC dell'operatore con conferma testuale esatta;
10. generazione atomica, non sovrascrivibile e validazione C9 del ruolo.

## Verificato software

- 17 test C9+C10+C11 PASS;
- risultato sintetico completo derivato e riletto dal validatore C9;
- metriche calcolate dai log, non copiate in un manifesto libero;
- release, ruolo, operatore e conferma verificati fail-closed;
- dieci mutazioni di schema, timeline, soglia, throttling, tile, fault,
  screenshot, traversal, firma e commit respinte;
- un risultato già eseguito non può essere sovrascritto;
- test C11 registrato in CTest/CI.

## Non verificato hardware

- nessun PFD o MFD contattato;
- nessun renderer, cache o fault reale eseguito;
- nessun log frame/termico reale acquisito;
- nessuno screenshot del pannello o giudizio operatore reale raccolto;
- `flight_approval=false` invariato.

## Stato

C11 è `VERIFIED_SW_NOT_RUN`. La prossima attività hardware, da autorizzare ed
eseguire separatamente, è creare il bundle dal commit realmente installato,
acquisire i sei artefatti su PFD e MFD e assemblare i due risultati senza
modificare le soglie C9. Gli ID coinvolti non cambiano stato.
