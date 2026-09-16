# EFIS NEXT — Network endurance and endpoint batch 50E

Data: 2026-09-15

Branch: `work/mavlink-observer-gateway-v1`

Stato: AUDIT COMPLETE / FINDINGS OPEN / DEVELOPMENT / NON-CERT / NON FLIGHT APPROVED

POWER1 è esplicitamente rinviato. Tutte le prove di questo lotto sono state
eseguite con l'alimentazione USB di banco esistente, senza rimuovere o
ricablare alimentazioni.

## Cinquanta punti del lotto

1. Riletta la baseline autorevole prima delle modifiche.
2. Confermato il branch di lavoro e l'HEAD `1589945` iniziale.
3. Preservati e non aggiunti all'indice tutti gli output utente.
4. Registrato il rinvio esplicito del gate POWER1.
5. Escluse modifiche a PSU, USB, Pixhawk e cablaggi.
6. Eseguito collector Blackbox retrospettivo di 3.600 secondi.
7. Confermata lettura completa del journal broker.
8. Confermata lettura completa dello stato systemd broker.
9. Confermata disponibilità della misura termica.
10. Confermata disponibilità dei flag di throttling.
11. Raccolti 3.508 campioni su 3.600 attesi.
12. Misurata copertura 97,44%, superiore al gate 90%.
13. Misurato ingresso MAVLink medio a 26,293 Hz.
14. Verificato incremento MAVLink +94.656.
15. Verificato incremento recorder +71.995.
16. Verificato incremento DDP +143.990.
17. Verificato incremento fan-out +189.312.
18. Verificato observer Ground Station +94.656.
19. Verificato observer server +94.656.
20. Verificato display PFD +71.995.
21. Verificato display MFD +71.995.
22. Confermata parità esatta di observer e display.
23. Confermati contatori monotoni e zero errori broker/fan-out.
24. Confermato broker PID 2448, attivo e `NRestarts=0`.
25. Misurati 3.816.308.736 byte di memoria disponibile.
26. Misurato RSS broker 4.341.760 byte.
27. Misurata Blackbox a 47,7 °C al collector e 48,3 °C all'audit.
28. Confermato `get_throttled=0x0`.
29. Confermato `/proc/sys/net/ipv4/ip_forward=0`.
30. Confermata assenza di bridge, bond e VRF sulla Blackbox.
31. Confermato unico bind MAVLink operativo `10.77.10.2:14550`.
32. Confermato link Blackbox FLIGHT-DATA gigabit/full duplex/EEE off.
33. Confermato link PFD FLIGHT-DATA gigabit/full duplex/EEE off.
34. Confermato link MFD FLIGHT-DATA gigabit/full duplex/EEE off.
35. Confermato link EMS FLIGHT-DATA gigabit/full duplex/EEE off.
36. Confermata persistenza della mitigazione EEE sui quattro nodi.
37. Confermati zero errori RX/TX Ethernet sui quattro nodi; EMS conserva due
    drop TX cumulativi non crescenti non attribuiti.
38. Baseline Pixhawk: 100/100, media 0,163 ms, mdev 0,051 ms.
39. Baseline PFD: 100/100, media 0,200 ms, mdev 0,021 ms.
40. Baseline EMS: 100/100, media 0,182 ms, mdev 0,016 ms.
41. Baseline MFD: 100/100, media 0,202 ms, mdev 0,020 ms.
42. Baseline server: 100/100, media 4,226 ms, mdev 1,937 ms.
43. Rilevata Ground Station Wi-Fi variabile: prima finestra 98/100, media
    128,726 ms e mdev 94,590 ms; seconda finestra 16/20 e mdev 89,104 ms;
    terza finestra 20/20 ma media 147,902 ms e mdev 100,342 ms.
44. Confermato QGC `PASS`, PID 2660, `NRestarts=0`, firewall e mute attivi.
45. Confermato socket QGC UDP 14550 con coda zero e drop socket zero; in 20 s
    +537 datagrammi UDP host e zero incremento `InErrors/RcvbufErrors`.
46. Confermato sink server PID 999, LIVE/RX-only e zero errori/restart.
47. Confermato PFD user service attivo da due giorni e `NRestarts=0`.
48. Confermato MFD direct service attivo da 19 ore e `NRestarts=0`.
49. Confermato EMS attivo da oltre una settimana; `NRestarts=2` è storico ma
    il journal è ruotato e non consente di attribuirne la causa.
50. Implementato e provato su Blackbox il misuratore read-only versionato
    `network_endpoint_acceptance.py`; 7/7 test software dedicati sono `PASS`.

## Valutazione critica

- FLIGHT-DATA supera la baseline breve: zero perdita e latenze sub-millisecondo
  verso Pixhawk, PFD, EMS e MFD.
- La Ground Station Wi-Fi non supera il gate proposto di 1% perdita, 100 ms di
  media e 50 ms di RTT mdev. Il segnale radio è forte (-40 dBm), il link è VHT
  5 GHz ad alto bitrate, ma il power save risulta attivo. ICMP non equivale a
  perdita MAVLink: il socket QGC non mostra drop, quindi la telemetria non è
  dichiarata persa. La variabilità resta però incompatibile con una baseline
  professionale senza ulteriore prova.
- Gli `InErrors/RcvbufErrors` UDP cumulativi Blackbox sono 111.985 ma il delta
  misurato su 20 secondi è zero. Non sono attribuiti al broker e restano un
  contatore storico da sorvegliare.
- I due restart EMS non sono recenti: il processo corrente è stabile da oltre
  una settimana. Poiché il journal originario è ruotato, la causa resta
  indeterminata e non viene dichiarata risolta.
- Il tentativo remoto non privilegiato di impostare il profilo Wi-Fi su power
  save disabilitato è stato rifiutato da NetworkManager con `Insufficient
  privileges`; non ha modificato il profilo (`default`) né il runtime (`on`).

## Gate successivi

1. verificare al prossimo reboot la persistenza del power save disabilitato;
   preferire comunque Ethernet per un ruolo Ground Station stabile;
2. aggiungere una misura applicativa di sequenza/freshness/jitter lato QGC, perché
   ICMP e contatori di invio Blackbox non provano la consegna MAVLink;
3. preparare un fault perdita/jitter confinato al solo observer, con rollback
   automatico e senza toccare FLIGHT-DATA, DDP/EDP o SSH;
4. ripetere endurance con terreno/video worst-case;
5. investigare i restart EMS soltanto alla prossima finestra manutentiva;
6. mantenere POWER1 aperto fino all'identificazione fisica richiesta.

## Follow-up power save 50E

#### Verificato su hardware reale

- L'operatore ha impostato il profilo `CASA` su `disable` e il runtime riporta
  `Power save: off`; la connessione è rimasta attiva su `192.168.1.39`.
- La matrice completa successiva è `PASS`. Ground Station: 20/20, perdita 0%,
  RTT medio 7,528 ms, massimo 21,355 ms e mdev 4,401 ms. I quattro endpoint
  FLIGHT-DATA restano senza perdita e sotto 0,22 ms medi; server `PASS`.
- La prova estesa Ground Station è `PASS`: 200/200, perdita 0%, RTT medio
  10,352 ms, massimo 125,880 ms e mdev 17,390 ms.
- QGC conserva PID 2660, `NRestarts=0`, socket UDP 14550 con coda/drop zero;
  `InErrors` e `RcvbufErrors` host restano zero. Segnale -42 dBm e power save
  runtime ancora `off` al termine.

#### Limiti / aperto

- Il miglioramento ICMP è forte ma non sostituisce una metrica di sequenza e
  freshness MAVLink lato consumer. Il picco RTT di 125,880 ms conferma che
  Ethernet resta preferibile per una Ground Station con ruolo operativo.
- La persistenza dell'impostazione `disable` deve essere verificata dopo il
  prossimo reboot del portatile; non viene dichiarata verificata ora.
