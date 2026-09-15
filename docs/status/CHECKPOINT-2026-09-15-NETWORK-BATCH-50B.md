# EFIS NEXT — Network acceptance batch 50B

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: DEVELOPMENT / NON-CERT / NON FLIGHT APPROVED

## Risultato dei 50 punti

1. Fissato il firewall laptop come `DEFERRED-BY-USER`.
2. Esclusa ogni installazione o attivazione privilegiata del firewall.
3. Preservati gli artefatti utente non tracciati in `output/`.
4. Verificata la branch dedicata prima delle modifiche.
5. Letti stato autorevole, handoff, checkpoint e ADR pertinenti.
6. Corretto il percorso reale AppDir dal servizio systemd.
7. Inventariati 1.081 file AppDir.
8. Inventariati 74 link simbolici AppDir.
9. Inventariate 336 directory AppDir col nuovo algoritmo.
10. Misurati 471.337.419 byte di contenuti regolari.
11. Creato algoritmo di digest deterministico dell'albero.
12. Inclusi i nomi relativi nel digest.
13. Inclusi i tipi delle entry nel digest.
14. Inclusi i bit eseguibili nel digest.
15. Inclusi i target dei link senza seguirli.
16. Inclusi tutti i contenuti regolari nel digest.
17. Rifiutate entry speciali non previste.
18. Creato report JSON atomico del digest.
19. Calcolato digest reale `34b0f677...f29271c`.
20. Misurato tempo reale del digest in circa 0,67 secondi.
21. Promosso manifest QGC allo schema V2.
22. Aggiunto percorso AppDir approvato al manifest.
23. Aggiunti digest e contatori completi al manifest.
24. Collegato il digest AppDir al preflight fail-closed.
25. Aggiunto rifiuto di hash AppDir formalmente invalido.
26. Aggiunto rifiuto di inventari negativi.
27. Aggiunto rifiuto di AppDir mancante.
28. Aggiunto rifiuto di AppRun esterna all'AppDir risolta.
29. Distribuiti digest, manifest e preflight V2 su `Port-hp`.
30. Verificato preflight V2 reale `PASS`.
31. Verificato QGC reale `active/running` e `NRestarts=0`.
32. Verificato un solo listener QGC su UDP 14550.
33. Confermato ancora lo shutdown bounded ma non graceful.
34. Verificate librerie ELF del plugin `speechd` tutte risolte.
35. Verificati `speech-dispatcher` e `libspeechd2` 0.12.0 installati.
36. Verificati socket e servizio Speech Dispatcher.
37. Enumerate con successo le voci senza riprodurre audio.
38. Ordinato Speech Dispatcher prima di QGC con `Wants`/`After`.
39. Verificata sintassi systemd dell'unità aggiornata.
40. Provato che il warning plugin persiste anche col backend attivo.
41. Esteso lo status alla sola invocazione QGC corrente.
42. Mantenuto correttamente lo status Ground Station `PARTIAL`.
43. Aggiunto gate di copertura campioni al soak.
44. Aggiunto gate di parità fan-out aggregata.
45. Aggiunti gate di parità per ogni observer e display.
46. Eseguito soak reale 600 s con copertura 97,33% e 26,247 Hz.
47. Verificati parità esatta, monotonia e zero errori su tutte le uscite.
48. Creato collector integrato read-only di risorse e telemetria.
49. Verificato collector reale `PASS` a 48,3 °C e throttling zero.
50. Aggiornati runbook, ADR-015, matrice, stato e questo checkpoint.

## Verificato solo software

- 59 test mirati `PASS`; due test POSIX sono `SKIP` sul runner Windows e gli
  stessi cinque test digest sono tutti `PASS` sul portatile Linux.
- Sintassi Python e whitespace diff sono conformi.
- Casi negativi coperti: file mutato, AppDir/hash mancanti, AppRun esterna,
  copertura/parità errate, temperatura, throttling, RAM e restart.

## Verificato su hardware reale

- AppDir completa, manifest/preflight V2, QGC, listener e backend speech sul
  portatile Debian `Port-hp`.
- Blackbox: 600 secondi, 584 campioni, +15.748 frame/observer,
  +11.977 record/display, zero errori/restart, 48,3 °C, throttling `0x0`,
  3.813.163.008 byte RAM disponibili e RSS broker 4.341.760 byte.

## Aperto — non dichiarare DONE

- firewall laptop e fault WLAN: `DEFERRED-BY-USER`;
- plugin QGC `speechd` e chiusura graceful;
- reboot/login grafico QGC e secondo reboot server;
- perdita/jitter ed endurance termica;
- alimentazione Pixhawk POWER1 definitiva e rimozione USB di banco.
