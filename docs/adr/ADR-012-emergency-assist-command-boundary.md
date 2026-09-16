# ADR-012 — Emergency Assist consultivo e confine dei comandi

- Stato: accettata per Software V1
- Data: 2026-09-09
- Riferimenti: ADR-001, ADR-002, ADR-003, ADR-004, ADR-005,
  `EMERGENCY_ASSIST_V1.md`, `EMERGENCY_ASSIST_ENGINE_POWER_LOSS_V1.md`

## Decisione

`EmergencyAssist` appartiene al core C++20 e produce soltanto uno stato
consultivo: messaggi, gate di validità, classifica di candidati, passo di una
procedura approvata ed eventi di audit. La sua API non contiene comandi
MAVLink, modalità ArduPilot, valori servo, radio, transponder o attuatori.

L'attivazione manuale porta sempre a una conferma del pilota. Un rilevatore
automatico può generare soltanto `SUSPECTED`; è inerte se soglie, persistenza e
applicabilità non sono approvate. Nessuna singola misura può attivarlo.

I calcoli di planata sono fail-closed. Un risultato richiede modello di
prestazione approvato, AGL, posizione, vento, terreno, ostacoli e database
aeroporti validi. Un risultato costruito con input o modello simulato è
`SIM_ESTIMATE`, mai `REACHABLE`.

Le checklist richiedono package ID, revisione, fonte, hash, flag di
approvazione e almeno un passo. La mancanza di uno solo di questi elementi
blocca il contenuto operativo. I valori legacy del profilo aeromobile non sono
un package approvato.

## Confine di processo

La UI PFD può invocare soltanto azioni del pilota sul motore consultivo. Gli
eventi vengono prima scritti in uno spool JSONL locale versionato; questo
spool non viene chiamato Blackbox e non dimostra la consegna al registratore
centrale. Il protocollo di inoltro e conferma Blackbox sarà un lavoro separato.

## Conseguenze

- una perdita PFD/Qt/Ethernet non può modificare il Pixhawk;
- le otto famiglie di emergenza possono essere selezionate senza inventare una
  procedura operativa;
- il simulatore può esercitare layout, ranking e workflow con marcatori SIM;
- audio reale, pulsante GPIO, procedure POH e sink Blackbox centrale restano
  gate indipendenti;
- Stabilize Assist ed Emergency Glide/Autoland non sono estensioni implicite
  della V1 e richiedono una nuova autorità architetturale e prove HIL.

## Alternative escluse

- comando MAVLink dal PFD o dalla Blackbox: viola ADR-003 e introduce una
  dipendenza Linux/rete nel controllo di volo;
- fallback a velocità o rapporto di planata legacy: produce un dato plausibile
  ma non applicabile, vietato da ADR-004;
- checklist generata da AI: non conserva applicabilità, revisione e fonte e
  non è deterministica;
- aeroporto più vicino assunto raggiungibile: ignora terreno, vento, pista e
  margine all'arrivo.
