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
