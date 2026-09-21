# CHECKPOINT — gate golden rerender Linux/Qt

Data: 2026-09-21  
Branch: `test/golden-rerender-linux-gate-v1`  
ID matrice: `PRJ-004`  
Classificazione: verifica software, non acceptance hardware

## Implementato

- job Core CI dedicato `linux-qt-golden` su Ubuntu 24.04;
- toolchain Qt 6.8.2 con Quick 3D, Quick Timeline e Shader Tools;
- build obbligatoria di `efis-pfd-qt` con `EFIS_REQUIRE_QT_RENDERER=ON`;
- baseline Linux distinta in
  `tests/golden/ems-responsive-states-linux`;
- matrice EMS completa: sei stati per tre risoluzioni, 18 PNG;
- confronto dei pixel decodificati con soglie fail-closed;
- tolleranza esplicita del solo rumore minimo ±1 su canale 8-bit; differenze
  da ±2 restano soggette alle soglie aggregate strette e picchi oltre 8
  falliscono sempre;
- correzione del percorso Python `--executable`, che in precedenza verificava
  soltanto matrice e ordine dei manifesti dopo il rerender;
- test unitario che dimostra il rifiuto di un'immagine visivamente diversa a
  parità di dimensioni e chiave semantica.

## Provenienza della baseline

- Core CI run: `#131`;
- commit sorgente: `39e4a280c1f3047882a4c4d97adc82bde0fc038f`;
- piattaforma manifesto: `offscreen`;
- artifact: `ems-responsive-states-linux-candidate`;
- SHA-256 ZIP:
  `632634d66d82c4e65a8993e5de8bc610a8c466672c7e11bce5e010f442be7626`;
- stato dichiarato dal manifesto:
  `SOFTWARE_REFERENCE_NOT_HARDWARE_ACCEPTANCE`.

Il digest scaricato coincide con quello pubblicato da GitHub Actions. Le 18
immagini e il manifesto sono presenti; un campione LIVE/STALE/FAIL è stato
ispezionato visivamente. La reference fotografa anche limiti grafici correnti
alle risoluzioni compatte e non li trasforma in approvazione estetica.

Il primo rerender bloccante (run #132) ha rilevato nel solo frame FAIL
1024×600 una variazione massima di un livello 8-bit (`max=1`). La policy è
stata resa esplicita e testata per accettare esclusivamente questo rumore
minimo di rasterizzazione, senza allargare le soglie per drift maggiori.

I run #133 e #134 hanno poi isolato una deriva ripetibile esclusivamente nei
frame `LOST` e `FAIL`. L'artifact diagnostico del run #134 ha SHA-256
`b4ac5ed1e103b9a832dfb815580098dc7013d0c1addbf716844c61088e9e6ef4`;
il digest locale coincide con quello pubblicato da GitHub Actions. Il confronto
pixel ha confinato tutte le differenze al rettangolo CAS superiore
(`x=688..1015`, `y=74..107` a 1024×600), mentre LIVE, STALE, DEGRADED e SIM
sono risultati identici. La causa è il lampeggio asincrono del banner WARNING,
non una deriva del renderer o degli strumenti.

La fixture `HmiGoldenMain.qml` attiva ora `goldenMode`; in tale modalità viene
congelata soltanto l'animazione del banner CAS. Il lampeggio operativo resta
attivo nell'applicazione normale. La correzione è coperta dal test di contratto
QML e non modifica la logica degli allarmi.

I sei frame LOST/FAIL aggiornati provengono dal run #135 e dal relativo
artifact `ems-responsive-states-linux-failed-rerender`, SHA-256
`b7567fb0a2e32067e9e6a2ad14cef5a4b1d4bc8e5af665c0bde52105851de0c9`.
Gli altri dodici frame sono rimasti pixel-identici alla baseline #131.

## Verificato solo software

- compilazione e link del renderer Qt su runner Linux x86-64;
- avvio headless con Mesa/OpenGL software;
- generazione deterministica della matrice e controllo dei digest;
- confronto pixel del rerender contro la reference Linux.

## Non verificato

- Raspberry Pi OS/ARM64, VideoCore e driver reali;
- display 800×480, 1024×600 e 1280×800 fisici;
- touch, leggibilità, sunlight/night, frame-rate e temperatura;
- equivalenza grafica fra Mesa CI e RPi4/RPi5;
- qualunque idoneità o approvazione al volo.

`flight_approval=false` resta invariato. Il prossimo gate autorizzabile è il
harness HMI sui pannelli reali con evidence bundle firmato.
