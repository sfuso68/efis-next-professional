# AVIONIX NEXT — G3 SkyView responsive batch 20

Data: 2026-09-18

Attività: `AVX-G3-04` / `PRJ-004`

Stato: VERIFIED-SW / VERIFY-HW

## Venti punti implementati

| ID | Controllo | Esito |
|---|---|---|
| HMI-R20-01 | schema, radice e stato non-flight chiusi | PASS |
| HMI-R20-02 | matrice SkyView 1024/1280/1920 ordinata | PASS |
| HMI-R20-03 | breakpoint compact a 1100 px | PASS |
| HMI-R20-04 | fascia FMA riservata a 58 px logici | PASS |
| HMI-R20-05 | margine verticale nastri a 12 px logici | PASS |
| HMI-R20-06 | separazione meteo/ALT a 8 px logici | PASS |
| HMI-R20-07 | pannello meteo compact 154×64 | PASS |
| HMI-R20-08 | meteo compact sotto heading, margine 62 | PASS |
| HMI-R20-09 | badge compact con altezza minima 44 | PASS |
| HMI-R20-10 | legenda secondaria soppressa in compact | PASS |
| HMI-R20-11 | no-data integrato nel badge compact | PASS |
| HMI-R20-12 | pannello meteo regular 178×103 | PASS |
| HMI-R20-13 | meteo regular sotto heading, margine 62 | PASS |
| HMI-R20-14 | badge regular alto 34 px logici | PASS |
| HMI-R20-15 | legenda disponibile in regular | PASS |
| HMI-R20-16 | banner no-data regular separato di 6 px | PASS |
| HMI-R20-17 | clearance minima heading dichiarata | PASS |
| HMI-R20-18 | attribuzione e testo no-data obbligatori | PASS |
| HMI-R20-19 | stati e interazione golden fail-closed | PASS |
| HMI-R20-20 | z-order e assenza autorità di comando | PASS |

## Risultato grafico

- A 1024×600 il doppio riquadro sovrapposto è sostituito da un solo badge
  compatto; la legenda secondaria non compete con gli strumenti primari.
- A 1280×800 e 1920×1080 badge e avviso no-data restano separati, ma l'intero
  overlay è portato sotto la heading ribbon.
- L'overlay resta a sinistra del nastro ALT e non modifica terreno, pitch
  ladder, flight-path marker o banda motore.

## Evidenza software

- Build MSVC/Qt 6.11.1 e QML AOT: PASS.
- Tre golden native Windows rigenerate e validate.
- Seconda cattura indipendente 3/3: confronto anti-alias PASS.
- Validatore responsive: 20 controlli fail-closed PASS.

## Limiti

- Le distanze sono verificate sulla fixture software, non sul pannello reale.
- Touch, FPS, contrasto giorno/notte, termica e fault restano da verificare su
  RPi5; nessuna acceptance hardware è implicita.
- Il precedente shadowing QML del nome `enabled` è rimosso tramite la proprietà
  esplicita `radarEnabled`.
