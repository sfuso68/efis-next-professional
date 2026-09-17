# Checkpoint — SkyView/EMS HMI Contract V1 — 2026-09-17

## Implementato offline

- Congelati dieci gate grafici e operativi per SkyView RPi5 ed EMS RPi4.
- Aggiunto manifesto macchina per risoluzioni, token, stati dati, freshness,
  V-speed, HSI, strumenti motore, flap, carrello ed elettrico.
- Aggiunti validatore fail-closed e sei test negativi/positivi.
- Integrato il controllo nel workflow Project Control.
- Conservato il concept approvato in `assets/concepts/` con hash SHA-256 nel
  documento di design; resta esplicitamente non operativo.

## Verificato solo software

- Il JSON è sintatticamente valido.
- Il validatore rifiuta swap RPi4/RPi5, perdita dello stato SIM, touch target
  sottodimensionati, VNE assente e qualunque autorità di comando nell'HMI.
- La CI Linux resta l'evidenza indipendente richiesta per Python e test.

## Hardware reale

- Nessuna nuova verifica o distribuzione hardware in questo incremento.

## Aperto

- Implementare componenti QML/Scene Graph e golden screenshot.
- Eseguire build ARM64, replay DDP/EDP e acceptance visiva/touch sui pannelli.
- Validare frame-rate, termica, sunlight/night mode e fault per 60 minuti.

## Incremento componenti — cinque fondazioni software

- `HmiTokens`: palette, misure minime e mapping stato-colore comuni.
- `HmiDataStateBadge`: LIVE/SIM/STALE/LOST con età esplicita.
- `HmiArcGaugeV4`: gauge V3 riusabile con health e limiti non verificati.
- `SkyViewCompassArc`: arco realmente circolare ±35°, CDI e dati waypoint.
- `HmiComponentShowcase`: superficie deterministica 1024×600 per golden futuri.

I cinque gauge principali dell'EMS usano ora V4 e propagano
`controller.dataSourceState`. I limiti sono intenzionalmente marcati
`UNVERIFIED` finché EMS-004 non viene chiuso con manuale applicabile. Nessuna
verifica grafica o hardware viene dichiarata da questo incremento.

`qmllint` Qt 6.11.1 sui cinque nuovi componenti: PASS. Configurazione e build
MSVC 19.51/Qt 6.11.1 dei target `efis-pfd-qt` ed `efis-ems-qt`: 224/224 step,
link finale PASS. Questa è evidenza software Windows; non prova resa, frame-rate
o interazione sui target ARM64.

## Incremento EMS — cinque passi successivi

- `HmiCylinderMatrixV2`: quattro canali CHT/EGT, hottest channel, spread e
  perdita esplicita del valore in stato LOST/FAIL.
- `HmiAirframePanel`: tre consensi carrello e flap verticale con posizione
  comandata/misurata e detent configurabili.
- `HmiFuelStatus`: quantità sinistra/destra, totale, riserva e degrado dati
  senza usare il solo colore.
- `HmiElectricalSynoptic`: alternatore, main bus ed essential bus come
  rappresentazione read-only con health esplicita.
- Integrazione adattiva dei quattro componenti nella pagina EMS reale e nella
  showcase deterministica; il validatore impedisce la loro rimozione silenziosa.

I limiti CHT/EGT restano `UNVERIFIED`, quindi questo incremento non chiude
EMS-004 e non costituisce approvazione al volo. Verifica hardware ancora
assente.

Verifica software dell'incremento: `qmllint` Qt 6.11.1 PASS; validatore HMI
PASS; 7/7 test dedicati PASS; build incrementale MSVC/Qt dei target PFD ed EMS
75/75 step con entrambi i link finali PASS.

## Incremento golden multi-risoluzione — cinque passi

- Aggiunto controller QML congelato con dati Rotax 912 UL simulati, stato SIM,
  Blackbox online e orologio fisso; non riceve dati avionici e non ha autorità.
- Aggiunta modalità renderer `--hmi-golden`, separata dall'avvio operativo, con
  normalizzazione DPI e cattura alle dimensioni richieste.
- Aggiunto tool di cattura ripetibile per 800×480, 1024×600 e 1280×800.
- Aggiunto validatore fail-closed di schema, risoluzioni, dimensione minima e
  SHA-256, con test positivo e prova di manomissione.
- Versionate tre golden Windows native e collegata la validazione alla CI
  Project Control.

### Evidenza software

- QML AOT e build PFD: 117 step, link PASS; rebuild dopo normalizzazione DPI:
  4/4 step PASS.
- `py_compile` dei tool PASS; suite HMI congiunta 10/10 PASS; validatore
  contratto PASS; validatore golden PASS.
- Una seconda cattura indipendente differisce soltanto per rumore raster
  subpixel (delta massimo 7/255 e frazione modificata sotto 0,00006 a 800×480):
  comparatore visuale con tolleranze chiuse PASS. L'hash file continua a
  proteggere gli artefatti versionati, mentre il confronto visuale usa i pixel.
- Ispezione visiva software: nessun taglio strutturale nelle tre immagini,
  cerchi non ellittici e moduli fuel/electrical/airframe/cilindri presenti.

### Limite rilevato

- A 800×480 diverse etichette secondarie rimangono troppo piccole per dichiarare
  acceptance ergonomica. Le immagini sono baseline di regressione software,
  non prova di leggibilità in cabina, sunlight, touch o frame-rate.
- Nessuna verifica hardware, ARM64 o pannello è stata eseguita.

## Incremento compact 800×480 — cinque passi

- Modalità compact automatica a `<=820×500`, senza cambiare il canvas logico.
- Header contestuale abbreviato e navigazione laterale ridotta a 72 px.
- Quattro azioni inferiori con altezza minima fisica di 44 px.
- Minimi tipografici per gauge e moduli EMS secondari, con propagazione
  esplicita dello stato compact.
- Contratto fail-closed e due test negativi per testo e touch target.

Evidenza solo software: contratto PASS, 12/12 test HMI PASS, `qmllint` senza
errori e build PFD MSVC/Qt 6.11.1 con link PASS. Golden rigenerate alle tre
risoluzioni, validazione SHA/pixel PASS e confronto con seconda cattura PASS.
La cattura 800×480 è stata ispezionata; non sostituisce acceptance sul display RPi4.
Build ARM64, touch, sunlight/night, 40 FPS, replay EDP, termica e fault restano
aperti. Nessuna funzione è promossa a flight approved.

## Incremento stati degradati — cinque passi

- Fixture golden parametrica per stato ed età.
- Header EMS fedele a `LIVE/STALE/LOST/FAIL/SIM`.
- Età esplicita per valori congelati `STALE/LOST`.
- Scale neutre, senza normalizzazione verde, in `LOST/FAIL`.
- Test fail-closed e comando per golden degradate; la cattura resta aperta dopo
  timeout del renderer nella sessione Windows corrente. Nessuna acceptance
  hardware implicita.
