# SIL motore di sessione software V1

2026-10-26, R1-23/P02 PARTIAL. DEVELOPMENT / NON-CERT.
Codice: `tools/simulation/session_engine.py`; test: `tests/test_sil_session_engine.py`.

## R1-25 recorder asincrono di test

2026-10-27: `--journal-mode async-test` è opt-in;default resta sync.
`async_journal.py` avvia un writer separato con spawn. Il manifesto/hash
config lega la policy `ASYNC_TEST_V1`:accettazione QUEUED_NOT_DURABLE,
massimo16record pendenti/512KiB,record massimo64KiB e watchdog ACK100ms.
I100ms del producer non cambiano. La nuova semantica NON è «persistito prima
di distribuire»:in questo solo modo di banco si distribuisce dopo accodamento,
con fino a100ms di dati non ancora attestati durevoli. SIM non è un volo.

Writer concatena al massimo8record per batch e fa un fsync prima di inviare
ACK cumulativo DURABLE. Producer misura l'età osservata dell'ACK dal proprio
enqueue,non assume latenza zero. Ogni pump controlla il watchdog anche fra
i frame20Hz;la pausa del runner mantiene il controllo senza avanzare FDM.
Coda piena,byte budget superato,ACK tardivo,crash/errore writer fermano la
run in FAILED. Nessun drop silenzioso del truth-log. Close termina il figlio
in errore senza svuotare una coda ormai fallita;le evidenze su disco restano.

Startup attende READY massimo5s prima della fisica;drain/finalizzazione worker
massimo3s dopo la fisica. Questi limiti di lifecycle non sono soglie di
freshness e non estendono il watchdog100ms dei record pendenti.
Il risultato del writer in writer-private è candidato:solo il result.json
alla radice è autorevole,dopo copia esclusiva,checksum/conteggi e fsync.
Operazioni finali di copia/metadata nel parent sono fuori dal loop temporizzato
e non ancora protette da watchdog di processo. Directory fsync/power loss e
I/O kernel in stato non interrompibile restano non qualificati;un SIGKILL
non garantisce cessazione immediata di una syscall in D-state.

Test di errore iniettano hang,crash e disk-full;non sono distacchi fisici o
fault sul disco operativo. Nessuna modifica al recorder Blackbox,ai suoi log,
ai permessi SSH o alle configurazioni dei pannelli. La modalità non va
promossa a prodotto sulla base di queste sole prove.

## R1-24 adapter fisico candidato

2026-10-27: `session_jsbsim.py` collega Backend JSBSim1.3.3 al nuovo motore
in un processo isolato senza rete. Conserva PHYSICS_WORLD raw nel journal
prima del FRAME normalizzato. `session_trace.py` verifica integrità e mapping
del log,non fedeltà rispetto a un aeromobile reale. Le sezioni R1-23 sotto
restano la baseline storica;il limite «non ancora collegato» è superato solo
per le prove fisiche espressamente registrate in R1-24,non per i pannelli.

Mapping:GNSS da world.navigation.gnss (non dalla posizione verità),heading
MAG e aria/quote dai sensori del mondo;CAS/TAS/assetto/motore dal FDM. Le
etichette degMAG/degTRUE diventano unità deg nei campi nominalmente distinti.
Pressioni raw assenti,comandi/lock carrello e per-cylinder assenti rimangono
UNAVAILABLE. NAV/WX restano nel trace raw:nessun riferimento prodotto valido
o nuovo publisher di cache viene inventato. Rollout grafico e DDP/EDP aperti.

Il runner accetta1–60s,realtime1x e pausa owner opzionale,tre probe in-memory:
fast letto,slow/absent non letti. Nessuna delle probe è un pannello reale.
Configurazione scenario/world/durata e modello legata a digest e commit.
Deadlines mantengono fase per il jitter normale;se una deadline futura non
esiste più si rinvia senza raffica e si conserva il ritardo globale con
stop oltre100ms. Soglia invariata,non qualifica hard-real-time.

### Risultati del27ottobre e gate storage

Source-v2 corrisponde al commit fec6507;archivio SHA256
76707d366b53f2ce424930e9b7e0ebdafedf7ffa67feeb7b8b8d634cae29928f.
111test serverha PASS,inclusi5nativi;106Windows e8CTest mirati PASS.
Le due run su disco ext4 falliscono per frame/persistenza>100ms a12.6s
e14s fisici. Dati falliti conservati sotto runs/r124-physical-60s-pause-v1/v2;
nessun risultato COMPLETE. Questi fail non scompaiono perché una prova RAM passa.

Run diagnostica r124-memory-v1:journal dentro tmpfs64MiB del container,
poi copia esclusiva in runs/r124-memory-archived-v1 e verifica hash/trace.
6000tick/1202frame/2epoch,60.120529s incluso pause100ms,maxlag3.460428ms.
Probe fast1200ricevimenti,slow1201overwrite;nessuna prova che tutti i frame
siano stati ricevuti o disegnati da un pannello. GNSS assente201frame
coerenti con fault20–30s e resume a25s. Trace hash:
4f6df98200e8ab09892d908aad650ad5609aa3e4c84fd08151d506d8052375f6.
Verifiche nel container prima/dopo copia e dopo trasferimento Windows PASS.

Questo isola un percorso funzionante senza persistenza sincrona sul disco,
ma non prova che fsync sia la sola causa né rende durevole la RAM. Il
passaggio a recorder asincrono bounded/processo e watchdog richiede prove
overflow/disk-hang/crash,policy di stop e dichiarazione esplicita del limite
di dati non ancora persistiti:non cambiare implicitamente append-before-publish.
Fino ad allora registrazione live su disco/endurance restano FAIL/OPEN.

Comandi ripetibili dalla copia esatta del source nel container isolato:

```sh
python -B -m tools.simulation.session_jsbsim --directory /tmp/NUOVA_RUN \
  --world config/simulation/treviso-belluno-world-reference.json \
  --session-id NUOVA_RUN --source-commit fec6507cacd797561ff3c9bb4bfb6b59b6501ed4 \
  --seconds 60 --pause-tick 2500
python -B -m tools.simulation.session_trace /tmp/NUOVA_RUN
```

Sono comandi software senza pannelli,non un'autorizzazione ad aumentare
le lease di banco,aprire reti o usare RAM come blackbox operativa.

## Percorso implementato

SessionEngine possiede manifest congelato,backend e registro di verità test.
Un unico thread proprietario esegue lifecycle e pump; consumer esterni
possono soltanto prelevare pacchetti immutabili. Non si invocano callback
consumer dal producer. Il loop run non attende richieste o ACK dei pannelli.
Nessun socket,porta,processo systemd o modifica all'adapter R1-20.

Interfaccia backend: step() avanza esattamente un tick fisico; frame(manifest,
relative_tick,mono_ns) genera il frame ICD candidato. Entrambe sono dipendenze
fidate sincrone; non devono fare I/O non limitato. La fixture dei test conta
i tick,NON simula un aeromobile. Il nuovo scheduler non è ancora collegato
al Backend JSBSim esistente: servono mapping P01 e prove native dedicate.

## Lifecycle e clock

CREATED→VALIDATED→ARMED_TEST→RUNNING; pause porta a PAUSED,resume incrementa
epoch e riapre tick/sequence relativi da0 senza resettare il backend o
total_ticks. Il log EPOCH conserva parent_total_ticks. PAUSED non fa step,
non pubblica e poll non restituisce il vecchio frame. RUNNING/PAUSED o un
banco ARMED_TEST possono terminare STOPPING→COMPLETE dopo finalizzazione.
COMPLETE/FAILED non si riavviano. Transizioni invalide respinte prima di
alterare il backend. Armo software non verifica isolamento di una rete reale.

LIVE1x soltanto;clock monotonic_ns iniettato per i test,tick10ms,frame ogni5
tick inclusa emissione iniziale. Registro di verità contiene snapshot20Hz
ed eventi,NON tutti gli stati interni FDM100Hz. Il contatore globale resta
indipendente dai prelievi consumer e dai reset relativi di epoch.

pump esegue al massimo un passo quando la deadline è raggiunta. Non salta
tick fisici e non recupera a raffica. Dopo uno step lento la prossima
deadline è futura. Il ritardo accumulato rispetto al clock ideale dell'epoca
è misurato e causa FAILED oltre100ms,senza aumentare il limite esistente.
Il criterio è conservativo: ripetuti piccoli ritardi possono interrompere
una run anche se nessuna singola operazione supera100ms. Non è pacing
JSBSim qualificato né una promessa hard-real-time.

Clock non intero/non finito/invertito,frame di backend incoerente,overrun,
errore backend o persistenza bloccano la sessione e non producono nuovo
successo. Un passo backend non è annullabile dopo l'errore: conservare il
contatore/log e creare una nuova sessione,non fingere rollback fisico.

## Distribuzione bounded

Massimo8 consumer registrati prima della validazione;un solo slot ciascuno.
Frame JSON serializzato una volta e condiviso come bytes immutabili. Un
consumer lento riceve solo il più recente,con contatore overwritten. Lock
consumer occupato: publish non attende,incrementa busy_drops e serve gli
altri slot. Questo verifica contenimento software,non priorità OS o latenza
wire. Il prelievo scarta epoch vecchie;alla pausa/stop non espone dati freschi.
I consumer non estendono lease o aggiornano timestamp del producer.

Accessi lifecycle riservati al thread owner. La futura console P03 dovrà
inviare eventi attraverso una coda bounded verso quel thread;non chiamare
pause/step da un callback Qt o thread di rete. Nessun comando P01 è già
applicato da questa libreria e nessun ACK APPLIED è emesso.

## Registro e risultato

TruthJournal crea soltanto una directory nuova sotto il percorso esplicito
del chiamante;nessuna sovrascrittura,retention o accesso ai log operativi.
Append JSONL con fsync per record,hash SHA256 progressivo e conteggio.
Frame validato e registrato PRIMA della distribuzione. Errori write/short
write/fsync causano FAILED. Il journal è sincrono: una write kernel appesa
non può essere interrotta da questa classe;watchdog/process isolation e
qualifica dello storage restano necessari prima di runtime distribuito.

END_OF_DATA nel log NON è successo. Dopo chiusura truth e fsync di un file
risultato candidato,pubblicazione atomica esclusiva crea result.json:rename
senza sovrascrittura su Windows,hardlink esclusivo su POSIX (rename POSIX
potrebbe sovrascrivere). Solo result.json è completo;un eventuale
result.pending.json è candidato e non va interpretato come successo. Su
POSIX filesystem senza hardlink fallisce chiuso,senza fallback permissivo.
Primo tentativo Windows hardlink negato dal sandbox:log conservato,nessun
permesso aumentato;percorso Windows finale verificato con rename esclusiva.
Il risultato contiene
COMPLETE_SOFTWARE_ONLY,hash/conteggio verità,SIM e hardware_acceptance=NOT_IMPLIED.
Fsync directory,power-cut tolerance,readback/restore e firma non sono qualificati.

## Prove e limiti

19 test nuovi: lifecycle/terminali,transizioni errate,assenza consumer,slot
lento,lock occupato,pause/restart epoch e pacchetto vecchio,overrun prima
dello step,assenza burst,clock invertito,frame errato,storage/chiusura guasti,
backend in stallo,manifest immutabile,limiti consumer,owner thread e bytes,
loop100tick/21frame,hash del journal reale su file temporanei e rifiuto
di sovrascrittura del risultato. Percorso POSIX hardlink ancora NOT_RUN.
Il loop da un secondo usa clock virtuale: non è una misura di performance
o una run fisica JSBSim. Le prove precedenti R1-20 restano riferite al vecchio
link,non attestano automaticamente questo producer.

Rimangono:adapter JSBSim con conversioni esplicite,prova temporizzata reale,
replay del nuovo log,watchdog storage,coda/lease/dedup comandi P03,codec/fanout
P04,soak e consumer reali multi-nodo. Nessun nodo è stato contattato o
attivato dal presente incremento;power/touch/golden/release restano gate.
