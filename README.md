# EFIS Next

Terzo progetto indipendente per la nuova piattaforma EFIS. I due progetti legacy
rimangono separati e operativi.

## Milestone corrente: Gate 1

- core C++20 privo di dipendenze Qt;
- campioni con provenienza, tempi, qualità, validità e sequenza;
- snapshot di volo immutabili per i consumer;
- provider di simulazione deterministico;
- primo modello di health e degradazione;
- test senza framework esterni.
- Display Data Protocol V2 e view model indipendente dal renderer;
- renderer vettoriale SVG di riferimento per layout adattabili e golden image.

## Build

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Qt/QML e LVGL saranno adapter di presentazione: non entreranno nelle API del core.

## Renderer Qt Quick

Con Qt 6 MSVC installato:

```sh
cmake --preset dev -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/msvc2022_64
cmake --build --preset dev
./build/dev/efis-pfd-qt.exe
```

Sul target Linux/Wayland il kiosk fullscreen si abilita senza cambiare il codice:

```sh
EFIS_KIOSK=1 QT_QPA_PLATFORM=wayland ./build/dev/efis-pfd-qt
```

La procedura di avvio automatico RPi5 è documentata in
`docs/deployment/RPI5_PFD.md` e usa l'unità versionata
`deploy/systemd/efis-pfd.service`.
