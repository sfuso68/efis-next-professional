# Archivio e ripristino delle run SIL V1

2026-10-28, R1-28/P07. DEVELOPMENT / NON-CERT, software di banco.

`python -m tools.simulation.session_archive pack --run RUN --archive NEW.tar`
verifica prima il truth trace e legge soltanto truth.jsonl,result.json e
probe-report.json. Poi crea un nuovo TAR non compresso senza sovrascrivere.
Il manifesto vincola SHA256/dimensione dei tre file,identità della sessione,
commit sorgente,hash config e modello,stato del profilo e confini SIM.
Non contiene XML di modelli,imagery,credenziali esterne o asset MSFS. La
revisione licenze e il catalogo dei dataset restano gate separati: un digest
di modello non prova diritti di redistribuzione o accuratezza aerodinamica.

`python -m tools.simulation.session_archive restore --archive NEW.tar --directory NEW_RUN`
accetta soltanto cinque nomi,in ordine:manifesto,tre payload,completion marker.
TAR massimo128MiB,truth massimo64MiB,altri record massimo64KiB. Non decomprime
input compressi e non usa extractall. Rifiuta duplicati,nomi sconosciuti,
traversal,link,device,sparse/PAX,header nascosti,footer troncato o payload
aggiunto dopo il footer. Non applica UID/GID/mode/path del TAR.

Tutti i controlli di struttura,hash e binding precedono la creazione del
target. La directory finale deve essere nuova,con parent già esistente e
fidato scelto dall'operatore;non usare cartelle condivise scrivibili da altri.
I tre file sono scritti in modalità esclusiva con fsync,poi viene rieseguito
il verificatore semantico/lifecycle del trace. Solo dopo PASS è creato
restore-complete.json,anch'esso fsync. Nessuna cancellazione o pulizia automatica.
Un errore successivo alla creazione conserva una directory incompleta,senza
receipt di successo. Non ritentare sopra di essa:conservarla come evidenza.

Il confronto stat/fstat include identità,size,mtime e ctime su POSIX. Su
Windows il Python locale ha mostrato ctime diversi per lstat/fstat sullo
stesso file invariato:qui si confrontano identità,size e mtime,oltre ai
payload completi prima/dopo la verifica. Non è protezione contro un processo
ostile con accesso di scrittura allo stesso spazio di lavoro.

Checksum e manifesto verificano integrità,NON autenticità. Non ci sono firma,
chiave di fiducia o certificazione di modello. fsync dei singoli file non
qualifica power-loss,fsync directory,retention,endurance o ripristino atomico
dell'intera directory. Il completamento è una receipt,non una promozione a
release operativa. Per archivi ricevuti da terzi serve trust separato.

## Gate residui P07

Catalogo versionato di runtime,profili,licenze e dataset;policy accessi;
replica del pacchetto verso NAS e ripristino da copia NAS indipendente;
catalogo geografico completo e restore delle dipendenze modello/runtime.
Questa V1 ripristina dati di run verificabili,NON un ambiente eseguibile o
un pannello avionico e non modifica il backup Git o il recorder operativo.
