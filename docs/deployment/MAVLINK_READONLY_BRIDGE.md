# Bridge MAVLink read-only del banco

Il portatile Ground Station apre una sola volta la porta USB del Pixhawk e
inoltra la telemetria alla Blackbox e, opzionalmente, a QGroundControl locale.
Questo è un collegamento provvisorio di banco; la topologia finale resta
Pixhawk Ethernet/CAN verso la Blackbox su rete avionica cablata.

Il processo apre la seriale con `O_RDONLY`, non contiene `write()` verso quel
descrittore e non genera heartbeat, richieste stream, comandi o scritture di
parametri. Non interpreta MAVLink: conserva i byte e lascia parsing, health e
arbitraggio al broker Blackbox, secondo ADR-003.

Le destinazioni devono essere indirizzi IPv4 numerici unicast. Il forwarder
rifiuta wildcard, multicast, broadcast, duplicati e l'intera rete avionica
FLIGHT-DATA `10.77.10.0/24`; quindi non puo' essere configurato per inviare una
copia al Pixhawk o a un altro nodo avionico. La policy viene rivalidata anche
quando `ReadOnlyForwarder` e' costruito direttamente, senza parser CLI.

## Installazione utente

Installare lo script in:

```text
/home/stefano/.local/lib/efis-next/mavlink_readonly_forwarder.py
```

Installare l'unità come:

```text
/home/stefano/.config/systemd/user/efis-mavlink-readonly-bridge.service
```

La configurazione specifica del banco appartiene al file non versionato
`~/.config/efis/mavlink-readonly-bridge.env`:

```ini
EFIS_PIXHAWK_SERIAL=/dev/serial/by-id/usb-ArduPilot_Pixhawk6X_3F001D000751333439393536-if00
EFIS_MAVLINK_DEST_ARGS=--destination 192.168.1.47:14550 --destination 127.0.0.1:14550
```

Il percorso `by-id` evita lo scambio fra `ttyACM0` e `ttyACM1`. La porta
`if00` è stata identificata come stream MAVLink 2; `if02` contiene anche
console/testo e non è la sorgente primaria.

## QGroundControl

Quando il bridge è attivo, QGroundControl deve ascoltare UDP su
`127.0.0.1:14550` e non aprire automaticamente la seriale USB. Per svolgere
manutenzione o modificare parametri, arrestare esplicitamente il bridge e
collegare QGroundControl alla seriale. La modalità manutenzione non fa parte
del percorso EFIS read-only e deve essere tracciata separatamente.

## Gate

- byte reali osservati sulla Blackbox con contatori MAVLink crescenti;
- snapshot PFD con provenienza Pixhawk e assenza marcatore simulazione;
- nessun file descriptor seriale scrivibile nel processo;
- scollegamento e riconnessione USB senza blocco del broker;
- nessuna variazione dei parametri ArduPilot durante il collaudo.
