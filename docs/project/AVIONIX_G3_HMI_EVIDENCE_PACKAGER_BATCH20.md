# AVIONIX NEXT — G3 HMI evidence packager batch 20

Data: 2026-09-18

Attività: `AVX-G3-03` / `PRJ-004`

Stato: VERIFIED-SW / NOT_RUN / VERIFY-HW

## Venti controlli implementati

| ID | Controllo | Esito software |
|---|---|---|
| HMI-P20-01 | schema risultati V1 | PASS |
| HMI-P20-02 | proprietà radice chiuse | PASS |
| HMI-P20-03 | conferma operatore e stato READY espliciti | PASS |
| HMI-P20-04 | operatore e timestamp UTC | PASS |
| HMI-P20-05 | run esatte e ordinate PFD/MFD/EMS | PASS |
| HMI-P20-06 | proprietà run chiuse | PASS |
| HMI-P20-07 | identità nodo obbligatoria | PASS |
| HMI-P20-08 | release con object ID Git completo | PASS |
| HMI-P20-09 | proprietà misure chiuse | PASS |
| HMI-P20-10 | durata minima 60 minuti | PASS |
| HMI-P20-11 | FPS minimo ≥40 | PASS |
| HMI-P20-12 | frame-time p95 ≤25 ms | PASS |
| HMI-P20-13 | temperatura massima realmente numerica | PASS |
| HMI-P20-14 | throttling/undervoltage `0x0` | PASS |
| HMI-P20-15 | touch target ≥44 px | PASS |
| HMI-P20-16 | sei stati dati completi | PASS |
| HMI-P20-17 | DAY e NIGHT completi | PASS |
| HMI-P20-18 | cold boot e reboot positivi | PASS |
| HMI-P20-19 | replay trasporto e recovery Ethernet positivi | PASS |
| HMI-P20-20 | otto artefatti sicuri, non vuoti, firmati e output immutabile | PASS |

## Comportamento operativo

Il file `avionix-hmi-hardware-results-not-run-v1.json` è un template sicuro:
non contiene identità, firma, misura o risultato. L'assemblatore rifiuta di
usarlo finché non viene prodotto un file separato `READY_FOR_ASSEMBLY` a valle
delle prove reali.

Quando tutte le misure e gli otto artefatti per ogni ruolo sono presenti:

```text
python tools/ui/assemble_hmi_hardware_evidence.py \
  --results <bundle>/results.json \
  --output <bundle>/evidence.json
```

L'output viene creato atomicamente, non sovrascrive evidenza esistente, calcola
SHA-256 e viene riletto dal validatore indipendente. Se il controllo finale
fallisce, il manifesto viene rimosso. I file originali non vengono modificati.

## Confini di sicurezza

- L'assemblatore non acquisisce dati, non attiva fault e non controlla alcun
  sistema avionico.
- `hardware_verified=true` può essere scritto solo da risultati completi e
  confermati dall'operatore.
- Ogni manifesto impone `flight_approval=false`: questa evidenza non è una
  certificazione né un'autorizzazione al volo.
- Nessun hardware è stato provato in questo incremento; `AVX-G3-03` resta
  `OPEN` e `PRJ-004` resta `VERIFY-HW`.
