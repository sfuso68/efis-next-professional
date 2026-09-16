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

L'unità è collegata a `default.target`, perché nelle sessioni Wayland Debian
il target utente `graphical-session.target` può restare inattivo anche quando
il compositor e il display sono operativi. `Restart=always` mantiene il
tentativo di avvio durante il breve intervallo in cui il socket Wayland non è
ancora disponibile.

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

## Variante direct DRM/EGLFS

Il nodo storico `pi5` usa il renderer GLES direttamente sul DRM e non una
sessione Wayland. Su quel profilo usare
`deploy/systemd/efis-next-pfd-direct.service` e
`deploy/rpi5/eglfs-kms-card1.json`. Il file KMS seleziona esplicitamente
`/dev/dri/card1`: sul Raspberry Pi 5 verificato `card0` e' il solo device V3D,
mentre il connettore `HDMI-A-2` appartiene a `card1` (`vc4-drm`). Lasciare la
selezione automatica produce `drmModeGetResources failed` e nessuno schermo.

Installazione in staging, senza cambiare il renderer al boot:

```sh
sudo install -d -m 755 /etc/efis
sudo install -m 644 deploy/rpi5/eglfs-kms-card1.json /etc/efis/eglfs-kms.json
sudo install -m 644 deploy/systemd/efis-next-pfd-direct.service /etc/systemd/system/
sudo systemctl daemon-reload
```

L'unita' e' in conflitto esplicito con `fuso-sv3d.service`, con la sua variante
di simulazione e con LightDM: un solo processo puo' possedere il DRM master.
Non abilitarla al boot prima di avere chiuso ruolo del nodo, indirizzo
FLIGHT-DATA, pacchetto terrain/imagery e acceptance prestazionale. Il rollback
di banco e':

```sh
sudo systemctl disable --now efis-next-pfd-direct.service
sudo systemctl enable --now fuso-sv3d.service
```

### Profilo MFD/Canvas del secondo RPi5

Il nodo `mfd-rpi5-01` (`10.77.10.13`, WLAN manutentiva storica
`192.168.1.49`) usa la stessa applicazione e la stessa codebase del PFD, ma
parte dalla pagina `CANVAS`. Installare l'unità dedicata
`deploy/systemd/efis-next-mfd-direct.service`; essa è in conflitto sia con il
renderer legacy sia con il servizio PFD direct, così un solo processo può
possedere il DRM master.

Il profilo di unità del nodo è installato separatamente dal binario:

```sh
install -d -m 755 ~/.config/EFIS-NEXT
install -m 600 deploy/rpi5/mfd-rpi5-01-units.ini \
  ~/.config/EFIS-NEXT/ems-units.ini
```

`IMPERIAL` significa °F, PSI, pollici di mercurio, galloni e GPH nelle viste
motore/sistemi. Le grandezze aeronautiche principali restano espresse secondo
la simbologia delle rispettive pagine. Il file è configurazione del nodo e non
deve essere sostituito durante l'aggiornamento di una release.

La migrazione definitiva richiede questo ordine: compilare e testare una
release immutabile, selezionarla con `/home/pi/efis-next-current`, installare il
profilo unità, disabilitare i servizi legacy, abilitare il servizio MFD e
riavviare. Conservare il legacy fuori dal percorso di avvio fino al collaudo;
la sua cancellazione fisica non deve rimuovere `/home/pi/efis-data`, che è la
radice dati persistente riutilizzata da EFIS Next.

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

Prima di attivare una release, verificare che contenga `logs/`. Il drop-in
systemd apre `logs/pfd.log` prima di avviare il processo: se la directory manca,
il servizio fallisce senza eseguire il binario. Il repository conserva
`logs/.gitkeep` proprio per non perdere la directory durante `git archive`.

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

Il journal del PFD riporta nel record periodico `render_health`:

- `presentation_hz` e `presentation_interval_ms`, cioè quante immagini sono
  state realmente presentate. Non sono una misura del tempo impiegato dalla GPU;
- `snapshot_hz`, cioè la cadenza degli snapshot consumati dal controller;
- `ddp_hz`, `ddp_state`, pacchetti accettati/rifiutati, gap di sequenza ed
  errori socket.

Un valore basso di `presentation_hz` può significare che la scena non richiede
un nuovo frame; non dimostra da solo una saturazione grafica. Per profilare il
tempo reale del render thread usare temporaneamente `QSG_RENDER_TIMING=1` e
confrontare `syncAndRender` con il budget di 16,7 ms a 60 Hz. Il publisher da
banco richiede sempre `--simulate`: non è il processo di ingestione reale della
Blackbox e non deve essere installato come servizio di volo.

## Vincoli di banco

- `vcgencmd get_throttled` deve restituire `0x0`;
- nessuna porta SSH esposta fuori dalla LAN;
- il renderer usa solo dati simulati finché non viene dichiarato diversamente;
- il servizio di sviluppo non è ancora un'immagine di produzione.
