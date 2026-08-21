# EFIS NEXT Adaptive Mission Deck — Concept Baseline

## Stato

Le tavole PFD Focus, Mission View e FPV Vision costituiscono la nuova baseline
concettuale. La UI Qt V5 rimane un banco tecnico e non è un riferimento
estetico. Nessuna tavola dichiara funzioni già certificate o già collegate.

## DNA comune

- Command Rail continuo per modi, sorgenti, integrità, registrazione e allarmi.
- Flight Canvas dominante; i dati critici conservano posizione e gerarchia.
- Context Lanes laterali adattive, non tape statici sovraccarichi.
- Mission Dock inferiore con moduli vivi e stato immediatamente leggibile.
- Bianco caldo per valori primari, cyan per selezioni, verde per attivo,
  ambra/rosso solo per caution/warning, magenta solo per flight director.
- Touch per contesto e configurazione; controlli fisici previsti per azioni
  critiche e uso in turbolenza.

## PFD Focus

Synthetic vision e guida occupano il centro. IAS/TAS/AoA/energia sono riuniti
a sinistra; ALT/RA/VSI/minimums a destra. Il Mission Dock mantiene motore,
carburante, elettrico, navigazione, meteo, video e checklist senza sottrarre
spazio alla scansione primaria.

## Mission View

La mappa è dominante, ma un mini-PFD invariabile resta sempre disponibile.
Traffico, meteo, glide reach, fuel range, profilo verticale e diversion sono
separati in livelli: dati reali, previsioni e raccomandazioni non condividono
la stessa codifica visiva.

## FPV Vision

Il video è una sorgente contestuale, non il riferimento di volo unico. Assetto,
flight path, IAS, AoA, ALT, RA e VSI restano indipendenti dal flusso video. La
modalità espone latenza, packet loss, gimbal, camera attiva e registrazione.
Perdita o inaffidabilità video impongono AUTO REVERT PFD.

## Gate prima dell'implementazione

1. Definire geometrie e safe areas a 1024x600, 1280x800 e 1920x1080.
2. Classificare ogni elemento come flight-critical, contextual o mission.
3. Definire stati LIVE, SIM, DEGRADED, STALE, FAIL e NOT INSTALLED.
4. Validare interazioni touch con target minimi e percorso di ritorno PFD.
5. Implementare prima il design system e i componenti, poi le tre composizioni.

## Tavole

- `output/adaptive-mission-deck/01-pfd-focus.png`
- `output/adaptive-mission-deck/02-mission-view.png`
- `output/adaptive-mission-deck/03-fpv-vision.png`
