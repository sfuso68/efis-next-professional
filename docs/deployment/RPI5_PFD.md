# Raspberry Pi 5 PFD deployment

Target di banco iniziale:

- Raspberry Pi OS Trixie 64-bit;
- Qt 6.8.2;
- Wayland/labwc;
- pannello MPI7002/MPI7003 1024x600 a 60 Hz;
- utente `efis`;
- progetto in `/home/efis/efis-next`.

## Servizio utente

Installazione:

```sh
install -d -m 755 ~/.config/systemd/user
install -m 644 deploy/systemd/efis-pfd.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now efis-pfd.service
```

Manutenzione:

```sh
systemctl --user status efis-pfd.service
journalctl --user -u efis-pfd.service -n 100 --no-pager
systemctl --user stop efis-pfd.service
systemctl --user restart efis-pfd.service
```

Il servizio è legato alla sessione grafica: non parte prima di Wayland e viene
arrestato quando la sessione termina. `Restart=on-failure` evita loop di riavvio
dopo uno stop volontario.

## Ricezione DDP V2 dalla Blackbox

Il servizio apre in sola ricezione UDP unicast la porta `49002` e accetta come
publisher il nodo logico Blackbox `47`. Le variabili sono intenzionalmente
esplicite nel file systemd:

```ini
Environment=EFIS_DDP_BIND_ADDRESS=0.0.0.0
Environment=EFIS_DDP_LISTEN_PORT=49002
Environment=EFIS_DDP_SOURCE_NODE=47
Environment=EFIS_SIMULATION_ROLE=FALLBACK
```

## Deployment atomico del renderer

Sul nodo operativo non eseguire direttamente un binario incrementale in
`build/dev`: una pulizia della directory può lasciare systemd con un processo
ancora vivo ma senza eseguibile riavviabile. Ogni release verificata vive in una
directory immutabile e il puntatore operativo è:

```text
/home/efis/efis-next-current -> /home/efis/efis-next-<release>
```

Installare `deploy/systemd/efis-pfd-release.override.conf` come
`~/.config/systemd/user/efis-pfd.service.d/release.conf`. Il drop-in avvia il
binario attraverso il puntatore stabile, configura il receiver DDP e scrive il
log nella release attiva. Per il rollback si cambia atomicamente il symlink
verso una release precedente già verificata, quindi si riavvia il servizio.

In sviluppo la simulazione resta soltanto fallback: quando DDP è `LIVE` vincono
i campioni reali trasportati dalla Blackbox; a collegamento assente o perso la
simulazione è visibile come tale. Per una prova di failure senza sostituzione
impostare `EFIS_SIMULATION_ROLE=DISABLED`.

Prova da banco, con dati dichiaratamente simulati inviati da un altro host:

```sh
./build/dev/efis-ddp-bench-publisher \
  --destination 192.168.1.45 --port 49002 --source-node 47 \
  --rate-hz 20 --seconds 60 --simulate
```

Il journal del PFD riporta `ddp_state`, pacchetti accettati/rifiutati, gap di
sequenza ed errori socket nel record periodico `render_health`. Il publisher da
banco richiede sempre `--simulate`: non è il processo di ingestione reale della
Blackbox e non deve essere installato come servizio di volo.

## Vincoli di banco

- `vcgencmd get_throttled` deve restituire `0x0`;
- nessuna porta SSH esposta fuori dalla LAN;
- il renderer usa solo dati simulati finché non viene dichiarato diversamente;
- il servizio di sviluppo non è ancora un'immagine di produzione.
