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
del parent non fanno parte della prova qui eseguita. Nessun writer concorrente
o ordine dei download simultanei qualificato da questo blocco.

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
