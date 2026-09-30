# PFD recorder retention

Stato: **CANDIDATE / DEVELOPMENT / NON-CERT / NOT DEPLOYED**

Il recorder PFD usa un buffer rotante locale. La policy candidata nella unità
release V1 imposta:

- quota dati completati: 24 GiB (`25769803776` byte);
- riserva minima del filesystem: 12 GiB (`12884901888` byte);
- protezione minima: 120 segmenti completati recenti.

Con segmenti nominali da 3.000 campioni a 50 Hz, 120 file rappresentano circa
due ore. Il rate osservato al banco il 30 settembre 2026 è circa 3,6 MB/min;
24 GiB corrispondono quindi a circa cinque giorni, ma questa è una stima di
banco e non un requisito di conservazione approvato.

## Comportamento fail-closed

La retention è disabilitata per default e si attiva soltanto con
`--max-storage-bytes` o `--min-free-bytes`. Il recorder:

1. considera esclusivamente file regolari, non symlink, con nome
   `flight-*.csv`;
2. non tocca `.partial`, file operatore o file con nome estraneo;
3. ordina i segmenti per data di modifica e poi per nome;
4. calcola prima se può raggiungere quota e riserva senza scendere sotto il
   numero minimo protetto;
5. se il target non è raggiungibile, non elimina nulla e termina in errore;
6. altrimenti elimina soltanto i segmenti completati più vecchi necessari;
7. pubblica contatori V3 di segmenti/byte presenti, spazio disponibile e byte
   potati.

La verifica avviene all'avvio e dopo la pubblicazione atomica di ogni nuovo
segmento. Un errore di ispezione o rimozione resta un errore recorder visibile,
non un successo silenzioso.

## Gate prima dell'attivazione

La prima attivazione può rimuovere circa 8 GiB dall'attuale buffer PFD da
32 GiB. Prima del deployment sono quindi obbligatori:

1. copia dei record correnti in un percorso datato su `serverha`;
2. secondo `rsync -aHnc --delete --itemize-changes` con zero differenze;
3. build ARM64 della nuova release dual-role PFD/RECORDER;
4. validazione manifest e rollback drill;
5. activation e reboot acceptance;
6. soak di almeno 24 ore con recorder `NRestarts=0`, status V3 crescente,
   filesystem con almeno 12 GiB disponibili e prova di una rotazione reale.

La copia dei record e il deployment non sono impliciti nella presenza del
codice. Richiedono finestra autorizzata. Nessun risultato autorizza il volo.
