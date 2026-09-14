# EFIS NEXT — gateway osservatori MAVLink sulla Blackbox

Stato: **IMPLEMENTATO E PROVATO SU BANCO / NON FLIGHT APPROVED**

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
QGroundControl ascolta UDP sulla porta 14550. La connessione può mostrare
telemetria, ma le funzioni che richiedono risposte o scritture devono fallire:
è il comportamento previsto.

### QGroundControl permanente MONITOR ONLY

L'unità utente `efis-qgc-monitor.service` è legata a
`graphical-session.target`; sul portatile `Linger=no` resta intenzionale, così
l'applicazione grafica non parte senza login. Prima di ogni avvio il preflight
`qgc_monitor_preflight.py` blocca QGC con exit 78 quando rileva almeno uno di
questi casi:

- qualsiasi `/dev/ttyACM*` o identità seriale Pixhawk/ArduPilot/PX4/FMU;
- il vecchio `efis-mavlink-readonly-bridge.service` attivo;
- UDP `0.0.0.0:14550` già occupata.

Il risultato atomico viene scritto in
`~/.local/state/efis-next/qgc-monitor-preflight.json`. Il preflight non apre
connessioni e non invia MAVLink. Questi controlli evitano configurazioni locali
ambigue; la garanzia di nessun ritorno verso Pixhawk resta strutturale nella
Blackbox TX-only, non nell'etichetta dell'interfaccia QGC.

Per conservare l'hardening `NoNewPrivileges`, QGC non viene avviato montando
l'AppImage con FUSE. Estrarre una sola volta la versione approvata:

```bash
cd ~/EFIS-ArduPilot/qgroundcontrol
mkdir -m 0700 .extract-qgc-5.0.8
cd .extract-qgc-5.0.8
../QGroundControl-5.0.8-x86_64.AppImage --appimage-extract >/dev/null
mv squashfs-root ../QGroundControl-5.0.8-x86_64.AppDir
cd ..
rmdir .extract-qgc-5.0.8
```

Installare quindi i file mantenuti dal repository:

```bash
install -d -m 0755 ~/.local/lib/efis-next ~/.config/systemd/user \
  ~/.local/share/applications
install -d -m 0700 ~/.local/state/efis-next
install -m 0755 tools/groundstation/qgc_monitor_preflight.py \
  ~/.local/lib/efis-next/qgc_monitor_preflight.py
install -m 0644 deploy/systemd/efis-qgc-monitor.user.service \
  ~/.config/systemd/user/efis-qgc-monitor.service
install -m 0644 deploy/desktop/efis-qgc-monitor.desktop \
  ~/.local/share/applications/efis-qgc-monitor.desktop
systemd-analyze --user verify \
  ~/.config/systemd/user/efis-qgc-monitor.service
systemctl --user daemon-reload
systemctl --user enable --now efis-qgc-monitor.service
```

Verificare `status=PASS`, servizio `active/running`, un solo processo in bind
su UDP 14550 e `NRestarts=0`. QGC 5.0.8 non ha risposto a SIGINT nel banco:
systemd forza perciò la chiusura dopo otto secondi. È un arresto limitato nel
tempo ma non graceful e resta una limitazione aperta. Anche il plugin Qt
`speechd` mancante è registrato come gap non bloccante; non nascondere il log e
non confonderlo con la futura catena voce EFIS.

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

### Consumer passivo persistente sul server

Il consumer previsto per `serverha` è
`tools/network/mavlink_observer_sink.py`. Accetta datagrammi esclusivamente da
`192.168.1.47`, verifica i confini dei frame MAVLink2 e pubblica soltanto
contatori aggregati in `health.json`. Non conserva payload, non possiede socket
di trasmissione e non è un relay verso Pixhawk o altri client.

Per il banco può essere installato come servizio utente:

```bash
install -d -m 0755 ~/.local/lib/efis-next ~/.config/systemd/user
install -d -m 0700 ~/.local/state/efis-next
install -m 0755 tools/network/mavlink_observer_sink.py \
  ~/.local/lib/efis-next/mavlink_observer_sink.py
install -m 0644 deploy/systemd/efis-mavlink-observer-sink.user.service \
  ~/.config/systemd/user/efis-mavlink-observer-sink.service
systemctl --user daemon-reload
systemctl --user enable --now efis-mavlink-observer-sink.service
```

Questa variante sopravvive ai riavvii soltanto se la sessione utente viene
avviata automaticamente oppure se l'amministratore abilita il linger:

```bash
sudo loginctl enable-linger sfuso
```

Per il servizio definitivo usare invece l'unità hardenizzata di sistema
`deploy/systemd/efis-mavlink-observer-sink.service`, dopo avere distribuito il
repository in `/opt/efis-next`. L'unità usa `DynamicUser`, directory di stato
dedicata e filtro IP systemd. Verificare sempre prima che nessun altro processo
usi `192.168.1.6:14550`.

Il firewall del server usa una tabella nftables indipendente, per non alterare
le tabelle `ip filter`/`ip nat` gestite da Docker, iptables-nft e Tailscale:

Con i tre file già predisposti in `/home/sfuso`, l'installazione verificata si
riduce a:

```bash
sudo /bin/sh /home/sfuso/install_mavlink_observer_firewall.sh
```

L'installatore effettua prima `nft --check` e `systemd-analyze verify`, quindi
abilita l'unità e stampa stato e tabella risultante. I passaggi equivalenti
manuali sono:

```bash
sudo install -d -m 0755 /etc/efis
sudo install -m 0644 deploy/network/efis-mavlink-observer-server.nft \
  /etc/efis/mavlink-observer-firewall.nft
sudo install -m 0644 deploy/systemd/efis-mavlink-observer-firewall.service \
  /etc/systemd/system/efis-mavlink-observer-firewall.service
sudo nft --check --file /etc/efis/mavlink-observer-firewall.nft
sudo systemd-analyze verify \
  /etc/systemd/system/efis-mavlink-observer-firewall.service
sudo systemctl daemon-reload
sudo systemctl enable --now efis-mavlink-observer-firewall.service
```

La sola regola di drop riguarda l'indirizzo locale `192.168.1.6`, UDP 14550 e
sorgenti diverse da `192.168.1.47`. Non crea hook `forward`/`output`, NAT o
regole generiche di accettazione. Dopo l'installazione verificare:

```bash
sudo nft list table inet efis_mavlink_observer
systemctl status efis-mavlink-observer-firewall.service --no-pager -l
```

Un datagramma di prova proveniente da un altro host deve incrementare il
contatore nft `drop` senza modificare `source_rejected` del sink; i contatori
MAVLink provenienti dalla Blackbox devono continuare a crescere.

Controlli minimi:

```bash
systemctl --user status efis-mavlink-observer-sink.service --no-pager -l
ss -lunp | grep ':14550'
cat ~/.local/state/efis-next/mavlink-observer-health.json
```

Il risultato richiesto è `mode=RX_ONLY_NO_RETURN_PATH`, `status=LIVE`, aumento
di `datagrams`/`frames`, heartbeat presenti e zero `source_rejected`,
`malformed` e `socket_errors`. Il controllo è strutturale: la convalida CRC e
semantica rimane responsabilità della pipeline Blackbox.

Al boot il servizio tollera per un massimo di 60 secondi il solo errore
`EADDRNOTAVAIL`, che si verifica quando systemd user parte prima
dell'assegnazione di `192.168.1.6`. Il numero di tentativi resta visibile in
`bind_retries`; ogni altro errore socket rimane immediatamente fatale. Il file
health è aggiornato ogni cinque secondi, mentre il journal riceve un riepilogo
ogni 60 secondi per evitare scritture continue non necessarie.

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
