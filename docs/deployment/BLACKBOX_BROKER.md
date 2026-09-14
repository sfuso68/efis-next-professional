# Blackbox DDP broker

Stato: installato, attivo e abilitato sulla Blackbox reale il 2026-09-07;
verificato end-to-end con il receiver DDP sul PFD attraverso FLIGHT-DATA
Ethernet `10.77.10.0/24`.
I gate prodotto su Ethernet isolata, autenticazione e fault test estesi restano
aperti.

`efis-blackbox-broker` è il processo del data plane che realizza il confine
della ADR-003:

```text
Pixhawk MAVLink UDP ─┐
                     ├─ pipeline normalizzata/arbitrata ─┬─ DDP V2 UDP ─ RPi5
ESP32 SocketCAN ─────┘                                   └─ flight recorder
          │
          └─ copia MAVLink TX-only ─ portatile/server osservatori
```

Entrambi gli ingressi sono monitor-only. Il processo apre una porta UDP locale
per ricevere MAVLink e un socket SocketCAN in lettura; non contiene sender
MAVLink verso Pixhawk, richieste stream, scritture parametri o trasmissioni
CAN. ADR-015 aggiunge un fan-out UDP esclusivamente trasmittente verso
osservatori esterni nominati; non legge risposte e non crea un return path.
Le uscite operative verso i display restano snapshot DDP/EDP normalizzati.
Lo stesso snapshot, preceduto dagli eventi nodo/sorgente che lo hanno prodotto,
viene scritto nel recorder segmentato locale.

## Configurazione iniziale

Il file `deploy/systemd/efis-blackbox-broker.service` usa questi valori di banco:

- MAVLink: `10.77.10.2:14550`, sola sorgente `10.77.10.3`;
- osservatori TX-only: `192.168.1.39:14550` e `192.168.1.6:14550`;
- sensori ESP32: `can_a`;
- destinazione PFD: `10.77.10.11:49002`;
- nodo logico Blackbox: `47`;
- frequenza snapshot: 20 Hz.

`--display` è ripetibile. Per esempio, quando anche un secondo pannello avrà un
consumer DDP verificato:

```sh
--display 10.77.10.11 --display 10.77.10.13 --display-port 49002
```

Tutte le destinazioni condividono identità publisher e cadenza, ma mantengono
socket e contatori separati. Una destinazione duplicata viene rifiutata; un
pannello spento non arresta recorder o invio agli altri endpoint. L'unità
systemd usa `EFIS_DDP_DISPLAY_ARGS`, sovrascrivibile nel file ambiente, per
fornire la lista completa senza modificare l'unità distribuita.

Il secondo esempio è un futuro MFD che consuma lo stesso snapshot di volo.
Non usare `--display 10.77.10.12` per l'EMS: DDP V2 non contiene
`EngineSnapshot`. L'EMS usa EDP V1 separato sulla porta 49003. Il feed iniziale
di banco va abilitato solo in modo esplicito:

```sh
--engine-display 10.77.10.12 --engine-port 49003 --engine-simulation
```

Sul banco a due display installare anche
`deploy/systemd/efis-blackbox-broker-multidisplay.override.conf` come
`/etc/systemd/system/efis-blackbox-broker.service.d/multidisplay.conf`. Il
drop-in sostituisce intenzionalmente `ExecStart` e rende esplicite entrambe le
destinazioni (`10.77.10.11` PFD e `10.77.10.13` MFD), anche quando un precedente
`/etc/efis/blackbox-broker.env` contiene ancora un solo display. La destinazione
EMS resta separata su EDP V1/49003.

`--engine-simulation` non è un alias per il futuro ingresso reale: deve essere
rimosso quando il publisher verrà collegato allo snapshot CAN arbitrato.

Gli indirizzi osservati via DHCP non sono identità permanenti. Prima
dell'installazione creare `/etc/efis/blackbox-broker.env` con i valori della
rete di banco e permessi `0640 root:blackbox`, oppure introdurre prenotazioni
DHCP/DNS controllate. Il file non deve contenere token e non va committato.

Prima di installare l'unità creare esplicitamente l'area persistente richiesta
dal sandbox systemd:

```sh
sudo install -d -o blackbox -g blackbox -m 0750 /var/lib/blackbox/flight
```

## Avvio manuale di verifica

```sh
/opt/efis-blackbox/bin/efis-blackbox-broker \
  --mavlink-bind 10.77.10.2 --mavlink-port 14550 \
  --mavlink-source 10.77.10.3 \
  --mavlink-observer 192.168.1.39 \
  --mavlink-observer 192.168.1.6 --mavlink-observer-port 14550 \
  --can-interface can_a \
  --display 10.77.10.11 --display-port 49002 \
  --source-node 47 --records /var/lib/blackbox/flight --rate-hz 20
```

Non esiste un fallback simulato in questo eseguibile. Se gli ingressi tacciono,
il broker continua a inviare uno snapshot esplicitamente non disponibile, così
il display distingue un collegamento DDP vivo da sensori assenti.

Il log periodico `blackbox_display_health` espone per ogni destinazione
datagrammi, byte, errori e ultimo errore; `blackbox_broker_health` riporta il
totale e continua a essere emesso anche se una destinazione fallisce.
`blackbox_mavlink_observer_health` e
`blackbox_mavlink_observer_target` espongono i contatori del fan-out. Il loro
stato non autorizza comandi e non dichiara raggiungibile il Pixhawk dalla LAN.

## Gate prima dell'attivazione persistente

1. confermare quale interfaccia fisica (`can_a`/`can_b`) porta ESP32 e Matek;
2. configurare Pixhawk/companion networking separatamente e senza disabilitare
   controlli pre-arm; il broker non effettua questa configurazione;
3. verificare con `ss` che il broker ascolti soltanto sulla porta prevista;
4. catturare contatori `blackbox_broker_health` e `render_health` PFD;
5. provare scollegamento, riordino/perdita, reboot, latenza e jitter;
6. confermare con cattura rete che dal PFD non partano pacchetti verso il
   controllo di volo.

Questi gate non sono chiusi dalla build o dal test loopback software.
