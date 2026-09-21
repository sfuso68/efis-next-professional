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
