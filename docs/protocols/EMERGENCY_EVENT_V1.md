# EFIS NEXT — Emergency Event V1

Stato: **SOFTWARE CONTRACT / LOCAL SPOOL ONLY / NON OPERATIVO**

Data: 2026-09-09

## Scopo

Rendere riproducibili le decisioni del motore Emergency Assist senza fingere
che il collegamento alla Blackbox centrale sia già disponibile. Ogni
transizione e ogni conferma del pilota produce un evento ordinato.

## Record JSONL locale

Schema: `efis-next.emergency-event.v1`.

Campi attuali:

- `event_sequence`: sequenza monotona della sessione;
- `written_utc`: istante di scrittura del PFD, non tempo GNSS certificato;
- `code` e `detail`;
- `assist_state` ed `emergency_kind`;
- `simulation`;
- `blackbox_state` osservato dal PFD.

Il percorso predefinito è `state/emergency-assist-events.jsonl`, sostituibile
con `EFIS_EMERGENCY_EVENT_LOG`. La directory `state/` non è versionata.

## Limiti

`LOCAL SPOOL` significa soltanto che il PFD ha aperto e scritto il file locale.
Non significa:

- evento persistito su NVMe Blackbox;
- snapshot completo dei segnali usati;
- fsync e resistenza alla perdita di alimentazione;
- autenticazione, firma o ricevuta end-to-end;
- ricostruzione completa della schermata mediante replay.

## Gate successivo

Definire un canale unidirezionale PFD → Blackbox separato da DDP, con identità,
sequenza, timestamp monotono, snapshot/revisioni referenziati, autenticazione,
acknowledgement di persistenza e buffer bounded. Deve essere impossibile usare
quel canale per inoltrare comandi al Pixhawk.
