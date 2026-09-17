# AVIONIX NEXT — G3 SkyView golden batch 20

Data: 2026-09-17

Attività: `AVX-G3-04` / `PRJ-004`

Stato: VERIFIED-SW / VERIFY-HW

## Venti punti implementati

| ID | Controllo | Esito |
|---|---|---|
| HMI-G20-01 | schema V2 e radice JSON valida | PASS |
| HMI-G20-02 | proprietà manifesto chiuse | PASS |
| HMI-G20-03 | divieto di dichiarare hardware acceptance | PASS |
| HMI-G20-04 | ruolo limitato a EMS/SkyView | PASS |
| HMI-G20-05 | fixture coerente con il ruolo | PASS |
| HMI-G20-06 | piattaforma di rendering dichiarata | PASS |
| HMI-G20-07 | timestamp ISO-8601 UTC | PASS |
| HMI-G20-08 | stato dati canonico | PASS |
| HMI-G20-09 | età coerente con SIM/STALE/LOST | PASS |
| HMI-G20-10 | matrice risoluzioni esatta per ruolo | PASS |
| HMI-G20-11 | proprietà immagine chiuse | PASS |
| HMI-G20-12 | filename deterministico ruolo-risoluzione | PASS |
| HMI-G20-13 | filename univoci | PASS |
| HMI-G20-14 | presenza di ogni PNG | PASS |
| HMI-G20-15 | dimensioni PNG reali coerenti | PASS |
| HMI-G20-16 | dimensione file dichiarata e non sospetta | PASS |
| HMI-G20-17 | SHA-256 file ricalcolato | PASS |
| HMI-G20-18 | SHA-256 pixel decodificati ricalcolato | PASS |
| HMI-G20-19 | path confinati al bundle | PASS |
| HMI-G20-20 | ordine risoluzioni congelato | PASS |

## Implementazione

- Nuova modalità renderer `--skyview-golden`, separata dall'avvio operativo.
- Fixture unica con dati volo, navigazione, motore e meteo statici.
- Terreno golden sintetico privo di dipendenze da rete, DEM o texture locali.
- Capture e confronto resi role-aware senza rompere il ruolo EMS.
- Golden SkyView native Windows a 1024×600, 1280×800 e 1920×1080.
- Manifesto EMS migrato a V2 senza modificare i tre PNG esistenti.
- Validazione di entrambi i bundle collegata a Project Control CI.

## Evidenza software

- Build MSVC/Qt 6.11.1 `efis-pfd-qt`: PASS, 57/57 step finali.
- Cattura SkyView nativa: 3/3 immagini prodotte.
- Seconda cattura indipendente: confronto entro tolleranza anti-alias PASS.
- Suite golden: 5/5 PASS, incluse venti mutazioni fail-closed.
- Validator EMS V2 e SkyView V2: PASS.

## Limiti

- Le golden verificano geometria software, non resa del terreno reale.
- L'ispezione a 1024×600 mostra una zona superiore densa attorno al riquadro
  meteo: resta un punto di affinamento, non viene nascosto dal gate.
- Nessuna misura FPS, touch, sunlight/night, termica o fault è stata eseguita
  su RPi4/RPi5. `PRJ-004` resta `VERIFY-HW`.
