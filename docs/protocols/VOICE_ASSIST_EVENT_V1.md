# EFIS NEXT — Voice Assist Event V1

Stato: **CONTRATTO LOGICO SOFTWARE / TRASPORTO NON IMPLEMENTATO**

Data: 2026-09-14

## Direzione

Il futuro flusso è unidirezionale dal Voice Core verso i display e lo spool di
audit. Non contiene endpoint di comando radio, Pixhawk o attuatori.

## Candidate locale del recognizer

Prima del gate C++ esiste il contratto locale
`efis-next.voice-recognition-candidate.v1`. È un risultato probabilistico del
decoder, non un evento accettato. In questa fase viene scritto soltanto su
standard output dal replay SIM e non attraversa FLIGHT-DATA.

Campi obbligatori del candidato Vosk replay:

- sessione, sequenza, UTC di emissione e tempo monotono;
- nodo sorgente SIM, recognizer, modello, lingua e SHA-256 della grammatica;
- trascrizione, confidenza minima, media e numero di parole misurate;
- health, PTT e flag `simulated=true`;
- `candidate_only=true`, `accepted=false` e `intent=NONE`;
- `transmission_or_actuation_allowed=false`.

Se mancano le confidenze parola-per-parola, la confidenza conservativa è zero.
La grammatica Vosk è soltanto un hint del decoder; il gate C++ resta
autorevole e deve rivalidare frase, lingua, PTT, health e soglia.

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
