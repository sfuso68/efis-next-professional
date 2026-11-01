# SIL session frame e comandi V1

2026-10-26, R1-22/P01. DEVELOPMENT CONTRACT, solo oggetti OFFLINE.
Specificazione macchina: `../../config/protocols/sil-session-icd-v1.json`.
Validatore: `tools/simulation/session_contract.py`; vettori sotto tests/data/protocols.
Non è un nuovo protocollo avionico distribuito né un ingresso remoto installato.

## Ambito e compatibilità

Questo contratto prepara il producer indipendente P02 e la console P03.
Non sostituisce fuso-avionics.physics-sil.v1 o sil-live.v1, non cambia
DDP V2/EDP V1, non è accettato dall'adapter R1-20 e non estende la sua chiave.
Un adapter esplicito deve convertire il vecchio frame e conservare raw/unità/
validità; nessuna conversione permissiva per sola somiglianza del nome.
Il codice importa soltanto stdlib, legge il contratto locale e valida oggetti:
nessun socket, shell, FDM, servizio o API Pixhawk/attuatori.

## Envelope e manifesto

Schema `fuso-avionics.sil-session.v1`, kind MANIFEST/FRAME/COMMAND/ACK.
session_id token1–64 ASCII,epoch intera non negativa,simulated=true e
flight_approval=false in ogni oggetto. Nessun endpoint/path/eseguibile libero.
JSON massimo65536byte UTF-8; chiavi duplicate, campi extra, NaN/Infinity,
booleani al posto di numeri e interi oltre2^53-1 sono respinti.
Questi sono limiti del formato diagnostico SIL, non nuovi budget freshness.

MANIFEST fissa LIVE/OFFLINE,UTC scenario ISO con Z,100Hz/20Hz,node65000,
hash config/model-tree,commit40hex,profilo SYNTHETIC_FIXTURE o
REFERENCE_UNVALIDATED. Solo questi due livelli ammessi nella V1: nessuna
promozione a modello approvato attraverso un messaggio. event_ids e station_ids
sono token; route_hashes sono digest64hex. Massimo64 elementi univoci ciascuno.
I digest attestano identità/integrità, non autenticazione o esistenza dell'oggetto.

## Frame atomico

FRAME conserva identità/epoch/hash manifesto,sequence,tick,sim_time_ns e
producer_mono_ns. Prima emissione sequence0/tick0; successive tick=5*sequence
e tempo SIM=tick*10000000ns. Sono tick relativi all'epoca: dopo resume/seek
la futura P02 deve aprire epoca nuova, conservando offset e parent nel log.
Il validatore verifica la relazione nel singolo frame; ordine fra frame,
reboot, replay/duplicate, continuità monotona e lifecycle spettano alla P02.

Tre gruppi completi:17 segnali flight,18 engine,5 airframe. Registro unità
univoco nel contratto. Assenza di canali non è assenza di chiave: il campione
resta presente UNAVAILABLE/null. Airframe separa flap comandato/posizione e
carrello comandato/lock; la topologia completa e i singoli lock sono futuri.
Non classificare il registro V1 come modello esaustivo di un impianto reale.

Ogni campione ha value/raw/unit/validity/SIM/source_node/channel/sample_tick/
age_ms/quality/calibration_revision/profile_revision. Source test65000;
sample_tick non futuro,age_ms almeno(tick-sample_tick)*10ms. L'età può essere
maggiore per ritardi precedenti: non la si azzera al ricevimento. Questa
formula è un controllo interno al medesimo clock SIM, non confronta host.
UNAVAILABLE e INVALID richiedono value/raw=null; STALE/LOST possono conservare
l'ultimo valore finito con SIM=true. Booleani reali soltanto nei segnali bool.
Le soglie STALE/LOST runtime restano quelle del core/ADR-017: il validatore
non inventa classificazioni temporali né rende valido un valore fisiologicamente
plausibile. Il confronto fisico/unità/datum completo richiede adapter e oracle.

Set prodotti completo: NAV,WEATHER,RADAR,TERRAIN,OBSTACLES,AIRPORTS,ROUTE.
Ogni entry è riferimento hash con stato e observed/valid_until in clock SIM;
UNAVAILABLE/INVALID richiedono campi null. SIMULATED non ammette osservazione
futura o validità scaduta; STALE esplicito permette l'archivio del prodotto
scaduto. Non si leggono file o URL da questo oggetto. Raw METAR/TAF, immagini,
CRS/datum/licenza e copertura restano nel pacchetto prodotto hash-bound P06/P07.

## Comandi e acknowledgement

COMMAND vincola sessione/epoch,command_id,sequence>=1,authority_id e lease_id.
Il chiamante autenticato fornisce current_tick e lease_until_tick locali
autorevoli. apply_tick deve essere strettamente futuro,entro100tick e la
scadenza lease; expires_tick>=apply_tick e dentro gli stessi limiti. Una
lease già scaduta rifiuta tutto.100tick è l'orizzonte dei comandi, NON una
soglia di salute del trasporto. STOP qui è schedulato; il futuro kill/restore
di sicurezza resta un meccanismo indipendente non bloccato dalla coda.

| Azione | Argomenti ammessi |
|---|---|
| START/STOP/PAUSE/RESUME | Nessuno |
| SET_CONTROL | control e value; assi/trim[-1,1],throttle/mixture/freni[0,1] |
| SET_OBS | degrees_magnetic in[0,359.999999] |
| SELECT_NAV | station_id già congelato nel manifesto |
| LOAD_ROUTE | digest rotta già congelato nel manifesto |
| SCENARIO_EVENT | ID evento già congelato,senza parametri/path liberi |

Eventi di guasto/meteo/interruttori/flap/carrello si riferiscono a configurazioni
congelate: questa V1 non permette di inviare da rete leggi fisiche o script.
Valori e capability specifici del modello sono un secondo gate obbligatorio
P03/P05: un SET_CONTROL sintatticamente valido non prova che il modello
supporti mixture o che un asse sia armato. Non c'è autorità verso hardware.

Il validatore restituisce validation=PASS,application=NOT_APPLIED. Non
emette QUEUED/APPLIED né autentica l'autorità: sono responsabilità della
futura coda/executor P02/P03. Un retry identico deve ricevere lo stesso ack
registrato senza riapplicazione; stesso command_id con contenuto diverso è
DUPLICATE_CONFLICT. Queste sono regole normative, non esecutore già presente.
Dedup va mantenuta per tutta la run bounded o fallire al limite,non eliminare
silenziosamente vecchi ID e renderli nuovamente applicabili.

ACK correla sessione/epoch/command_id/sequence. Stati REJECTED/QUEUED/APPLIED;
reason deve appartenere all'enumerazione. QUEUED richiede NONE e tick=null;
REJECTED richiede motivo diverso da NONE e tick=null; APPLIED richiede NONE e
tick dentro apply/expires. Verificare un ack non dimostra che la fisica abbia
applicato il comando: serve evento nel truth-log e confronto dello stato.

## Lifecycle da implementare

CREATED→VALIDATED dopo manifest/config verificati; VALIDATED→ARMED_TEST solo
con isolamento e gate di banco; ARMED_TEST→RUNNING tramite comando autorizzato.
RUNNING→PAUSED conserva log ma non rinnova misure ferme; ripresa crea epoch
nuova con nuovo stream instance. RUNNING/PAUSED→STOPPING→COMPLETE dopo flush
e restore. FAILED è terminale per violazione/infrastruttura: log failure e
restore prima di una nuova run. COMPLETE e FAILED non tornano RUNNING.
Questa tabella non è implementata come macchina a stati dalla P01.

## Mappa dei gap verso i trasporti canonici

| Dominio | Esistente verificato | Gap e destinazione della modifica |
|---|---|---|
| Sessione/epoch/config hash | Envelope DDP/EDP con publisher_instance/sequence; link SIL R1-20 separato | UUID/epoch/hash non si aggiungono ai datagrammi congelati; mapping/manifest stream test e correlazione P02/P04 |
| Flight | DDP23 campioni; adapter R1-20 per aria/assetto/GNSS/OBS/CDI |17 segnali SIL non coincidono con i23 wire; mappa esplicita per convenzioni,VS e datum prima del publisher |
| Motore | EDP18 campioni,wire-check offline | Publisher live da stessa sessione,provenienza/health,conversione revisioni e PFD engine-deck non ancora collegati |
| Airframe | Stato impianti/UI dimostrativi | Comandi/posizioni/lock indipendenti: contratto dedicato prima di wire; non sovraccaricare EDP |
| NAV | Campioni CDI/OBS/GS parziali in DDP | Identità stazione,piano/leg,XTK/DTK,capability e annunci richiedono oggetti applicativi versionati |
| Meteo | Cache/parser/overlay esistenti e mondo SIL offline | Pacchetti SIM isolati con tempi/CRS/hash verso adapter reali P06; nessun nuovo campo DDP |
| Terreno | Pacchetti/cache installati e SVS | Bind dataset/datum/copertura/sessione P07/P09,nessuna copia DEM nel frame |
| Comandi | Nessun ritorno dai display sul data-plane | Control-plane test distinto,autenticazione/coda/lease/esecuzione P03; chiave R1-20 immutata |
| Ack/eventi/ore | Recorder e spool consultivi distinti | Audit test e dedup,ore SIM separate; nessuna scrittura ore operative |

Riferimenti: codice sil_panel_replay.cpp,jsbsim_wire_check.cpp; protocolli
DISPLAY_DATA_PROTOCOL_V2.md/ENGINE_DATA_PROTOCOL_V1.md e ADR-003/010/011/012/017.
La mappa censisce gap, non autorizza revisioni del wire o nuovi endpoint.

## Gate P01

Vettori manifest/comando/campione nominali e mutazioni esercitano i validatori
offline; CTest registra la suite. Il pacchetto resta PARTIAL finché review del
contratto/mapping e correlazione semantica con i futuri adapter non sono chiuse.
P02 può usare questo candidato in test isolati,ma non sostituire il protocollo
live R1-20 sui nodi senza gate e verifica dedicati. Nessuna qualifica hardware.
