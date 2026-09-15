# EFIS NEXT — Network assurance batch 50D

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: IN PROGRESS / DEVELOPMENT / NON-CERT / NON FLIGHT APPROVED

## Cinquanta punti del lotto

1. Riletta la baseline autorevole e preservati gli output utente.
2. Confermato il nuovo boot ID del laptop.
3. Confermato firewall `enabled` dopo reboot.
4. Confermato firewall `active` dopo reboot.
5. Confermato rollback firewall `inactive`.
6. Confermato sink server `active/running`.
7. Confermato sink server `NRestarts=0`.
8. Confermato health server LIVE.
9. Confermato health server RX-only.
10. Confermati zero errori socket server.
11. Identificata assenza di sessione grafica utente.
12. Distinto greeter GDM da login operativo.
13. Evitato l'avvio QGC remoto privo di Wayland.
14. Mantenuto aperto il gate QGC post-reboot.
15. Conservato `Linger=no` come requisito.
16. Osservato warning Linux di `pgrep -x` oltre 15 caratteri.
17. Escluso il precedente esito process-list come prova sufficiente.
18. Sostituito il probe con command-line regex ancorata.
19. Aggiornato il kill controllato con la stessa regex.
20. Evitato match generico su processi non pertinenti.
21. Aggiunto test del comando `pgrep -f`.
22. Aggiunto test contro regressione `pgrep -x`.
23. Scoperta assenza di `os.getuid` sul runner Windows.
24. Sostituita l'identità numerica con utente nominale portabile.
25. Rilanciati i test mirati del monitor QGC.
26. Distribuito il monitor corretto sul laptop.
27. Validata sintassi Python sul laptop.
28. Distribuito lo script mute corretto.
29. Validata sintassi shell sul laptop.
30. Confermata assenza processo speech col nuovo probe.
31. Analizzato il fault WLAN esistente.
32. Conservato il requisito di console locale.
33. Aggiunto preflight `--check` non mutante.
34. Aggiunto controllo presenza tool richiesti.
35. Aggiunto controllo tipo dispositivo Wi-Fi.
36. Conservato limite durata 20–180 secondi.
37. Risolto il profilo NetworkManager in UUID.
38. Rifiutati profili mancanti o UUID multipli.
39. Verificata connessione attiva tramite UUID/interfaccia.
40. Usato UUID anche nel comando di recovery.
41. Conservato timer recovery armato prima del disconnect.
42. Conservata verifica timer attivo prima del fault.
43. Aggiunto test preflight-before-root.
44. Aggiunto test recovery-by-UUID.
45. Aggiunto test arm-before-disconnect.
46. Distribuito il fault harness sul laptop.
47. Validata sintassi shell nativa.
48. Eseguito preflight reale `PASS` su `wlo1`/`CASA`.
49. Rinviato il disconnect fino ad autostart QGC verificato.
50. Mantenuto POWER1 bloccato fino a identificazione hardware e cablaggio.

## Esito gate

1. `PASS` — login grafico e QGC/firewall/mute post-reboot;
2. `PASS` — fault WLAN 45 s con monitor Blackbox/server e recovery;
3. `OPEN` — identificazione e collaudo POWER1, quindi rimozione USB di banco.

POWER1 non è dichiarato verificato senza evidenza reale.

## Evidenza fault WLAN 50D

- Dopo il ripristino `wlo1` è nuovamente `connected` al profilo `CASA` e il
  laptop possiede nuovamente `192.168.1.39`.
- QGC è rimasto `active/running` con lo stesso PID 2660 osservato prima del
  fault, `NRestarts=0` e listener UDP 14550 unico. Firewall V2, mute e assenza
  del processo Speech Dispatcher sono rimasti `PASS`.
- Il collector Blackbox di 180 s ha acquisito 176 campioni (copertura 97,78%):
  MAVLink 26,156 Hz e delta +4708, ciascun observer +4708, ciascun display
  +3581, DDP +7162 e fan-out +9416. Monotonia, parità, errori e restart sono
  tutti a zero; temperatura 47,7 °C e `get_throttled=0`.
- Il broker Blackbox è rimasto PID 2448 con `NRestarts=0`. Il sink server è
  rimasto PID 999, `active/running`, `NRestarts=0`, LIVE/RX-only, con sorgente
  Blackbox e zero datagrammi malformati, errori socket o sorgenti rifiutate.
- I contatori observer dimostrano continuità degli invii della Blackbox, non
  ricezione sul laptop durante l'intervallo senza WLAN. I timestamp esatti di
  disconnect/reconnect NetworkManager non sono disponibili: il collector
  transitorio era `--collect` e l'utente non dispone dell'accesso al journal
  di sistema. Il gate copre quindi fault, recupero e assenza di restart, non
  una misura certificata del tempo di riconnessione.
