# PFD Focus — specifica deterministica 1024x600

## Griglia

| Regione | Coordinate logiche | Funzione |
|---|---:|---|
| Command Rail | `0,0 1024x54` | identità, modo, integrità, recorder, allarme |
| Left Context Lane | `0,54 158x440` | IAS, TAS, AoA, trend/validità |
| Flight Canvas | `158,54 708x440` | assetto, guida, heading, energia, NAV |
| Right Context Lane | `866,54 158x440` | ALT, RA, VSI, minimums/validità |
| Mission Dock | `0,494 1024x106` | stato sintetico dei sottosistemi |

Le coordinate sono scalate preservando l'aspect ratio. A risoluzioni più alte
si amplia il Flight Canvas; i dati primari non cambiano posizione relativa.

## Classi di dato

- **Primary**: assetto, IAS, ALT, heading, VSI. Sempre visibile.
- **Guidance**: FD, course, selected speed/altitude. Cyan o magenta.
- **Context**: TAS, AoA, RA, minimums, vento, energia.
- **Mission**: motore, fuel, elettrico, meteo, video, checklist, recorder.

## Stati obbligatori

`LIVE`, `SIM`, `DEGRADED`, `STALE`, `FAIL`, `NOT INSTALLED`, `UNVERIFIED`.
Un valore non disponibile non può essere sostituito da un numero decorativo.

## Regole

- rosso e ambra sono riservati agli allarmi;
- il magenta è riservato al flight director;
- nessun overlay può coprire contemporaneamente assetto, IAS e ALT;
- la perdita video non modifica il PFD Focus;
- le azioni critiche richiederanno conferma o comando fisico.
