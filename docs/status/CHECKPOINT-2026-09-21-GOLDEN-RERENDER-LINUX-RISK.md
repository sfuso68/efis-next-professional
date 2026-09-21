# CHECKPOINT — rischio golden rerender su Linux

Data: 2026-09-21  
Branch: `test/golden-rerender-linux-note-v1`  
Classificazione: rischio documentato, verifica software incompleta

## Situazione osservata

Il test CTest `efis_ems_golden_rerender_tests` viene registrato soltanto quando
esistono contemporaneamente il target `efis-pfd-qt` e la piattaforma `WIN32`.
Questa restrizione è intenzionale: il manifesto golden corrente dichiara una
reference Windows e un confronto diretto con font o backend grafici Linux
misurerebbe anche differenze di rasterizzazione della piattaforma.

La Core CI Linux configura invece il progetto con
`EFIS_BUILD_QT_RENDERER=OFF`. Di conseguenza verifica strumenti, manifesti e
digest committati, ma **non avvia il renderer Qt e non protegge dalle
regressioni pixel del target Linux/RPi**.

## Mitigazione introdotta

CMake emette ora un messaggio `STATUS` esplicito quando il gate non viene
registrato. Il messaggio identifica sia il vincolo della baseline Windows sia
l'assenza del rerender pixel nella CI Linux/core-only. Questo impedisce di
interpretare silenziosamente la semplice assenza del test come una copertura
positiva.

## Rischio residuo e traguardo separato

Il rischio resta aperto. Una modifica successiva dovrà:

1. costruire `efis-pfd-qt` su Linux in CI con backend e font congelati;
2. generare e revisionare una baseline Linux distinta dalla reference Windows;
3. eseguire il rerender della matrice completa e confrontare i pixel;
4. rendere il gate bloccante soltanto dopo una baseline stabile e approvata.

Questa modifica non crea baseline, non cambia golden esistenti e non modifica
renderer, hardware, release o deployment. Non costituisce acceptance del
pannello e non modifica `flight_approval=false`.
