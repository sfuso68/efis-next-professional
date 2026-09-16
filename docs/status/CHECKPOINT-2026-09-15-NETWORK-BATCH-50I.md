# EFIS NEXT — MAVLink assessment states batch 50I

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: IMPLEMENTED / SOFTWARE PASS / REAL REPORTS CLASSIFIED / NON-CERT

ID interessati: `NET-001`, `GS-001`.

## Cinquanta punti del lotto

1. Riletta la baseline obbligatoria e ADR-015.
2. Confermato branch `work/mavlink-observer-gateway-v1`.
3. Preservati gli output utente non tracciati.
4. Mantenuto POWER1 fuori scope.
5. Escluse modifiche a firmware e parametri Pixhawk.
6. Confermata la natura monitor-only del percorso.
7. Conservate tutte le soglie 50H senza allargarle.
8. Separato il significato di integrità da quello di liveness.
9. Separato il significato di uniformità temporale.
10. Definito il gruppo di check `INTEGRITY`.
11. Inclusa copertura temporale nell'integrità.
12. Incluso rate minimo nell'integrità.
13. Inclusa presenza frame nell'integrità.
14. Inclusi malformed e gap nell'integrità.
15. Inclusi duplicati e riordino nell'integrità.
16. Inclusi errori socket nell'integrità.
17. Definito il gruppo di check `LIVENESS`.
18. Inclusa presenza HEARTBEAT nella liveness.
19. Incluso timing HEARTBEAT nella liveness.
20. Incluso massimo HEARTBEAT nella liveness.
21. Definito il gruppo di check `UNIFORMITY`.
22. Inclusa presenza campioni nell'uniformità.
23. Incluso massimo datagramma nell'uniformità.
24. Incluso p95 datagramma nell'uniformità.
25. Definito `link_status=PASS` solo con integrità+liveness PASS.
26. Definito `quality_status=DEGRADED` se uniformità fallisce.
27. Conservato `overall_status=FAIL` se un gruppo fallisce.
28. Imposto sempre `flight_approval=false`.
29. Resa fail-closed l'assenza di un check obbligatorio.
30. Elencati separatamente check falliti e mancanti.
31. Incrementato lo schema cattura a v3.
32. Mantenuto il campo storico `status` uguale all'esito globale.
33. Mantenuto exit code cattura 1 sull'esito globale FAIL.
34. Creato verificatore offline `assess_mavlink_capture.py`.
35. Supportati report storici v2 e nuovi v3.
36. Rifiutati schemi non supportati.
37. Rifiutati check non booleani.
38. Limitata la dimensione report a 1 MiB.
39. Calcolato SHA-256 del report sorgente.
40. Vietato qualsiasi replay o accesso al payload.
41. Definito exit 0 per PASS completo.
42. Definito exit 2 per link PASS/qualità DEGRADED.
43. Definito exit 1 per link FAIL o report invalido.
44. Aggiunti tre test dei gruppi nel capture tool.
45. Aggiunti quattro test del verificatore offline.
46. Suite dedicata totale 24/24 `PASS`.
47. Regressione rete totale 121 test, 119 `PASS`, 2 skip attesi.
48. Distribuiti entrambi i tool con hash identici sui due nodi.
49. Classificato Ethernet reale PASS/PASS/PASS con exit 0.
50. Classificato Wi-Fi reale link PASS, quality DEGRADED, overall FAIL con
    exit 2; QGC e broker restano attivi con zero restart.

## Evidenza di classificazione hardware

| Lato | Integrity | Liveness | Uniformity | Link | Quality | Overall | Exit |
|---|---|---|---|---|---|---|---:|
| Pixhawk→Blackbox Ethernet | PASS | PASS | PASS | PASS | PASS | PASS | 0 |
| Blackbox→QGC Wi-Fi | PASS | PASS | FAIL | PASS | DEGRADED | FAIL | 2 |

- Capture tool SHA-256:
  `d01e13888a8975bc0ac0f2065adef2a00815a3f8385bb26271764908236ad93d`.
- Assessment tool SHA-256:
  `25556a6613d24be975a4493f22c238805ab98f0aabdd493002dcf092b71f2b05`.
- Hash identici su portatile e Blackbox; `py_compile` verificato.
- QGC PID 2660 e broker PID 2448 sono `active/running`, `NRestarts=0`.

## Aperto / non dichiarare DONE

- nessuna cattura hardware v3 ancora eseguita; la classificazione usa report
  reali v2 già acquisiti;
- quality Wi-Fi resta `DEGRADED` e overall resta `FAIL`;
- prova simultanea e Ground Station Ethernet restano raccomandate;
- persistenza power save post-reboot e fault confinato restano aperti;
- nessun esito equivale ad approvazione al volo.
