# EFIS NEXT — Network hardening batch, 50 punti

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: DEVELOPMENT / NON-CERT / NON FLIGHT APPROVED

## Risultato dei 50 punti

1. Letto `EFIS_NEXT_STATE.md` prima delle modifiche.
2. Letto `EFIS_NEXT_MASTER_MATRIX.md` prima delle modifiche.
3. Letti handoff, checkpoint 2026-08-21 e ADR-015 pertinenti.
4. Verificata la branch dedicata e preservati tutti gli `output/*` utente.
5. Assunta come baseline verificata il checkpoint observer precedente.
6. Creato manifest QGC con schema versionato V1.
7. Fissata modalità `MONITOR_ONLY_NO_PIXHAWK_RETURN` nel manifest.
8. Fissata versione QGroundControl 5.0.8.
9. Fissato il percorso dell'AppImage approvata.
10. Fissato SHA-256 AppImage `06969c67...46737`.
11. Fissato e verificato il percorso AppDir/AppRun.
12. Fissato bind listener `0.0.0.0`.
13. Fissata porta listener UDP 14550.
14. Fissata sorgente ammessa Blackbox `192.168.1.47`.
15. Fissata esclusione del bridge USB legacy.
16. Esteso il preflight con parsing fail-closed del manifest.
17. Aggiunto ricalcolo SHA-256 prima di ogni avvio QGC.
18. Aggiunto rifiuto del manifest mancante o non leggibile.
19. Aggiunto rifiuto dell'AppImage modificata.
20. Aggiunto rifiuto di endpoint diverso dalla unità.
21. Aggiunto rifiuto di sorgente diversa dalla Blackbox approvata.
22. Esteso il report preflight con versione, hash, sorgente e AppRun.
23. Collegato il manifest all'`ExecCondition` systemd.
24. Distribuiti preflight, manifest e unità sul portatile reale.
25. Verificato su `Port-hp` preflight completo `PASS`, QGC attivo e zero restart.
26. Creata tabella nftables Ground Station dedicata.
27. Limitato ingresso UDP 14550 alla sola Blackbox.
28. Bloccato ogni UDP Ground Station→Blackbox.
29. Bloccata ogni destinazione Ground Station→`10.77.10.0/24`.
30. Verificata assenza di hook forward nella nuova tabella.
31. Verificata assenza di NAT e regole generiche di accept.
32. Creata unità systemd oneshot separata per il firewall.
33. Creato installatore con requisito root esplicito.
34. Imposto `nft --check` e `systemd-analyze verify` prima dell'enable.
35. Copiati i tre artefatti firewall in staging sul portatile.
36. Verificata sintassi `/bin/sh -n` dell'installatore.
37. Verificata sintassi systemd dell'unità firewall.
38. Confrontati gli hash repository/staging: tutti coincidenti.
39. Confermato che firewall e tabella non sono stati attivati senza sudo.
40. Mantenuto stato Ground Station `PARTIAL`, senza falsa promozione.
41. Creato parser del journal soak Blackbox.
42. Aggiunti gate di campioni, monotonia, progresso e rate MAVLink.
43. Aggiunti gate zero errori/reject/truncated/failure.
44. Aggiunti gate separati per entrambi gli osservatori.
45. Aggiunti gate separati per entrambi i display e per recorder/DDP.
46. Superati 5/5 test dell'analizzatore soak.
47. Eseguito soak breve reale 60 s: `PASS`, 25,433 Hz e zero errori.
48. Creato verificatore QGC con 4/4 test e stato reale `PARTIAL` corretto.
49. Aggiornati ADR-015, runbook, stato autorevole e riga `NET-001`.
50. Eseguiti gate sintattici, suite congiunte, generatore stato e diff-check.

## Evidenza hardware del lotto

- QGC: PID 47492, `active/running`, unico listener UDP 14550,
  `NRestarts=0`;
- manifest/preflight: `PASS`, versione 5.0.8, SHA-256 AppImage conforme;
- Blackbox soak: 58 campioni, 25,433 Hz, MAVLink +1.526, recorder +1.168,
  DDP +2.336, ciascun osservatore +1.526, ciascun display +1.168;
- tutte le serie monotone, zero socket error, truncated, source reject e
  failure;
- status Ground Station: `PARTIAL` esclusivamente perché il firewall è
  `not-found/inactive/not-enabled`.

## Verificato solo software

- manifest e tutti i casi negativi;
- policy firewall e ordine fail-closed dell'installatore;
- analizzatore soak e verificatore QGC;
- staging e identità hash dei file da installare.

## Aperto — non dichiarare DONE

- installazione privilegiata e prova contatori/cattura del firewall laptop;
- digest completo o deployment atomico verificato dell'AppDir estratta;
- fault Wi-Fi reale con recovery temporizzato;
- secondo reboot server e contatore nft post-reboot;
- reboot/login grafico QGC;
- chiusura non graceful e plugin `speechd`;
- POWER1 regolata al posto dell'USB da banco;
- soak lungo con perdita, jitter, CPU, RAM e temperatura.

Il firewall preparato è difesa in profondità. La garanzia primaria di assenza
del ritorno verso Pixhawk resta l'architettura TX-only della Blackbox.
