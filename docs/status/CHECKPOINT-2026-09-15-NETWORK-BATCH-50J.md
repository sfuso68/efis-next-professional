# EFIS NEXT — Ground Station Ethernet readiness batch 50J

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: SOFTWARE READY / HARDWARE BLOCKED NO-CARRIER / NO NETWORK MUTATION / NON-CERT

ID interessati: `NET-001`, `GS-001`.

## Cinquanta punti del lotto

1. Riletta la baseline e ADR-015.
2. Preservati gli output utente non tracciati.
3. Mantenuto POWER1 fuori scope.
4. Escluse modifiche a Pixhawk e ArduPilot.
5. Inventariata la Ground Station in sola lettura.
6. Identificata Ethernet `eno1`.
7. Identificato Wi-Fi `wlo1`.
8. Confermato Wi-Fi attivo sul profilo `CASA`.
9. Confermato indirizzo attuale `192.168.1.39/24`.
10. Confermata route Blackbox via `wlo1`.
11. Confermato `eno1` senza indirizzo.
12. Confermato `eno1` in stato `NO-CARRIER`.
13. Confermato QGC PID 2660 attivo.
14. Confermati zero restart QGC.
15. Confermato listener unico UDP 14550.
16. Confermato proprietario listener QGroundControl.
17. Identificato vincolo fan-out sull'IP `192.168.1.39`.
18. Identificato vincolo firewall sullo stesso IP.
19. Rifiutata l'assegnazione DHCP Ethernet casuale.
20. Scelto il trasferimento esclusivo dell'identità `.39`.
21. Vietato lo stesso IP simultaneo su Wi-Fi/Ethernet.
22. Mantenuto Wi-Fi solo come fallback controllato.
23. Definita fase preflight `physical`.
24. Definita fase preflight `post-switch`.
25. Implementato inventario read-only sysfs Ethernet.
26. Implementato inventario IPv4 tramite `ip -j`.
27. Implementata verifica route verso Blackbox.
28. Implementata verifica unità QGC.
29. Implementato conteggio listener UDP 14550.
30. Implementata verifica ownership QGC del listener.
31. Validati nomi interfaccia con grammatica chiusa.
32. Validati indirizzi come IPv4 specifici.
33. Vietate interfacce Ethernet/Wi-Fi identiche.
34. Reso `BLOCKED` il carrier assente.
35. Resa `BLOCKED` la duplicazione dell'indirizzo.
36. Reso `BLOCKED` ogni restart QGC.
37. Reso `BLOCKED` un listener assente o concorrente.
38. Resa `BLOCKED` una route post-switch errata.
39. Resa `BLOCKED` una sorgente route diversa da `.39`.
40. Definito exit 0 per readiness PASS.
41. Definito exit 2 per blocco hardware/configurazione.
42. Definito exit 1 per errore del preflight.
43. Aggiunti sei test dedicati.
44. Verificato caso fisico senza carrier.
45. Verificato caso fisico pronto.
46. Verificato caso post-switch corretto.
47. Verificati duplicazione IP, route e collisione listener.
48. Distribuito tool sul portatile con hash identico.
49. Eseguito preflight reale: unico fallimento `ethernet_carrier`.
50. Documentata sequenza con rollback; nessun profilo creato o attivato.

## Evidenza reale

- `eno1`: presente, operstate `down`, carrier falso, nessun IPv4.
- `wlo1`: attiva con `192.168.1.39`, route Blackbox e profilo `CASA`.
- QGC: PID 2660, `active/running`, zero restart, listener unico 14550.
- Preflight: `BLOCKED`, exit 2, solo check fallito `ethernet_carrier`.
- Tool SHA-256:
  `f1a8bb87ce8dd37b8e5d5c6693f0ba8addca42ac18763ee4b435fd0cb9a768ad`.

## Aperto / non dichiarare DONE

- collegare fisicamente il portatile allo switch Waveshare;
- ripetere fase `physical` e ottenere PASS;
- solo allora creare profilo inattivo e rollback;
- switch IP e post-switch richiedono console locale;
- nessuna prova Ethernet MAVLink Ground Station è ancora eseguita.
