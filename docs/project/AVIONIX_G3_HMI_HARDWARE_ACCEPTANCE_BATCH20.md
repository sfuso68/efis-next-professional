# AVIONIX NEXT — G3 HMI hardware acceptance batch 20

Data: 2026-09-18

Attività: `AVX-G3-03` / `PRJ-004`

Stato: PREPARED-SW / NOT_RUN / VERIFY-HW

## Venti punti predisposti

| ID | Controllo | Esito software |
|---|---|---|
| HMI-A20-01 | schema piano V1 | PASS |
| HMI-A20-02 | proprietà chiuse | PASS |
| HMI-A20-03 | stato obbligatorio PREPARED-SW/NOT-RUN | PASS |
| HMI-A20-04 | nessuna autorità di comando | PASS |
| HMI-A20-05 | PFD/MFD RPi5 ed EMS RPi4 con trasporto e risoluzione | PASS |
| HMI-A20-06 | scenari LIVE/STALE/DEGRADED/LOST/FAIL/SIM | PASS |
| HMI-A20-07 | valutazione DAY/NIGHT | PASS |
| HMI-A20-08 | schema criteri chiuso | PASS |
| HMI-A20-09 | target touch minimo 44 px | PASS |
| HMI-A20-10 | minimo 40 FPS e p95 frame-time ≤25 ms | PASS |
| HMI-A20-11 | temperatura registrata e throttling richiesto 0x0 | PASS |
| HMI-A20-12 | cold boot, reboot e prova di 60 minuti | PASS |
| HMI-A20-13 | replay DDP/EDP obbligatorio | PASS |
| HMI-A20-14 | fault Ethernet e recovery obbligatori | PASS |
| HMI-A20-15 | firma operatore per contrasto/leggibilità | PASS |
| HMI-A20-16 | firma operatore per touch | PASS |
| HMI-A20-17 | provenance, identità nodo, UTC e SHA-256 | PASS |
| HMI-A20-18 | schema manifesto evidence V1 | PASS |
| HMI-A20-19 | esattamente tre run PFD/MFD/EMS | PASS |
| HMI-A20-20 | PASS impossibile senza tutti gli artefatti firmati | PASS |

## Artefatti software

- Contratto macchina `avionix.hmi-hardware-acceptance.v1`.
- Manifesto iniziale `avionix.hmi-hardware-evidence.v1`, esplicitamente
  `NOT_RUN`, senza operatore, timestamp o artefatti inventati.
- Validatore CLI che accetta il template soltanto come preparazione e valida
  un eventuale PASS controllando target, identità, timestamp, misure numeriche,
  stati e modi provati, set completo di otto tipi di artefatto e SHA-256.
- Test di mutazione per tutti i controlli `HMI-A20-01`…`HMI-A20-20`.

## Procedura futura sul banco

Per ciascuno di PFD, MFD ed EMS si dovranno raccogliere screenshot, frame
timing, termica, log touch, boot/reboot, replay trasporto, fault/recovery e
firma operatore. I file dovranno stare accanto al manifesto compilato; quindi:

```text
python tools/ui/validate_hmi_hardware_acceptance.py --evidence <manifest.json>
```

Il comando non esegue prove hardware e non trasmette comandi. Verifica soltanto
coerenza, completezza, percorso sicuro e digest dell'evidence bundle.

## Limiti

- Nessun pannello o nodo è stato contattato in questo incremento.
- Nessun touch, FPS, contrasto, termica, reboot, replay o fault reale è stato
  provato.
- Il limite termico massimo non è inventato: si registra la temperatura e si
  richiede `get_throttled=0x0`; la soglia ambientale/prodotto resta da definire.
- `AVX-G3-03` resta `OPEN` e `PRJ-004` resta `VERIFY-HW`.
