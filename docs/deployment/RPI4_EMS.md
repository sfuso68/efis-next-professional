# Raspberry Pi 4 — nodo EMS

Il RPi4 ospita l'EFIS vecchio. Questa procedura affianca il nodo motore di EFIS
Next senza toccarlo: cartella distinta, unità systemd distinta, nessuna modifica
all'avvio esistente. Il README dichiara che i progetti legacy restano separati e
operativi, e qui quella frase è una procedura, non un'intenzione.

Nodo: `efis.local`, `192.168.1.48` al 21 agosto 2026 (DHCP, non stabile).

## Prima di installare: chi possiede lo schermo

L'EFIS vecchio e il nodo EMS sono entrambi applicazioni a schermo intero. Se
partono insieme si contendono il display e il risultato non è nessuno dei due.
Prima di abilitare l'unità, accertare come si avvia l'EFIS vecchio:

```sh
systemctl --user list-unit-files --state=enabled
systemctl list-unit-files --state=enabled | grep -i efis
ls ~/.config/autostart/ 2>/dev/null
```

Il nodo EMS si installa sempre, ma si **abilita** all'avvio solo dopo aver
deciso quale dei due impianti possiede lo schermo. Fino ad allora si avvia a
mano, che è anche il modo giusto di provarlo la prima volta.

## Prerequisiti

- Raspberry Pi OS / Debian Trixie 64-bit;
- Qt 6.8 o superiore con i moduli `Core Gui Qml Quick QuickControls2`;
- sessione grafica Wayland;
- **non** servono Quick3D, Network né Concurrent: il nodo EMS non li chiede, ed
  è la ragione per cui gira su questa macchina.

```sh
sudo apt update
sudo apt install --no-install-recommends \
    build-essential cmake ninja-build \
    qt6-base-dev qt6-declarative-dev qt6-wayland
```

I nomi dei pacchetti vanno confermati sulla macchina: l'unica prova che i moduli
ci sono è che `cmake` configuri senza emettere l'avviso `efis-ems-qt NON verra'
costruito`. Quell'avviso elenca ciò che manca — leggerlo, non aggirarlo.

## Sorgenti e build

Il progetto va in una cartella propria, `~/efis-next`, che non è quella
dell'EFIS vecchio.

```sh
cmake --preset dev -DEFIS_BUILD_QT_RENDERER=OFF
cmake --build --preset dev --target efis-ems-qt efis_engine_tests
ctest --preset dev
```

`EFIS_BUILD_QT_RENDERER=OFF` è deliberato: su questo nodo il PFD completo non si
costruisce e non deve costruirsi. Chiederlo qui significherebbe installare
Quick3D per non usarlo.

La build e i test si eseguono in una sola invocazione: separarli permette di
provare un binario compilato da un sorgente diverso.

## Prima prova, a mano

```sh
./deploy/scripts/run-ems.sh
```

Con la sessione grafica attiva compare la pagina motore a schermo intero. Ciò
che va verificato, in questo ordine:

1. le cinque lancette primarie si muovono — RPM, manifold, fuel flow, oil temp,
   oil press;
2. `SIMULATE NEXT FAULT` percorre i cinque scenari della
   `docs/protocols/EMS_SIMULATION_V1.md` e riporta `NONE`;
3. il piede della pagina dichiara `DATA NON-CERTIFIED • NOT FOR FLIGHT`. Non è
   decorazione: nessun valore di questo nodo descrive un motore reale.

Ritmo di rendering, nel journal o in `logs/ems.log`:

```sh
grep render_health logs/ems.log | tail -5
```

`fps` sotto 25 su questa pagina è un risultato da indagare, non da accettare: il
nodo aggiorna il motore a 20 Hz e non ha terreno da disegnare.

Salute termica del RPi4 sotto carico grafico continuo:

```sh
vcgencmd get_throttled   # deve restituire 0x0
vcgencmd measure_temp
```

## Servizio utente

```sh
install -d -m 755 ~/.config/systemd/user
install -m 644 deploy/systemd/efis-ems.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user start efis-ems.service      # solo avvio, non ancora automatico
```

L'avvio automatico si abilita **dopo** aver risolto la contesa dello schermo:

```sh
systemctl --user enable efis-ems.service
```

Manutenzione:

```sh
systemctl --user status efis-ems.service
journalctl --user -u efis-ems.service -n 100 --no-pager
systemctl --user restart efis-ems.service
systemctl --user stop efis-ems.service
```

L'unità usa `%h` invece di un percorso assoluto, quindi non contiene il nome
dell'utente di questa macchina e non va modificata per installarla. È legata a
`graphical-session.target`: non parte prima di Wayland e si ferma con la
sessione. `Restart=on-failure` evita il ciclo di riavvii dopo uno stop voluto.

## Che cosa si può modificare da qui

La pagina motore che questo nodo carica è **lo stesso file** che il PFD del RPi5
carica: `qml/AdaptiveEmsView.qml` e le sue dipendenze appartengono a entrambi i
programmi. Una modifica fatta qui è già nel display completo.

La conseguenza vale in tutti e due i versi: chi cambia quella pagina cambia due
programmi, e li deve verificare entrambi. La ADR-006 lo pone come vincolo, e la
verifica valida per l'interfaccia resta la cattura dal display reale — a
1024×600 su questo nodo, a 1024×600 sul PFD.

## Vincoli di banco

- l'EFIS vecchio non si disinstalla, non si disabilita e non si sposta;
- nessuna porta SSH esposta fuori dalla LAN;
- il nodo usa solo dati simulati finché non viene dichiarato diversamente;
- questo è un banco di sviluppo, non un'immagine di produzione.
