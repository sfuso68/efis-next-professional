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

## Vincoli di banco

- `vcgencmd get_throttled` deve restituire `0x0`;
- nessuna porta SSH esposta fuori dalla LAN;
- il renderer usa solo dati simulati finché non viene dichiarato diversamente;
- il servizio di sviluppo non è ancora un'immagine di produzione.
