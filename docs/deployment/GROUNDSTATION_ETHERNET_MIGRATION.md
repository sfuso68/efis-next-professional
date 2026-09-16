# Ground Station — migrazione Wi-Fi → Ethernet

Stato: procedura progettata, preflight software verificato, migrazione hardware
non eseguita. Uso di banco `NON-CERT`.

## Correzione topologica obbligatoria (lotto 50K)

La porta `eno1` della Ground Station deve essere collegata alla **LAN di
servizio `192.168.1.0/24`**, cioe' a una porta LAN del router o a uno switch
Ethernet appartenente alla stessa LAN. Non deve essere collegata al Waveshare
FLIGHT-DATA, che trasporta esclusivamente `10.77.10.0/24`.

Il Waveshare fotografato e gia' installato non e' uno switch LAN generico del
progetto: e' il dominio avionico isolato fra Pixhawk, Blackbox e display.
Assegnare `192.168.1.39` a una porta collegata a quel dispositivo non rende
raggiungibile `192.168.1.47`; crea invece una topologia incoerente e rischia di
confondere service plane e data plane. Non si deve aggiungere un alias
`192.168.1.x` alla Blackbox Ethernet, ne' abilitare bridge, NAT o forwarding.

## Decisione

La Ground Station conserva l'identità di servizio `192.168.1.39`, oggi
assegnata a `wlo1`, e la trasferisce in modo esclusivo a `eno1`. Questo evita
di cambiare il target unicast del broker Blackbox e le regole firewall già
verificate. Non è ammesso assegnare lo stesso indirizzo alle due interfacce.

Il Wi-Fi resta un fallback amministrativo controllato, non un percorso attivo
in parallelo. QGroundControl continua a possedere il solo socket wildcard UDP
14550; il firewall continua ad ammettere soltanto la sorgente Blackbox.

## Gate prima di qualsiasi mutazione

Eseguire localmente o via SSH:

```bash
/usr/bin/python3 \
  /home/stefano/.local/lib/efis-next/groundstation_ethernet_preflight.py \
  --phase physical
```

Il risultato deve essere `PASS`/exit 0. `BLOCKED`/exit 2 non autorizza la
migrazione. In particolare servono carrier su `eno1`, IP ancora esclusivo su
Wi-Fi, QGC attivo senza restart e un solo listener UDP 14550.

## Sequenza operativa progettata

1. Collegare fisicamente `eno1` al router/switch della LAN di servizio; **non**
   al Waveshare FLIGHT-DATA.
2. Ripetere il preflight fisico e identificare porta, MAC e carrier.
3. Creare un profilo NetworkManager Ethernet inattivo con indirizzo
   `192.168.1.39/24`; non attivarlo mentre Wi-Fi possiede l'indirizzo.
4. Verificare sintassi e proprietà del profilo senza applicarlo.
5. Installare un rollback one-shot che disattiva Ethernet prima di riattivare
   il profilo Wi-Fi `CASA`; il rollback deve essere armato prima dello switch.
6. Da console locale, disattivare Wi-Fi e attivare Ethernet come singola
   operazione controllata. La sessione SSH corrente può interrompersi.
7. Verificare con `--phase post-switch`: indirizzo esclusivo su `eno1`, route
   Blackbox direttamente via Ethernet, QGC attivo e listener unico. Il gate
   rifiuta inoltre forwarding IPv4 e una route diretta `10.77.10.0/24` su
   `eno1`.
8. Verificare firewall, telemetria passiva v3, broker e server observer.
9. Annullare il rollback soltanto dopo tutti i gate.
10. Registrare profilo, UUID, boot ID, hash dei report e risultati nella
    baseline di stato.

## Rollback obbligatorio

Il rollback deve, nell'ordine: disattivare il profilo Ethernet, verificare che
`192.168.1.39` non sia più su `eno1`, riattivare `CASA` su `wlo1` e confermare
route/QGC. Non deve mai attivare Wi-Fi lasciando lo stesso IP su Ethernet.

## Criteri di rifiuto

- carrier assente o instabile;
- indirizzo presente su entrambe le interfacce;
- DHCP Ethernet non controllato;
- route Blackbox ancora via Wi-Fi nel post-switch;
- listener UDP diverso o concorrente con QGC;
- restart QGC, errori firewall o perdita di sequenza MAVLink;
- rollback non armabile o console locale non disponibile.

Nessuna fase modifica Pixhawk, Blackbox, server o parametri ArduPilot.
