# Migrazione Ethernet avionica V1

Stato: procedura di banco reversibile, non approvata per il volo.

## Risultato dell'audit del 2026-09-07

| Nodo | Interfaccia | Stato osservato | Accesso di recupero |
|---|---|---|---|
| PFD `192.168.1.45` | `eth0` | `10.77.10.11/24`, 1000/full | WLAN attiva |
| Blackbox `192.168.1.47` | `eth0` | `10.77.10.2/24`, 1000/full | WLAN attiva |
| Ground Station `192.168.1.39` | `eno1` | `NO-CARRIER` | WLAN attiva |
| EMS `192.168.1.36` | `eth0` | `10.77.10.12/24`, 1000/full | WLAN attiva |

La targhetta fotografata ha confermato il modello Waveshare
`Gigabit-Switch-5P (EU)`. Il 2026-09-07 Blackbox e PFD sono stati collegati allo
switch e configurati sui profili NetworkManager `netplan-eth0`, senza gateway
o DNS sulla rete avionica e mantenendo la WLAN come accesso di recupero.

## Porte dello switch FLIGHT-DATA a cinque porte

Per il Gigabit-Switch-5P non gestito verificato:

| Porta | Collegamento iniziale | Nota |
|---|---|---|
| 1 | Blackbox `eth0` | broker centrale |
| 2 | Pixhawk 6X Ethernet | solo dopo backup/configurazione controllata |
| 3 | PFD RPi5 `eth0` | consumer DDP |
| 4 | EMS RPi4 `eth0` | consumer EDP V1 `10.77.10.12:49003`; feed banco SIM |
| 5 | Ground Station `eno1` | manutenzione di banco; futuro MFD in esercizio |

Non collegare le telecamere a questo switch. Quattro camere più VIDEO-CORE
richiedono almeno cinque porte e lascerebbero il data plane senza capacità né
separazione. Usare uno switch VIDEO separato da almeno sei porte. Anche il
FLIGHT-DATA definitivo richiede almeno sei porte operative per aggiungere MFD
e VIDEO-CORE; il cinque porte è soltanto il banco iniziale.

## Sequenza di migrazione senza lock-out

1. Etichettare switch, alimentazione e ogni cavo; verificare continuità e
   schermatura senza collegare ancora il Pixhawk.
2. Collegare Blackbox e PFD. Mantenere la WLAN accesa come recovery e assegnare
   gli indirizzi Ethernet statici senza gateway.
3. Verificare link 1000/full, MTU 1500, ping su `10.77.10.0/24` e assenza di una
   route predefinita su Ethernet.
4. Cambiare temporaneamente la destinazione DDP Blackbox in `10.77.10.11` e
   provare `LIVE → STALE → LOST → LIVE` scollegando soltanto il cavo Ethernet.
5. Collegare EMS e ripetere la prova senza interrompere il PFD.
6. Collegare Ground Station con `10.77.10.50/24`; non abilitarle forwarding o
   bridge con il Wi-Fi.
7. Solo dopo backup e diff dei parametri, configurare l'Ethernet Pixhawk e una
   porta UDP client MAVLink2 unicast verso `10.77.10.2:14550`.
8. Arrestare il bridge USB soltanto quando i contatori Blackbox e la provenienza
   recorder dimostrano che il flusso Ethernet è stabile.

## Profili NetworkManager proposti

Non eseguire i comandi finché il relativo cavo non è collegato e l'accesso WLAN
di recupero non è stato verificato.

Blackbox (modificare il profilo netplan esistente, senza crearne un duplicato):

```sh
sudo nmcli connection modify netplan-eth0 ipv4.method manual \
  ipv4.addresses 10.77.10.2/24 ipv4.gateway "" ipv4.never-default yes \
  ipv4.dns "" ipv6.method disabled 802-3-ethernet.mtu 1500
```

PFD (modificare il profilo netplan esistente, senza crearne un duplicato):

```sh
sudo nmcli connection modify netplan-eth0 ipv4.method manual \
  ipv4.addresses 10.77.10.11/24 ipv4.gateway "" ipv4.never-default yes \
  ipv4.dns "" ipv6.method disabled 802-3-ethernet.mtu 1500
```

Ground Station di banco:

```sh
sudo nmcli connection add type ethernet ifname eno1 con-name efis-flight-maint \
  ipv4.method manual ipv4.addresses 10.77.10.50/24 ipv4.never-default yes \
  ipv6.method disabled 802-3-ethernet.mtu 1500
```

I profili non definiscono DNS o gateway. Non creare `bridge`, NAT o Internet
Connection Sharing.

## Protezione egress fail-closed

Poiché la WLAN resta disponibile durante il collaudo come accesso di recupero,
la scomparsa della route connessa di `eth0` non deve permettere al traffico
FLIGHT-DATA di seguire il gateway predefinito. Installare sui nodi Linux:

```sh
sudo install -D -o root -g root -m 0644 \
  deploy/network/efis-avionics-egress.nft \
  /etc/efis/avionics-egress.nft
sudo install -o root -g root -m 0644 \
  deploy/systemd/efis-avionics-egress.service \
  /etc/systemd/system/efis-avionics-egress.service
sudo systemctl daemon-reload
sudo systemctl enable --now efis-avionics-egress.service
```

La tabella `inet efis_avionics` rifiuta e conta ogni pacchetto IPv4 destinato a
`10.77.10.0/24` quando l'interfaccia di uscita scelta non è `eth0`. Non cambia
la policy generale del firewall e non inoltra alcun pacchetto. Rollback:

```sh
sudo systemctl disable --now efis-avionics-egress.service
```

Il gate richiede tre prove: comunicazione invariata con `eth0` attiva; contatore
`reject` crescente e nessun pacchetto avionico sulla WLAN con `eth0` disattiva;
ripristino `LIVE` dopo la riattivazione. Il semplice fatto che il servizio sia
`active` non costituisce evidenza sufficiente.

## Tuning deterministico del link

Sul banco reale EEE risultava `enabled - active` sui tre Raspberry Pi. Con EEE
attivo sono stati osservati gap DDP e, sul RPi4 EMS, perdita ICMP del 13–15%.
Disabilitando EEE e riservando CPU0 agli interrupt Ethernet dell'EMS, la prova
correlata ha ottenuto 300/300 ping Blackbox-PFD con massimo 0,272 ms e 1.201
snapshot DDP senza nuovi gap in 60 secondi. Sul RPi4, con renderer fermo oppure
confinato a CPU1–3, i ping EMS hanno avuto massimo 0,309 ms; senza affinità si
osservavano burst fino a 2.040 ms.

Installare sui tre nodi verificati:

```sh
sudo install -o root -g root -m 0644 \
  deploy/systemd/efis-avionics-link-tuning.service \
  /etc/systemd/system/efis-avionics-link-tuning.service
sudo systemctl daemon-reload
sudo systemctl enable --now efis-avionics-link-tuning.service
```

Sul solo RPi4 EMS installare inoltre il drop-in:

```sh
sudo install -d -o root -g root -m 0755 \
  /etc/systemd/system/efis-ems.service.d
sudo install -o root -g root -m 0644 \
  deploy/systemd/efis-ems-performance.override.conf \
  /etc/systemd/system/efis-ems.service.d/performance.conf
sudo systemctl daemon-reload
sudo systemctl restart efis-ems.service
```

`systemctl disable --now efis-avionics-link-tuning.service` riabilita EEE come
rollback. Questi risultati sono prove di banco brevi, non sostituiscono il gate
di endurance o la qualifica dello switch/cablaggio.

## Parametri Pixhawk — valori proposti, non ancora autorizzati

ArduPlane 4.7 supporta il framework Ethernet sul Pixhawk 6X. La configurazione
prevista è statica e unicast:

- `NET_ENABLE=1`, `NET_DHCP=0`;
- `NET_IPADDR=10.77.10.3`, `NET_NETMASK=24`;
- nessun traffico fuori subnet;
- una porta `NET_Px_TYPE=1` UDP client;
- destinazione `10.77.10.2`, porta `14550`, protocollo MAVLink2.

Non scrivere questi parametri dal bridge EFIS. La modifica deve avvenire in una
sessione manutentiva separata con QGroundControl/MAVProxy, backup precedente,
elenco esatto delle sole differenze, riavvio, read-back e rollback provato.
I nomi/valori definitivi vanno letti direttamente dal firmware ArduPlane 4.7.0
installato: non si assume che un numero di porta logica sia libero.

## Gate di accettazione

- link cablato stabile su ogni nodo e nessun `NO-CARRIER`;
- zero default route e zero forwarding sul data plane;
- nessun pacchetto PFD/EMS diretto al Pixhawk;
- provenienza Pixhawk reale e `simulated=0` nel recorder;
- perdita, riordino, jitter e latenza misurati per almeno 60 minuti;
- scollegamento di un display senza perdita di recorder o dell'altro display;
- perdita Blackbox annunciata, senza alcun effetto sul controllo ArduPlane;
- quattro video sotto carico su rete separata prima di collegare VIDEO-CORE al
  dominio display.
