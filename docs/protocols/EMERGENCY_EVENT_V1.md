# EFIS NEXT — Emergency Event V1

Stato: **SOFTWARE IMPLEMENTED AND TESTED / DEPLOYMENT E PROVE HW APERTI / NON OPERATIVO**

Data: 2026-09-13

## Scopo

Rendere riproducibili le decisioni del motore Emergency Assist. Ogni
transizione e ogni conferma del pilota produce un evento ordinato nello spool
locale; un forwarder separato lo consegna alla Blackbox e avanza il proprio
offset soltanto dopo una ricevuta di persistenza verificata.

## Record JSONL locale

Schema: `efis-next.emergency-event.v1`.

Campi attuali:

- `event_sequence`: sequenza monotona della sessione;
- `session_id`: UUID del processo PFD; la coppia sessione/sequenza è univoca;
- `event_mono_ns`: timestamp monotono locale del motore Emergency Assist;
- `written_utc`: istante di scrittura del PFD, non tempo GNSS certificato;
- `code` e `detail`;
- `assist_state` ed `emergency_kind`;
- `simulation`;
- `source_node`, stato DDP, stato dati motore e stato recorder osservato;
- `snapshot_sequence` e `decision_context`: valori selezionati con usabilità,
  marker SIM, signal ID e sequenze degli snapshot volo/motore;
- `assist_output`: annunci FLY/NAV/COMM/voice, gate procedura e motivi di
  degradazione che erano visibili al momento dell'evento.

Il percorso predefinito è `state/emergency-assist-events.jsonl`, sostituibile
con `EFIS_EMERGENCY_EVENT_LOG`. La directory `state/` non è versionata.

## Trasporto e ricevuta

`tools/blackbox/emergency_event_forwarder.py` legge solo record JSONL completi,
valida lo schema e invia `POST /v1/emergency-events` al servizio dedicato sulla
porta 18094. L'identità del nodo usa un token bearer separato dai token fuel.

`tools/blackbox/emergency_event_hub.py` valida limiti e tipi, rifiuta la stessa
coppia `(source, session_id, event_sequence)` con payload differente e salva in
SQLite WAL con `synchronous=FULL`. Solo dopo il commit risponde con schema
`efis-next.emergency-event-ack.v1`, digest SHA-256 stabile e
`persisted=true`. Il forwarder confronta digest, sorgente, sessione e sequenza
prima di aggiornare atomicamente l'offset con flush e fsync.

Se SQLite rifiuta una scrittura per spazio esaurito, database read-only,
corruzione, contesa o altro errore di storage, l'hub esegue rollback e non
emette mai una ricevuta positiva. Risponde `507` con `persisted=false`; il
forwarder conserva l'offset precedente e ritenta lo stesso record. La health
passa a `503`, espone un codice di errore normalizzato e il conteggio delle
scritture rifiutate. Lo stato storage torna sano soltanto dopo una successiva
scrittura e commit riusciti, non per il solo fatto che una lettura funzioni.

All'apertura del database viene eseguito `PRAGMA quick_check`; WAL,
`synchronous=FULL`, timeout di contesa e checkpoint automatico sono impostati
esplicitamente. Queste misure e i test di uscita brusca del processo verificano
il comportamento software, ma non equivalgono a una prova di perdita
alimentazione sull'NVMe reale.

Il servizio espone soltanto health, inserimento eventi e lettura autenticata.
Qualsiasi endpoint di comando restituisce 404; non esistono payload di risposta
che possano rappresentare MAVLink, modalità, servo, radio o attuatori.

## Replay di audit

`tools/blackbox/emergency_event_replay.py` rilegge spool o export, valida lo
schema e segnala gap/regressioni per sessione. Produce un rapporto consultivo;
non invoca azioni e non riproduce comandi. Il contesto registrato consente
l'audit della decisione e della UI, ma non sostituisce ancora un replay binario
sincronizzato di tutti i bus.

## Limiti e gate aperti

Senza stato forwarder `BLACKBOX ACK #n`, `LOCAL SPOOL` significa soltanto che il
PFD ha aperto e scritto il file locale. Anche con ack, i test software non
dimostrano ancora:

- resistenza a perdita di alimentazione reale di PFD e NVMe Blackbox;
- TLS o isolamento fisico finale della FLIGHT-DATA;
- rotazione/retention e capienza durante una lunga indisponibilità di rete;
- tempo UTC GNSS affidabile e sincronizzazione completa con CAN/MAVLink;
- ricostruzione pixel-perfect della schermata e audio effettivamente riprodotto.

Sul banco token, unità systemd, bind FLIGHT-DATA, rete assente, riavvio del
processo e duplicati sono già stati verificati. Il disco pieno è coperto da
fault injection SQLite soltanto in software. Restano da provare sul nodo reale
filesystem pieno, esaurimento inode, I/O error, riavvio macchina e power-loss.
Il protocollo resta separato da DDP/EDP e non autorizza alcun comando al
Pixhawk.
