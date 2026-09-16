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
