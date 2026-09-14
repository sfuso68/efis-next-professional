# EFIS NEXT — Voice Assist Event V1

Stato: **CONTRATTO LOGICO SOFTWARE / TRASPORTO NON IMPLEMENTATO**

Data: 2026-09-14

## Direzione

Il futuro flusso è unidirezionale dal Voice Core verso i display e lo spool di
audit. Non contiene endpoint di comando radio, Pixhawk o attuatori.

## Campi minimi evento riconoscimento

- `schema=efis-next.voice-assist-event.v1`;
- `event_id`, `session_id`, `sequence`;
- tempo monotono e UTC con stato di affidabilità;
- `source_node`, recognizer e versione modello;
- lingua e hash della grammatica;
- PTT attivo, health recognizer e flag SIM;
- trascrizione normalizzata e confidenza;
- stato `DISABLED/UNAVAILABLE/STANDBY/ACCEPTED/UNCERTAIN/REJECTED`;
- intent oppure `NONE`;
- reason code;
- `transmission_or_actuation_allowed=false` obbligatorio.

## Campi minimi bozza fraseologica

- package ID, revisione, giurisdizione, fonte, checksum e stato approvazione;
- lingua, fase di volo, stazione e tipo servizio;
- valori sostituiti con origine, validità e freshness;
- campi mancanti e read-back richiesti;
- stato `BLOCKED/SIMULATION_DRAFT/READY_FOR_PILOT_REVIEW`;
- `pilot_review_required=true`;
- `transmit_allowed=false`.

## Acknowledgement

Il record checklist mantiene separati `presented`, `pilot_acknowledged`,
`acknowledged_by_voice` e `sensor_verification`. Il consumer non può derivare
la verifica del sensore dalla sola frase del pilota.

## Integrità e trasporto aperti

Il formato wire, autenticazione, anti-replay, ack persistente e retention
saranno definiti dopo il test locale. Fino ad allora il contratto non viene
trasmesso sulla rete FLIGHT-DATA e non è dichiarato integrato con Blackbox.
