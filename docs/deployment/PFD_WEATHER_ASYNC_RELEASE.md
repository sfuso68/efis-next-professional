# PFD weather async — immutable ARM64 rollout

Stato: **PREPARATO SOFTWARE / NON ESEGUITO SU HARDWARE**
Prodotto: **DEVELOPMENT/NON-CERT** — `flight_approval=false`

## Scopo e confini

Questa procedura distribuisce sul PFD RPi5 la correzione che sposta la
compressione e il commit dei frame meteo simulati fuori dal thread grafico.
La release candidata e' bloccata al merge:

- commit: `3c7bb4d5495e1ce9f582dd185be48b9563993e48`;
- release: `pfdrec-3c7bb4d-weather-async-v1`;
- rollback: `pfdrec-7051a47-retention-v1`;
- ruoli: `PFD` e `RECORDER` sullo stesso albero immutabile;
- piattaforma: `RPI5_AARCH64`.

La preparazione non attiva nulla. L'attivazione richiede una finestra banco,
privilegi root e accesso locale/SSH al nodo `efis-pfd-01`. Non eseguire in
volo e non usare questi esiti come approvazione aeronautica.

## 1. Creare e trasferire l'archivio esatto

Da un clone pulito e aggiornato:

```bash
git archive --format=tar.gz --prefix=efis-next-3c7bb4d/ \
  -o efis-next-3c7bb4d5495e1ce9f582dd185be48b9563993e48.tar.gz \
  3c7bb4d5495e1ce9f582dd185be48b9563993e48
sha256sum efis-next-3c7bb4d5495e1ce9f582dd185be48b9563993e48.tar.gz
```

SHA-256 atteso:
`beb4cce4eed80e950dda6e245ff3bae42842f96242f4836cddeb5fe7310c61f3`.

Sul PFD creare `/home/efis/efis-release-pfd-weather-stage/` e copiarvi
l'archivio e i tre script di questa procedura.

## 2. Preparare senza attivare

Come utente `efis`:

```bash
bash /home/efis/prepare_pfd_weather_async_release.sh
```

Il gate controlla nodo, architettura, release attiva, hash dell'archivio e
riserva minima di 12 GiB; compila nativamente PFD, recorder e test meteo,
esegue il test mirato, genera il manifest dual-role e termina con:

```text
PFD_WEATHER_ASYNC_RELEASE_PREPARATION=PASS
EXECUTION_AUTHORIZED=false
NEXT_REQUIRED=ROOT_ACTIVATION_WINDOW
```

Un PASS qui e' solamente build/test ARM64; non equivale ad attivazione.

## 3. Attivazione controllata e rollback reale

Solo in una finestra banco autorizzata:

```bash
sudo bash /home/efis/activate_pfd_weather_async_release.sh
```

Lo script:

1. pretende che la release attiva sia ancora il rollback noto;
2. installa e rende immutabile la candidata;
3. verifica manifest, processi, DDP e recorder;
4. torna realmente alla release precedente e ne verifica il runtime;
5. riattiva la candidata;
6. inietta temporaneamente 400 ms di I/O nel writer meteo;
7. richiede l'evento `weather_simulation_worker_slow` e rifiuta qualsiasi
   `callback_stall callback=weather_simulation`;
8. rimuove l'iniezione, riavvia il PFD e ripete il controllo.

Qualsiasi errore rimuove il drop-in diagnostico e ripristina il rollback. Il
PASS atteso e':

```text
PFD_WEATHER_ASYNC_ACTIVATION=PASS
NEXT_REQUIRED=REBOOT_ACCEPTANCE
```

## 4. Reboot acceptance

Dopo riavvio fisico del PFD:

```bash
sudo bash /home/efis/verify_pfd_weather_async_reboot.sh
```

Il controllo e' read-only rispetto alla release: convalida manifest, binari
effettivi, servizi, DDP, avanzamento recorder, throttling e assenza dello
stall meteo in una finestra di 30 secondi. Il PASS atteso e':

```text
PFD_WEATHER_ASYNC_REBOOT_ACCEPTANCE=PASS
```

## Evidenze e gate successivi

Le evidenze runtime vengono raccolte in
`/home/efis/.local/state/efis-next/release-pfd-weather-async-20260930/`.
Dopo il reboot occorre copiarle nel repository, rieseguire il controllo stato
e aggiornare `PFD-001`. Restano separati e aperti: soak retention >=24 h,
startup retry, acceptance visuale/touch/terrain e 27 gap imagery.
