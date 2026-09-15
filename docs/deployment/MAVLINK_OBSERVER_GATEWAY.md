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
`qgc_monitor_preflight.py` è eseguito come `ExecCondition` e blocca QGC con
exit 78 quando rileva almeno uno di questi casi:

- qualsiasi `/dev/ttyACM*` o identità seriale Pixhawk/ArduPilot/PX4/FMU;
- il vecchio `efis-mavlink-readonly-bridge.service` attivo;
- UDP `0.0.0.0:14550` già occupata;
- manifest, versione, AppRun, SHA-256 dell'AppImage o digest completo
  dell'AppDir non conformi.

Il risultato atomico viene scritto in
`~/.local/state/efis-next/qgc-monitor-preflight.json`. Il preflight non apre
connessioni e non invia MAVLink. Questi controlli evitano configurazioni locali
ambigue; la garanzia di nessun ritorno verso Pixhawk resta strutturale nella
Blackbox TX-only, non nell'etichetta dell'interfaccia QGC.

Non spostare il gate in `ExecStartPre`: su systemd l'uscita 78 da un pre-start
è una failure soggetta a `Restart=on-failure` e può generare un retry-loop.
Come `ExecCondition`, invece, la stessa uscita produce `Result=exec-condition`,
lascia l'unità inattiva e non incrementa `NRestarts`.

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
install -m 0755 tools/groundstation/qgc_appdir_digest.py \
  ~/.local/lib/efis-next/qgc_appdir_digest.py
install -m 0755 tools/groundstation/qgc_monitor_preflight.py \
  ~/.local/lib/efis-next/qgc_monitor_preflight.py
install -m 0644 deploy/groundstation/qgc-monitor-manifest.json \
  ~/.local/lib/efis-next/qgc-monitor-manifest.json
install -m 0644 deploy/systemd/efis-qgc-monitor.user.service \
  ~/.config/systemd/user/efis-qgc-monitor.service
install -m 0644 deploy/desktop/efis-qgc-monitor.desktop \
  ~/.local/share/applications/efis-qgc-monitor.desktop
systemd-analyze --user verify \
  ~/.config/systemd/user/efis-qgc-monitor.service
systemctl --user daemon-reload
systemctl --user enable --now efis-qgc-monitor.service
```

Il manifest V2 contiene anche il digest deterministico dell'albero AppDir:
nomi, tipi, bit eseguibili, destinazioni dei link simbolici e contenuto di
ogni file. Sul pacchetto approvato sono stati misurati 1.081 file, 74 link,
336 directory, 471.337.419 byte e SHA-256 albero
`34b0f67710f20ed380848185028dd8cbe5ad356caa1f99bfae7cc78a0f29271c`.
Il calcolo reale richiede circa 0,67 secondi e viene ripetuto prima di ogni
avvio.

Verificare `status=PASS`, servizio `active/running`, un solo processo in bind
su UDP 14550 e `NRestarts=0`. QGC 5.0.8 non ha risposto a SIGINT nel banco:
systemd forza perciò la chiusura dopo otto secondi. È un arresto limitato nel
tempo ma non graceful e resta una limitazione aperta. Le librerie Qt/speechd,
il socket, il servizio e le voci sono presenti. La diagnostica Qt ha
identificato il conflitto ABI: la GLib inclusa nell'AppDir non espone
`g_string_free_and_steal`, richiesto da `libspeechd.so.2` Debian 13. Il
manifest autorizza la GLib di sistema e il preflight ne verifica percorso e
simbolo; `/usr/bin/env LD_PRELOAD=...` la applica soltanto al processo QGC.
La prova reale ha caricato il plugin senza warning mantenendo immutata AppDir.
Questo non sostituisce la catena voce EFIS né costituisce acceptance audio in
cabina.

### Politica voce della Ground Station

La sintesi QGC sul portatile non fa parte della catena audio EFIS. Per scelta
del proprietario è disabilitata in modo persistente impostando
`audioMuted=true`, mascherando `speech-dispatcher.service` e relativo socket e
rimuovendo il preload speech dall'avvio operativo. Lo script riavvia soltanto
QGC e verifica il ritorno della telemetria:

```sh
sh deploy/scripts/disable_qgc_synthetic_speech.sh
```

Il gate richiede entrambi `masked/inactive`, il valore QGC mute e nessun
processo `speech-dispatcher`; continua inoltre a verificare QGC, listener e
zero restart. Il mute QGC comprende anche gli avvisi audio della Ground
Station. La futura voce naturale è una funzione EFIS distinta e viene
allocata alla Blackbox o a un nodo audio dedicato secondo ADR-014.

Nota di portabilità Debian 13: gli helper invocano `ss` tramite il `PATH` e non
presuppongono `/usr/sbin/ss` (sul laptop è `/usr/bin/ss`). Le regole nftables
che selezionano tutto UDP usano `meta l4proto udp`; il token `udp` da solo è
valido soltanto quando seguito da un'espressione di header come `dport`.

Per provare il gate porta senza dipendere da quoting shell, usare
`tools/network/udp_port_holder.py` come unico listener temporaneo. Il tool non
trasmette datagrammi, accetta una durata massima di dieci minuti e deve essere
terminato prima di riavviare QGC.

Il manifest autorizzato è parte del deployment e contiene versione, endpoint,
sorgente Blackbox, percorso AppRun, SHA-256 dell'AppImage originale e inventario
completo dell'AppDir. Il preflight ricalcola entrambi gli hash prima di ogni
avvio; non aggiornare i valori per far passare un file inatteso senza prima
inventariare provenienza, versione e rollback.

### Firewall Ground Station in difesa di profondità

La tabella `efis_qgc_monitor` è separata dalle policy di sistema e applica tre
soli divieti: sorgenti diverse dalla Blackbox verso UDP 14550, tutto il traffico
del portatile verso `10.77.10.0/24` e ogni UDP portatile→Blackbox. Non contiene
forward, NAT o regole generiche di accept e non blocca SSH/TCP sulla LAN di
servizio.

Prima dell'attivazione copiare l'intera directory di progetto sul portatile e
verificare lo staging. L'installazione richiede una console locale con sudo:

```bash
sudo deploy/scripts/install_qgc_monitor_firewall.sh
sudo nft list table inet efis_qgc_monitor
systemctl status efis-qgc-monitor-firewall.service --no-pager -l
```

L'installatore arma prima un rollback automatico di 300 secondi. Dopo la prova
esterna di ricezione e dei contatori, confermare entro la finestra soltanto con:

```bash
sudo /usr/local/sbin/commit_qgc_monitor_firewall
```

Se il comando non viene eseguito, `rollback_qgc_monitor_firewall` disabilita
esclusivamente il servizio dedicato e rimuove esclusivamente la tabella
`inet efis_qgc_monitor`; non esegue `flush ruleset`.

Subito dopo ripetere la ricezione QGC e una cattura negativa: le risposte UDP
generate da QGC devono incrementare soltanto il contatore output drop e non
devono comparire su `wlan0`; il flusso Blackbox→QGC deve continuare. Fino a
questa prova il verificatore operativo deve restituire `PARTIAL`.

### Verifica operativa e soak

`qgc_monitor_status.py` combina preflight, integrità artefatto, stato/riavvii
systemd, unicità del listener e firewall. Restituisce `FAIL` per un gate QGC
critico, `PARTIAL` quando QGC è corretto ma il firewall non è attivo e `PASS`
soltanto con tutti i gate verdi:

```bash
python3 tools/groundstation/qgc_monitor_status.py
```

Sulla Blackbox, `analyze_mavlink_observer_soak.py` analizza il journal del
broker e richiede contatori monotoni, progresso e zero errori per ingestion,
recorder, DDP, fan-out, ogni osservatore e ogni display. Esempio su 60 secondi:

```bash
journalctl -u efis-blackbox-broker.service --since '60 seconds ago' \
  -o cat --no-pager | \
python3 tools/network/analyze_mavlink_observer_soak.py \
  --input - --duration-seconds 60 --minimum-mavlink-hz 5 \
  --observer 192.168.1.39:14550 --observer 192.168.1.6:14550 \
  --display 10.77.10.11:49002 --display 10.77.10.13:49002
```

Il test di 60 secondi è un controllo funzionale breve, non sostituisce il soak
termico/endurance finale.

Per il controllo integrato retrospettivo usare
`blackbox_observer_acceptance.py`: produce atomicamente un report che unisce
soak, parità ingresso/uscite, copertura campioni, stato/restart del broker,
RSS, memoria disponibile, temperatura e flag di throttling. Il collector è
read-only e non possiede funzioni di trasmissione:

```bash
python3 tools/network/blackbox_observer_acceptance.py \
  --duration-seconds 600 --minimum-sample-coverage 0.9 \
  --observer 192.168.1.39:14550 --observer 192.168.1.6:14550 \
  --display 10.77.10.11:49002 --display 10.77.10.13:49002 \
  --output ~/.local/state/efis-next/blackbox-observer-acceptance.json
```

Il 2026-09-15 il collector reale ha restituito `PASS`: copertura 97,33%,
26,247 Hz MAVLink, parità esatta verso entrambi gli osservatori e display,
zero errori/restart, 48,3 °C, throttling `0x0`, 3.813.163.008 byte di memoria
disponibile e RSS broker 4.341.760 byte. Dieci minuti restano una prova
estesa di banco, non l'endurance/termica finale.

Una successiva finestra retrospettiva di 3.600 secondi è anch'essa `PASS`:
3.506 campioni (97,39%), 26,279 Hz, +94.604 frame per ciascun observer,
+71.976 record per ciascun display, parità esatta e zero errori/restart;
47,7 °C e throttling `0x0` al controllo finale.

I gate reboot vengono armati prima dell'azione privilegiata e verificati dopo
il ritorno del nodo:

```bash
python3 ~/.local/lib/efis-next/qgc_reboot_acceptance.py arm
# sudo reboot; effettuare login grafico
python3 ~/.local/lib/efis-next/qgc_reboot_acceptance.py check

python3 ~/.local/lib/efis-next/server_observer_reboot_acceptance.py arm
# sudo reboot
python3 ~/.local/lib/efis-next/server_observer_reboot_acceptance.py check
```

Il gate laptop ignora intenzionalmente il firewall rinviato, ma richiede
manifest/preflight, QGC, listener, zero restart, speech pulito e `Linger=no`.
Il gate server richiede nuovo boot ID, sink RX-only LIVE/fresco, firewall,
listener, `Linger=yes`, endpoint Caddy protetto e tutti i dodici container.

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

Per il fault controllato della sola rete Ground Station, copiare
`tools/network/fault_laptop_wifi.sh` sul portatile ed eseguirlo dalla console
locale, non via SSH:

```bash
sudo ./fault_laptop_wifi.sh wlo1 CASA 45
```

Lo script verifica interfaccia e connessione attive, accetta solo durate fra
20 e 180 secondi e programma nel system manager root il comando di recupero
prima di disconnettere il Wi-Fi. Il timer dedicato è
`efis-wifi-recovery.timer`. Durante i 45 secondi acquisire dalla Blackbox la
crescita di ingestion, recorder, DDP/EDP e fan-out server; sul server verificare
sink `LIVE` e contatori crescenti. Dopo il recupero controllare indirizzo
`192.168.1.39`, QGC ancora attivo e ricezione ripresa. Questa prova non
sostituisce lo spegnimento fisico completo del portatile.

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
