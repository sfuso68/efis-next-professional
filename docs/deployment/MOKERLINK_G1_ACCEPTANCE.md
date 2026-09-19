# MokerLink G1 — runbook di accettazione preparato

Questo runbook è deliberatamente **non esecutivo**. Il suo stato è `NOT_RUN`.
Non collegare o alimentare il candidato sulla base di questo documento.

## Prerequisiti bloccanti

- autorizzazione esplicita per la sessione hardware;
- verifica fotografica del modello e del pinout di alimentazione;
- banco isolato da FLIGHT-DATA e VIDEO installati;
- piano elettrico G4 con protezione, carico e strumenti definiti;
- operatore presente e percorso evidence dedicato.

## Evidence minima

Il contratto richiede dieci tipi di artefatto: tre fotografie di identità,
report link RJ45, velocità per porta, alimentazione/brownout,
corrente/termica/restart, isolamento fisico, fault/recovery e firma operatore.
Ogni file deve avere path relativo confinato e digest SHA-256.

## Validazione offline

```text
python tools/network/validate_g1_switch_acceptance.py \
  --results <manifest-risultati.json> \
  --evidence-root <directory-evidenze>
```

Il template repository resta `NOT_RUN`. Un eventuale `BENCH_PASS` dimostra
solo la completezza dell'accettazione al banco e non autorizza installazione,
rete avionica o volo.
