# Cache METAR/TAF: integrità e ripartenza offline

WX-001, A5 del piano autonomo. Candidato software, non distribuito.
Non autorizza attivazioni; rimangono bloccate fino a verifica alimentatori.

## Contratto candidato

Il provider deve fornire un oggetto JSON con almeno uno fra `metar` e `taf`:
ciascuno è testo (massimo 32768 caratteri) o null. Testo vuoto/spazi e null
non sono un bollettino disponibile. Oggetti/array non sono implicitamente
convertiti in testo: un provider che usa una struttura differente richiederà
un adapter revisionato. Non è stato verificato il payload reale del provider.
Se presenti, `icao` e `station` devono concordare con la stazione richiesta.
La nuova busta persistita contiene anche `icao`; le vecchie buste senza
identità esplicita restano leggibili se il resto del contratto è valido.

Download massimo 256 KiB, busta cache massima 512 KiB. Lettura limitata a
limite+1 byte; UTF-8 e JSON rigorosi, campi duplicati e numeri non finiti
rifiutati. Nessun payload viene scritto prima della validazione. Un errore
HTTP/rete/timeout, un JSON troncato o non valido e un errore di serializzazione,
fsync o rename prima del replace conservano la generazione completa precedente.
I temporanei creati dal writer vengono rimossi; non c'è potatura delle cache.

Il file viene sostituito atomicamente dopo flush/fsync. Non viene qualificata
la persistenza a perdita fisica di alimentazione: filesystem/storage e fsync
del parent non fanno parte della prova qui eseguita. Il controllo dei writer
cooperanti è stato aggiunto nel successivo blocco del 21 ottobre, sotto.

## Freshness: cosa significa e cosa NON significa

I confini sono applicati all'età frazionaria, non al solo intero visualizzato:
600 s CURRENT, oltre 600 STALE; 1200 s STALE, oltre 1200 EXPIRED. Orologio
non finito o timestamp troppo futuro non rendono disponibile il bollettino.
L'età rimane quella del download, non del METAR osservato o della validità TAF:
`freshness_basis=FETCH_TIME_ONLY`, `provider_report_time_validated=false` e
`safety_use_allowed=false`, sempre imposti dalla cache, non dal provider.
Un bollettino vecchio appena scaricato non viene qualificato come osservazione
attuale: interpretazione temporale e qualifica provider restano aperte.

## Verifica ripetibile

```sh
python3 -B -m unittest discover -s server/edge-node -p test_edge_weather.py -v
```

28 test PASS su Linux serverha e 28 su Linux ARM64 MFD, in directory isolate.
Test su fixture con HTTP loopback, nessun provider o credenziale reale. Sono
coperti rifiuti payload, limiti, ultimo file valido, fault fsync/rename,
riapertura con un interprete distinto e confini stale/expired. Il test HTTP
locale non qualifica HTTPS, CA, autenticazione, licenze o rete reale.

Prima della correzione due test riproducevano cinque fallimenti contando i
subtest. Python Windows embedded bloccato all'import _ssl, non dichiarato PASS;
usate piattaforme Linux senza modificare protezioni OS. Log e hash in
`docs/status/evidence/weather-cache-20261019/`. CI Project Control contiene
già la discovery di questa suite; esito remoto nuovo da leggere separatamente.

## Incremento 2026-10-21: writer concorrenti e crash del processo

`sync_one` acquisisce un flock POSIX esclusivo per stazione prima del download
e lo mantiene fino alla sostituzione della cache. Richieste della stessa
stazione non possono quindi scaricare e commettere generazioni fuori ordine
fra writer cooperanti dello stesso cache root. L'attesa è limitata a 2 secondi:
se la stazione è occupata non si scarica né si sovrascrive il file precedente.
Le altre stazioni hanno lock distinti; i lettori non prendono il lock.

Il file vuoto `.ICAO.sync.lock` resta sul disco intenzionalmente: cancellarlo
mentre esiste un titolare può creare due domini di lock. È privato dell'utente,
deve essere regolare/non symlink e non scrivibile da altri. Il kernel rilascia
il lock alla chiusura del descriptor o alla morte del processo, senza TTL
basati su clock civile. Nessun comando di pulizia dei lock viene introdotto.

Sync è destinato a Linux/POSIX; senza flock rifiuta l'aggiornamento prima di
creare la directory, mentre il percorso lettura/serving non richiede flock.
Non copre writer esterni che ignorano il protocollo, NFS/filesystem distribuiti,
power-loss o i crash durante ogni fase I/O. `--resolve` usa un override DNS
process-global preesistente: più sync con mapping diversi nello stesso processo
threaded erano fuori dal contratto; il successivo incremento elimina quel
resolver globale, senza dichiarare qualificata la connessione provider reale.

35/35 test PASS Linux serverha e 35/35 ARM64 in directory isolate: contesa,
stazioni distinte, timeout, symlink rifiutato, recovery dopo errore e SIGKILL
del solo processo fixture con cache precedente preservata. Nessun processo
produttivo è stato terminato. Evidenze `weather-cache-lock-20261021/`.
Nessuna attivazione, reboot, modifica ai timer o alla cache di produzione.

## Sviluppo successivo: connessioni isolate e refresh multi-stazione

L'override `--resolve` non modifica più socket.getaddrinfo: crea una connessione
HTTPS privata che cambia solo l'indirizzo TCP. Host HTTP, SNI e verifica del
certificato restano sul nome richiesto. Con pin esplicito la connessione è
diretta e non usa proxy d'ambiente; un URL non HTTPS o hostname diverso viene
rifiutato. L'opener non viene installato globalmente. I redirect HTTP sono
sempre rifiutati; non si inoltra Basic Auth verso destinazioni redirette.
Context TLS senza verifica certificato/hostname non ammesso.

`sync_many` normalizza/deduplica le stazioni e continua sulle successive se un
fetch fallisce. Mantiene i cache file precedenti e restituisce un rapporto
PARTIAL_FAILURE con il solo tipo d'errore, non messaggi/URL potenzialmente
riservati. La CLI conserva la mappa ICAO su stdout, scrive il rapporto sync su
stderr e termina exit 2 se uno o più refresh falliscono. Non maschera quindi
la failure al timer, pur aggiornando le stazioni funzionanti. Nessun timer
o servizio installato viene modificato in questo blocco.

Su richiesta del proprietario: campagne complete, collaudi integrati e ARM64
del nuovo incremento rinviati. Eseguiti soltanto sintassi e sette controlli
mirati Linux isolati, incluso HTTP loopback sintetico. Non è prova TLS con
certificati/provider reali né regressione completa della nuova versione.
