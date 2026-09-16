# EFIS NEXT — Ground Station topology guard batch 50K

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: SOFTWARE VERIFIED / HARDWARE NOT REACHED / NO NETWORK MUTATION / NON-CERT

ID interessati: `NET-001`, `GS-001`.

## Cinquanta punti del lotto

1. Riletta la baseline obbligatoria prima delle modifiche.
2. Riletti handoff e checkpoint storico.
3. Riletta ADR-010 sulla segmentazione.
4. Riletta ADR-015 sul fan-out monitor-only.
5. Riletto il checkpoint 50J.
6. Preservati gli output utente non tracciati.
7. Mantenuto POWER1 fuori scope.
8. Escluse modifiche a Pixhawk.
9. Escluse modifiche ad ArduPilot.
10. Escluse modifiche al software avionico runtime.
11. Tentato il preflight remoto in sola lettura.
12. Confermata raggiungibilita' SSH del portatile.
13. Registrato rifiuto autenticazione SSH.
14. Non inferito alcuno stato hardware dal rifiuto.
15. Riesaminata l'identita' Ground Station `192.168.1.39`.
16. Riesaminata l'identita' Blackbox service `192.168.1.47`.
17. Riesaminato FLIGHT-DATA `10.77.10.0/24`.
18. Riesaminato il ruolo del Waveshare 5P.
19. Identificata l'ambiguita' “Waveshare della LAN di servizio”.
20. Stabilito che il Waveshare appartiene soltanto a FLIGHT-DATA.
21. Stabilito che `eno1` deve entrare nella LAN di servizio.
22. Indicati router o switch LAN come terminazione corretta.
23. Vietato collegare la Ground Station service al Waveshare avionico.
24. Vietato aggiungere alias `192.168.1.x` alla Blackbox FLIGHT-DATA.
25. Confermato il divieto di bridge IP.
26. Confermato il divieto di NAT.
27. Confermato il divieto di forwarding IPv4.
28. Mantenuto il trasferimento esclusivo dell'indirizzo `.39`.
29. Mantenuto il divieto di duplicare `.39` su Wi-Fi ed Ethernet.
30. Elevato lo schema preflight alla versione 2.
31. Aggiunta acquisizione del gateway della route Blackbox.
32. Aggiunta acquisizione delle route esatte FLIGHT-DATA.
33. Aggiunta acquisizione dello stato `ip_forward`.
34. Mantenuta la fase `physical` priva di mutazioni.
35. Mantenuta la fase `post-switch` priva di mutazioni.
36. Aggiunto gate Blackbox direttamente sulla LAN di servizio.
37. Aggiunto gate assenza route FLIGHT-DATA su `eno1`.
38. Aggiunto gate forwarding IPv4 disabilitato.
39. Conservati i gate carrier e presenza interfaccia.
40. Conservati i gate di esclusivita' dell'indirizzo.
41. Conservati i gate QGC attivo e zero restart.
42. Conservato il gate listener UDP 14550 unico.
43. Conservato il gate ownership QGC del listener.
44. Aggiunto test di rifiuto topologia FLIGHT-DATA errata.
45. Aggiunto test di rifiuto percorso Blackbox via gateway.
46. Aggiunto test di rifiuto `ip_forward=1`.
47. Aggiornato il runbook con cablaggio corretto.
48. Aggiornata ADR-015 con la correzione topologica.
49. Aggiornati stato e matrice senza promuovere `GS-001`.
50. Lasciata la migrazione hardware bloccata fino a carrier e SSH verificati.

## Evidenza e limiti

Il lotto corregge e verifica software e documentazione. Non dimostra che il
cavo sia collegato, non crea profili NetworkManager, non cambia route e non
esegue una cattura MAVLink Ethernet. La precedente evidenza `NO-CARRIER` resta
l'ultima misura hardware valida; il tentativo odierno e' terminato prima del
login SSH con `Permission denied (publickey,password)`.

La verifica locale comprende 80 test rete/MAVLink PASS. La suite PTY del
forwarder read-only non e' eseguibile su Windows per assenza di `termios` e non
e' conteggiata come PASS; resta coperta dalle precedenti esecuzioni Linux.

## Ripartenza esatta

- Collegare `eno1` al router/switch della LAN `192.168.1.0/24`, non al
  Waveshare FLIGHT-DATA.
- Ripristinare la chiave SSH oppure lavorare dalla console locale.
- Distribuire il preflight v2 e ottenere `physical=PASS`.
- Solo dopo preparare profilo inattivo e rollback one-shot.
- Eseguire switch, `post-switch=PASS` e cattura passiva v3.
