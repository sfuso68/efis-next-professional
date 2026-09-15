# EFIS NEXT — Network acceptance batch 50C

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: IN PROGRESS / DEVELOPMENT / NON-CERT / NON FLIGHT APPROVED

## Cinquanta punti del lotto

1. Riletti stato autorevole, handoff, checkpoint e ADR-015.
2. Verificata la branch e preservati gli `output/*` utente.
3. Inventariato l'accesso privilegiato separato sui tre nodi.
4. Confermato `ip_forward=0` sulla Blackbox.
5. Confermati broker Blackbox e QGC attivi con zero restart.
6. Confermato `Linger=no` sul laptop.
7. Confermata la presenza del socket Speech Dispatcher.
8. Ispezionata la metadata Qt del plugin `speechd`.
9. Escluso il tentativo diagnostico `offscreen` non supportato.
10. Ripetuta la diagnostica nel vero ambiente Wayland.
11. Riprodotto l'errore `undefined symbol`.
12. Identificato il simbolo `g_string_free_and_steal`.
13. Verificata l'assenza del simbolo nella GLib AppDir.
14. Verificata la presenza del simbolo nella GLib Debian 13.
15. Inventariata GLib host 2.84.4.
16. Inventariata `libspeechd2` 0.12.0.
17. Provato `LD_PRELOAD` in un processo QGC isolato.
18. Verificato `loaded library` del plugin con il preload.
19. Aggiunto il contratto `runtime_preload` al manifest.
20. Aggiunto il probing fail-closed del simbolo al preflight.
21. Aggiunto il rifiuto di percorso preload mancante/non assoluto.
22. Limitato il preload al solo `ExecStart` QGC.
23. Conservato immutato il digest dell'AppDir.
24. Aggiornati i test del manifest/preflight.
25. Distribuiti manifest, preflight e unità sul laptop.
26. Verificato preflight reale `PASS` con simbolo presente.
27. Verificato QGC PID 49184 `active/running`.
28. Verificato `NRestarts=0`.
29. Verificato un solo listener UDP 14550.
30. Verificata assenza del warning speech nella nuova invocazione.
31. Creato gate reboot/login grafico QGC.
32. Imposto boot ID diverso come requisito del check.
33. Imposti manifest, listener, speech e zero restart come requisiti.
34. Imposto `Linger=no` come requisito laptop.
35. Armata baseline reboot QGC con stato `PASS`.
36. Creato gate reboot observer server.
37. Imposti sink RX-only LIVE e freshness come requisiti.
38. Imposti firewall server e listener come requisiti.
39. Imposti `Linger=yes` e zero restart come requisiti.
40. Imposti i dodici container e Caddy 401 come requisiti.
41. Armata baseline reboot server con stato `PASS`.
42. Eseguita acceptance Blackbox retrospettiva di 3.600 secondi.
43. Verificata copertura campioni 97,39%.
44. Verificata frequenza MAVLink 26,279 Hz.
45. Verificata parità esatta verso due observer e due display.
46. Verificati zero errori/restart e throttling zero.
47. Aggiunto rollback firewall automatico di 300 secondi.
48. Aggiunta conferma firewall solo dopo tre gate locali.
49. Aggiornato e validato lo staging firewall senza attivarlo.
50. Preparata la sequenza interattiva server→laptop→firewall→WLAN→POWER1.

Verifica regressione del lotto: 72 test mirati `PASS` (`skipped=2` sul
runner Windows per i casi POSIX), `py_compile`, generatore stato,
`generate_status.py --check` e `git diff --check` tutti `PASS`.

## Gate interattivi armati ma non ancora chiusi

1. reboot server e `server_observer_reboot_acceptance.py check`;
2. reboot/login laptop e `qgc_reboot_acceptance.py check`;
3. installazione firewall con rollback, verifica esterna e commit;
4. fault WLAN con recupero temporizzato;
5. alimentazione POWER1 definitiva e rimozione USB di banco.

Nessuno dei cinque gate è dichiarato verificato prima dell'evidenza reale.
Il controllo del 2026-09-15 ha inoltre confermato che il server conserva il
boot ID della baseline `6182cee7-962f-4e92-86ab-0e9fb51d6059`: il reboot non è
ancora avvenuto e il primo gate resta aperto.
