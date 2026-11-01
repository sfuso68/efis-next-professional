# R1-34 — console locale stdio per il solo SIL

2026-11-01. DEVELOPMENT / NON-CERT / NON APPROVATO AL VOLO.
Scelta utente: sessione locale su serverha via SSH esistente, nessun servizio
di rete nuovo. Nessun endpoint, chiave, firewall, dashboard, Pixhawk, servo,
display o release avionica modificati. Il cruscotto rimane read-only.

## Perimetro

`local_console.LocalConsole` compone executor/engine sul thread owner.
`owner_console` aggiunge processo finito JSBSim C172P reference, ingresso
stdin e uscita stdout tramite file descriptor del processo. È l'utente OS
della sessione locale/SSH a costituire il contesto fidato; nessun campo del
JSON può scegliere principal, manifesto, lease, epoch o tick. Non è una
sandbox fra utenti Python, un server SSH o un protocollo autenticato remoto.
Il codice del processo e gli oggetti iniettati restano fidati.

Configurazione, modello/digest, source commit dichiarato, policy di controllo
e limiti console sono congelati prima del manifesto. Il commit è un valore
fornito dal launcher, non attestazione crittografica dell'eseguibile. Il modello
è REFERENCE_UNVALIDATED, non un profilo approvato Rotax/Asso/Pioneer.

Run da 1 a 60 secondi di fisica a 100 Hz / output 20 Hz. Lease di controllo
massimo 5 secondi wall e 500 tick, senza rinnovo. Dopo la scadenza il modello
può continuare, ma rimuove gli override; non promette neutralizzazione o
stabilità aerodinamica. Il wall cap seconds+60 è cooperativo: non interrompe
una chiamata kernel/fsync bloccata. Nessuna qualifica hard real-time.

## Comandi JSONL (una riga ciascuno)

```json
{"action":"STATUS"}
{"action":"SET_CONTROL","command_id":"pilot-1","sequence":1,"control":"aileron","value":0.1}
{"action":"LOOKUP","command_id":"pilot-1"}
{"action":"STOP"}
```

Controlli ammessi: aileron/elevator [-1,1], throttle [0,1]. Solo FDM SIL.
Campi esatti, numeri finiti/non booleani, UTF-8 e JSON senza chiavi duplicate.
Owner assegna il prossimo tick fisico e una scadenza limitata dalla lease.
PAUSE/RESUME/START/REARM sono UNSUPPORTED: manca ancora dedup lifecycle
inter-epoch, quindi non viene esposta una transizione ambiguamente ripetibile.
Se l'engine esterno è in pausa, il pump serve comunque watchdog e STOP;
la vecchia lease resta chiusa dopo resume, senza riarmo automatico.

Ogni ID conserva il primo shorthand canonico e il comando completo originale,
anche quando ingress restituisce FULL/BUSY/revoca. Retry identico interroga
quell'identità, senza aggiornare tick/epoch né rieseguire. Payload diverso è
DUPLICATE_CONFLICT. Esito incerto è UNCERTAIN_NO_RETRY senza falso ACK.
LOOKUP/STATUS non allocano storia. ID non sfrattati; 256 ID o 512 KiB di
storia esauriti richiedono STOP. Per un ingresso non ammesso occorre un nuovo
ID esplicito, non un retry silenziosamente ripianificato.

## Limiti e terminazione

Stdin viene letto a blocchi bounded, al massimo 4097 byte per accertare il
superamento di una riga da 4096. Mailbox 16 righe (64 KiB massimi), servizio
4 per pump, lock nonblocking. STOP ha latch indipendente e revoca subito
l'autorità anche con mailbox piena o bloccata; l'owner applica l'arresto.
EOF/chiusura SSH, input oltre limite e wall cap richiedono arresto sicuro,
che può terminare anche con zero tick: COMPLETE non significa durata piena.
JSON malformato è INVALID; FULL/BUSY/INVALID sono visibili nell'ultimo stato
ingress con contatore saturato, non ACK durevoli. Errore read/write invece
marca FAILED; interruzione o errore di avvio/runner non pubblicano result
COMPLETE o probe-report di successo. Evidenze incomplete restano conservate.

Stdout usa un pacchetto attivo e uno latest, con conteggio sovrascritture;
non attende il client sul thread fisico. Lettura/scrittura FD diretta evita
lock di stdio buffered alla fine del processo. Join dei thread massimo 50 ms
ciascuno; thread daemon bloccato nel kernel non impedisce uscita del processo.
Mai riutilizzare questi FD per altre risorse nello stesso processo persistente.
READY è un oggetto; le iterazioni pubblicano un solo SNAPSHOT contenente
replies (batch oppure STOPPED/TERMINAL), stato ingress e contatore overwrite.
Ingress non sovrascrive da solo la risposta della stessa iterazione.
Lo stdout può perdere risposte e non garantisce consegna
del terminale: interrogare gli ID mentre la run esiste, poi i file di audit.
Banner diagnostici della libreria nativa non sono un protocollo wire.

## Avvio manuale del candidato (non installazione permanente)

Dalla copia sorgenti verificata in ambiente con runtime JSBSim fissato:

```sh
python3 -B -m tools.simulation.owner_console \
  --directory /percorso/nuova-run \
  --world config/simulation/treviso-belluno-world-reference.json \
  --session-id console-banco-001 --source-commit COMMIT_SORGENTE_40_HEX \
  --seconds 10 --journal-mode async-test
```

Serve directory nuova; mai sovrascrittura. Tenere stdin aperto: una pipe che
invia il comando e chiude subito richiede STOP prima che possa essere applicato.
Confermare applicazione solo con LOOKUP/APPLIED e audit/readback, non con
QUEUED_NOT_VALIDATED. Credenziali non entrano nei file. Verifica/archivio
utilizzano esplicitamente `--live-control` del contratto R1-33, quattro payload.

## Verifica e punti aperti

24 nuovi test locali: boundedness, STOP piena/busy, ID/retry/conflitto,
revoca/pausa, input non valido, esiti incerti, output bloccato/short write,
EOF/fault, startup parziale e failure injection runner con fixture proprietà.
Regressione selezionata 247 PASS; CMake configure e 14 suite CTest Python
PASS. Nessuna nuova build nativa Windows: WinError4551 e warning path restano.
Test JSBSim nativo aggiunto: stdio tramite pipe locali, comando reale, 100 tick,
audit e archive/restore V2; esito server da registrare nell'evidence bundle.
Non equivale a prova interattiva SSH di un pilota, durata lunga, power-loss,
panelli simultanei o verifica hardware avionica. M2/P03 rimangono PARTIAL.

Prossimi gate: lifecycle/riarmo con identità e dedup durevoli, ammissione e
timestamp wall nell'audit, replay fisico esecutivo, fanout isolato/parità
consumer, modelli/impianti/guasti completi e campagne timing/storage/pannelli.
