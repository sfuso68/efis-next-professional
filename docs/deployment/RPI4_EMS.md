# Raspberry Pi 4 — nodo EMS

Il RPi4 ospita l'EFIS vecchio. Questa procedura affianca il nodo motore di EFIS
Next senza disinstallarlo: cartella distinta, unità systemd distinta, il vecchio
impianto resta al suo posto e riavviabile. Il README dichiara che i progetti
legacy restano separati e operativi, e qui quella frase è una procedura, non
un'intenzione.

## Il nodo com'è davvero

Rilevato il 21 agosto 2026 collegandosi alla macchina. Vale la pena leggerlo
prima di installare, perché tre di questi punti smentiscono ciò che si
darebbe per scontato venendo dal RPi5.

| | |
|---|---|
| Accesso | `pi@192.168.1.48`, hostname `efis`, alias `efis.local` |
| Hardware | Raspberry Pi 4 Model B Rev 1.5, 4 core, 3,7 GiB |
| Sistema | Debian 13 trixie, aarch64, gcc 14.2, cmake 3.31 |
| Pannello | DSI collegato (`card1-DSI-1`); HDMI entrambi scollegati |
| Target di default | `multi-user.target` — **nessuna sessione grafica** |
| Qt | 6.8.2 già installato, con i moduli CMake che servono |
| `ninja` | **assente** |
| `sudo` | **richiede password** |

Le conseguenze, in ordine di importanza:

1. **Non c'è Wayland e non c'è X.** Il nodo disegna con `eglfs`, cioè
   direttamente su KMS/DRM. È anche ciò che fa l'EFIS vecchio.
2. **Non serve installare nulla con apt.** Qt 6.8.2 c'è già, e siccome `sudo`
   chiede la password è anche un bene: l'installazione non richiede root fino
   al momento del servizio.
3. **Si costruisce con i Makefile**, non con Ninja, perché Ninja non c'è e
   installarlo richiederebbe root per nulla.

## Chi possiede lo schermo

Non è una domanda aperta su questo nodo: è un vincolo. L'EFIS vecchio è
`efis.service`, un'applicazione Qt che gira come `pi` con
`QT_QPA_PLATFORM=eglfs` da `/home/pi/efis2/efis`. **eglfs prende il DRM master
in esclusiva**, quindi i due programmi non possono tenere il pannello insieme.
Non si tratta di un layout che si sovrappone: il secondo che parte non ottiene
il display.

Gli altri due servizi dell'impianto vecchio, `efis-web.service` (monitor web di
sola lettura sulla porta 8080, che legge il bus `can1`) e `efis-plan.service`,
**non toccano il pannello** e possono restare attivi accanto al nodo motore.

Decisione presa il 21 agosto 2026: il nodo EMS prende il display. `efis.service`
viene disabilitato all'avvio, `efis-ems.service` abilitato al suo posto. L'EFIS
vecchio resta installato e riavviabile a mano, dopo aver fermato il nodo motore.

L'unità **non** dichiara `Conflicts=efis.service`: la mutua esclusione resta a
carico di chi la ricorda, ed è stata una scelta esplicita. Chi cambiasse idea
trova qui la ragione per cui aggiungerla sarebbe difendibile.

## Sorgenti e build

Il progetto va in `~/efis-next`, che non è né `~/efis` né `~/efis2`.

Dalla macchina di sviluppo:

```sh
git archive --format=tar HEAD | ssh pi@192.168.1.48 "mkdir -p ~/efis-next && tar -x -C ~/efis-next"
```

Sul nodo, senza root:

```sh
cd ~/efis-next && cmake -S . -B build/dev -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug -DEFIS_BUILD_QT_RENDERER=OFF
```

```sh
cd ~/efis-next && nice -n 10 cmake --build build/dev -j3 && (cd build/dev && ctest)
```

`EFIS_BUILD_QT_RENDERER=OFF` è deliberato: su questo nodo il PFD completo non si
costruisce e non deve costruirsi. Chiederlo qui significherebbe installare
Quick3D per non usarlo.

Build e test in un'unica invocazione: separarli permette di provare un binario
compilato da un sorgente diverso. `-j3` su quattro core con `nice` tiene la
macchina fredda; il 21 agosto 2026 la build completa è arrivata a 66,7 °C con
`throttled=0x0`.

Un avviso è atteso e va ignorato: il Qt di Debian ne emette una dozzina della
forma *«The qml plugin 'qtquick2plugin' is a dependency of `efis-ems-qt`, but
the link target it defines does not exist in the current scope. The plugin will
not be linked»*. Riguarda il collegamento statico dei plugin, che su un Qt a
librerie condivise non serve. Verificato sul RPi5, dove compare identico anche
per `efis-pfd-qt`, undici volte per ciascuno dei due, e dove entrambi girano.
La prova che sia innocuo è che il binario parta, non che l'avviso sparisca.

L'avviso che invece **deve fermare** è `efis-ems-qt NON verra' costruito`, che
elenca i componenti Qt mancanti. Leggerlo, non aggirarlo.

## Prima prova sul pannello

Richiede root, perché bisogna liberare il display:

```sh
sudo systemctl stop efis.service
```

Poi, come `pi`:

```sh
cd ~/efis-next && ./deploy/scripts/run-ems.sh
```

Ciò che va verificato, in questo ordine:

1. le cinque lancette primarie si muovono — RPM, manifold, fuel flow, oil temp,
   oil press;
2. `SIMULATE NEXT FAULT` percorre i cinque scenari della
   `docs/protocols/EMS_SIMULATION_V1.md` e riporta `NONE`;
3. il piede della pagina dichiara `DATA NON-CERTIFIED • NOT FOR FLIGHT`. Non è
   decorazione: nessun valore di questo nodo descrive un motore reale.

Ritmo di rendering, in `logs/ems.log`:

```sh
grep render_health ~/efis-next/logs/ems.log | tail -5
```

Il nodo aggiorna il motore a 20 Hz e non ha terreno da disegnare: `fps` molto
sotto 20 è un risultato da indagare, non da accettare.

Su `eglfs` non esiste un compositore, quindi **`grim` e gli altri strumenti di
cattura Wayland non funzionano** su questo nodo: la verifica dell'interfaccia
qui si fa guardando il pannello, o aggiungendo una cattura interna al
programma. È la differenza pratica più fastidiosa rispetto al RPi5.

## Servizio

Unità di sistema, non utente: è la forma che la macchina usa già per
`efis.service`, e un servizio utente avrebbe richiesto in più
`loginctl enable-linger` per partire senza login.

```sh
sudo install -m 644 ~pi/efis-next/deploy/systemd/efis-ems.service /etc/systemd/system/
```

```sh
sudo systemctl daemon-reload && sudo systemctl disable --now efis.service && sudo systemctl enable --now efis-ems.service
```

Manutenzione:

```sh
systemctl status efis-ems.service
```

```sh
journalctl -u efis-ems.service -n 100 --no-pager
```

## Tornare all'EFIS vecchio

Va saputo prima di partire, non dopo. Il nodo motore si ferma e l'impianto
vecchio riprende il pannello:

```sh
sudo systemctl disable --now efis-ems.service && sudo systemctl enable --now efis.service
```

Nulla dell'EFIS vecchio è stato rimosso o modificato: cambia solo quale dei due
è abilitato all'avvio.

## Che cosa si può modificare da qui

La pagina motore che questo nodo carica è **lo stesso file** che il PFD del RPi5
carica: `qml/AdaptiveEmsView.qml` e le sue dipendenze appartengono a entrambi i
programmi. Una modifica fatta qui è già nel display completo.

La conseguenza vale in tutti e due i versi: chi cambia quella pagina cambia due
programmi, e li deve verificare entrambi. La ADR-006 lo pone come vincolo.

## Vincoli di banco

- l'EFIS vecchio non si disinstalla e non si sposta; si disabilita soltanto
  all'avvio, ed è reversibile con una riga;
- `efis-web.service` ed `efis-plan.service` restano attivi e non vanno toccati;
- nessuna porta SSH esposta fuori dalla LAN;
- il nodo usa solo dati simulati finché non viene dichiarato diversamente;
- questo è un banco di sviluppo, non un'immagine di produzione.
