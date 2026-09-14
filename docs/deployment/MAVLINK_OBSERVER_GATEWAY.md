# EFIS NEXT — gateway osservatori MAVLink sulla Blackbox

Stato: **IMPLEMENTATO SOFTWARE / NON INSTALLATO / BENCH ONLY**

Data: 2026-09-14

## Risultato previsto

Il Pixhawk resta collegato via Ethernet al Waveshare FLIGHT-DATA e non deve più
dipendere dalla USB del portatile. La Blackbox riceve il flusso operativo come
prima e ne invia due copie monitor-only:

| Sistema | Rete di servizio | Endpoint |
|---|---:|---:|
| Portatile Debian / QGroundControl | `192.168.1.39` | UDP `14550` |
| Server `serverha` | `192.168.1.6` | UDP `14550` |

Questa funzione non è un bridge fra reti. `ip_forward`, bridge L2 e NAT devono
restare assenti. Le risposte dei programmi non vengono lette dal fan-out e non
possono raggiungere Pixhawk.

## Preflight della Blackbox

Prima dell'installazione salvare l'output completo:

```bash
hostname
ip -br address
ip route get 10.77.10.3
ip route get 192.168.1.39
ip route get 192.168.1.6
sysctl net.ipv4.ip_forward
bridge link
sudo nft list ruleset
systemctl status efis-blackbox-broker.service --no-pager -l
```

Risultati necessari:

- Pixhawk raggiunto da `eth0` con sorgente `10.77.10.2`;
- portatile e server raggiunti dalla WLAN di servizio;
- `net.ipv4.ip_forward = 0`;
- nessun bridge contenente `eth0` o `wlan0`;
- broker attivo e recorder disponibile.

## Configurazione broker

Gli argomenti nuovi sono:

```text
--mavlink-bind 10.77.10.2
--mavlink-port 14550
--mavlink-source 10.77.10.3
--mavlink-observer 192.168.1.39
--mavlink-observer 192.168.1.6
--mavlink-observer-port 14550
```

Se sono configurati osservatori, il broker rifiuta il bind wildcard e richiede
una sorgente esplicita. Datagrammi provenienti da indirizzi diversi dal
Pixhawk vengono contati in `mavlink_source_rejected` e non sono né ingeriti né
copiati.

Installare binario, unità e file ambiente con il normale meccanismo di release
immutabile. Prima del riavvio:

Il file già presente `/etc/efis/blackbox-broker.env` ha precedenza sui default
dell'unità. Aggiornarlo esplicitamente e verificarne i permessi
`0640 root:blackbox`; un vecchio `EFIS_MAVLINK_BIND=0.0.0.0` farà fallire
l'avvio in sicurezza quando gli osservatori sono abilitati. Le righe richieste
sono:

```text
EFIS_MAVLINK_BIND=10.77.10.2
EFIS_MAVLINK_PORT=14550
EFIS_MAVLINK_SOURCE=10.77.10.3
EFIS_MAVLINK_OBSERVER_ARGS=--mavlink-observer 192.168.1.39 --mavlink-observer 192.168.1.6
EFIS_MAVLINK_OBSERVER_PORT=14550
```

Poi installare le protezioni e riavviare:

```bash
sudo install -m 0644 deploy/network/99-efis-no-ip-forwarding.conf \
  /etc/sysctl.d/99-efis-no-ip-forwarding.conf
sudo install -m 0644 deploy/network/efis-avionics-egress.nft \
  /etc/efis/avionics-egress.nft
sudo sysctl --system
sudo systemctl restart efis-avionics-egress.service
sudo systemd-analyze verify /etc/systemd/system/efis-blackbox-broker.service
sudo systemctl daemon-reload
sudo systemctl restart efis-blackbox-broker.service
sudo systemctl status efis-blackbox-broker.service --no-pager -l
```

## Collaudo ricezione

Sul portatile chiudere il vecchio bridge USB e scollegare il cavo USB Pixhawk.
In QGroundControl aprire l'ascolto UDP sulla porta 14550. La connessione può
mostrare telemetria, ma le funzioni che richiedono risposte o scritture devono
fallire: è il comportamento previsto.

Sul server, per una prova senza dipendenze aggiuntive:

```bash
python3 - <<'PY'
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("0.0.0.0", 14550))
for index in range(20):
    data, source = s.recvfrom(65535)
    print(index + 1, len(data), source, hex(data[0]) if data else "empty")
PY
```

Un primo byte `0xfd` identifica MAVLink2. Questa osservazione non sostituisce
la verifica CRC o l'audit del broker.

## Collaudo fail-closed

Sulla Blackbox controllare:

```bash
journalctl -u efis-blackbox-broker.service -n 100 --no-pager
ss -lunp
sysctl net.ipv4.ip_forward
sudo tcpdump -ni eth0 'src host 10.77.10.2 and dst host 10.77.10.3 and udp'
```

Durante un tentativo di richiesta parametri dal portatile la cattura finale
deve restare vuota: il broker non possiede un receiver LAN né un sender verso
Pixhawk. Contemporaneamente devono continuare a crescere recorder, DDP/EDP e
`blackbox_mavlink_observer_health` senza failure.

Spegnere a turno QGroundControl, portatile, server e WLAN. Nessuna di queste
perdite deve arrestare il broker o modificare il flusso Pixhawk→Blackbox.

## Rollback

Ripristinare il precedente binario immutabile e la precedente unità systemd,
quindi rimuovere soltanto gli argomenti `--mavlink-observer*` e
`--mavlink-source`. Non riabilitare il vecchio inoltro USB verso la Blackbox:
creerebbe due copie indistinguibili. Se necessario, la USB può restare
temporaneamente locale al solo portatile come già documentato.

## Manutenzione bidirezionale non implementata

Non usare `iptables`, `nftables`, `socat`, MAVProxy o un bridge improvvisato per
aggirare il limite. La futura modalità di manutenzione dovrà essere separata,
temporizzata, auditata e protetta da un interlock fisico a terra. Fino ad
allora, le operazioni di configurazione Pixhawk richiedono una sessione locale
controllata con backup, diff, read-back e rollback.
