# CHECKPOINT — integrita' scrittura recorder V1

- Data: 2026-09-19
- Branch: `safety/recorder-write-integrity-v1`
- Baseline: `80791a4e72430dd7e9e14491fcb428e8716b750c`
- Requisiti: `BBX-001`, `BBX-003`
- Ambito: software; nessun deployment o intervento hardware

## Venti passi eseguiti

1. Letti stato, matrice e rilievo indipendente §5.
2. Creato branch dedicato dalla baseline `main` successiva a PR #13.
3. Aggiunto accesso di test controllato allo stream privato.
4. Aggiunto test che forza realmente `std::ios::badbit`.
5. Eseguito il test sul codice originale e registrato il fallimento atteso.
6. Introdotto stato health persistente in memoria nel recorder.
7. Verificata la scrittura dell'header iniziale.
8. Verificata ogni scrittura snapshot.
9. Verificata ogni scrittura evento nodo.
10. Verificata ogni scrittura cambio sorgente.
11. Verificato ogni flush periodico.
12. Verificati flush e close finali.
13. Impedito l'avanzamento del conteggio su append fallito.
14. Impediti append successivi dopo un errore storage.
15. Impedita la rinomina `.partial` → `.csv` dopo qualsiasi errore.
16. Trattato come errore anche il fallimento della rinomina finale.
17. Esteso `RecorderStatus` a V2 con diagnostica, mantenendo compatibilita' V1.
18. Pubblicato `ERROR` da `efis-recorder` con exit non-zero e log diagnostico.
19. Propagato `write_failed` a PFD/EMS e stato/conteggio ai log broker.
20. Aggiornati stato e matrice senza promuovere `BBX-001` o `BBX-003` a DONE.

## Evidenza software

- Pre-fix: test FAIL con mancata segnalazione dell'append fallito.
- Post-fix mirato: `efis_core_tests` PASS.
- Test processo recorder: PASS con exit non-zero e status `ERROR` persistito.
- Build Qt/QML completa: PASS.
- CTest: 47 test PASS; una copia di `efis_safety_tests` non avviata per Windows
  Application Control. La stessa suite eseguita da `build-hmi` e' PASS.
- Fault coperti: stream `badbit`, conteggio, `.partial`, falso `.csv`, apertura
  nuovo segmento fallita e lettura status legacy V1.

## Evidenza hardware

Nessuna. ENOSPC, inode exhaustion, errore I/O, power-loss e resa `ERROR` sui
display reali restano da verificare su filesystem isolato e banco autorizzato.
