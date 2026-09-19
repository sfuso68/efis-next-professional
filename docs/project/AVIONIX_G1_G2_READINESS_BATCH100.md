# AVIONIX G1/G2 — integration readiness, batch 100

Data: 2026-09-18

Stato: **IMPLEMENTED-SW / PREPARED-NOT-RUN / NO-GO**

## Obiettivo

Il batch organizza cento controlli verificabili prima dell'integrazione reale
Sequent→RPi4→Blackbox→EMS. Non sostituisce l'inventario fisico e non autorizza
scansioni, comandi vendor, tarature, cablaggi, deployment o prove live.

La fonte macchina è
`config/hardware/avionix-g1-g2-readiness-v1.json`; il validator è
`tools/hardware/validate_g1_g2_readiness.py`.

## I cinque sottolotti

### 1–20 — inventario fisico

Provenienza del nodo, digest della cattura, quattro slot e quantità previste,
foto fronte/retro, SHA-256, confinamento dei percorsi, etichette, revisione,
seriale, DIP visuali, connettori, modello/rating alimentatore, alimentazione
singola, divieto di probing e revisione tecnica.

### 21–40 — mappa dei 18 segnali motore

Snapshot EDP, identificativi e unità stabili, RPM, MAP, pressione/temperatura
olio, pressione/flusso carburante, due quantità carburante, bus, alternatore,
quattro CHT, quattro EGT, raw value, `ChannelId`, `NodeId`, revisione di
calibrazione, futuro `SourceId` reale e separazione REAL/SIM.

### 41–60 — governance calibrazione

Profilo versionato legato al motore, revisione per canale, conservazione raw,
coefficienti, unità, dominio raw, soglie di plausibilità/open/short non
inventate, calibrazione per canale, SHA-256, UTC, operatore, strumento campione,
condizioni ambientali, rollback, storico immutabile, separazione SIM e divieto
di approvazione al volo.

### 61–80 — trasporto e Blackbox

Ruolo del nodo, autorità esclusiva Blackbox, trasporto upstream ancora
irrisolto e non eseguibile, EDP V1/UDP 49003 one-way verso EMS, display
receive-only, assenza MAVLink/route/bridge/NAT, identità, CRC32C, sequenze,
istanza publisher, freshness monotona, STALE/LOST, recorder/replay, firewall,
fault matrix e runbook live soggetto ad approvazione separata.

### 81–100 — evidence e release

Root confinata, percorsi relativi, SHA-256, UTC, operatore, nodo, commit,
digest manifest, raw capture, redazioni, bundle foto/power/calibrazione/mappa
canali/trasporto, report fault e rollback, soak minimo 3600 secondi, replay
dello stesso dataset e decisione finale NO-GO senza flight approval.

## Esito verificato

- 100/100 controlli strutturali validati.
- 100/100 mutazioni isolate rifiutate fail-closed.
- 49 controlli `PASS` su contratti software già dimostrati.
- 37 controlli `PREPARED` per future evidenze.
- 11 controlli `BLOCKED_HARDWARE`.
- 2 controlli `BLOCKED_ARCHITECTURE`.
- 1 controllo `BLOCKED_APPROVAL`.
- Suite dedicata: 8/8 test PASS.

`PASS` del validator significa esclusivamente che il contratto rappresenta
correttamente la situazione. Non significa che il sistema reale sia pronto.

## Decisione

`NO_GO_PENDING_G1_PHYSICAL_INVENTORY_AND_ARCHITECTURE_REVIEW`.

Restano 0/4 schede verificate, `SourceId` reale non deciso, mappa canali e
calibrazioni assenti e trasporto RPi4→Blackbox irrisolto.
