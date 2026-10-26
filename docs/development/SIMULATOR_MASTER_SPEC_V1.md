# Fuso Avionics Next simulatore integrato e piano di completamento

Data: 2026-10-26. Specifica di sviluppo V1, DEVELOPMENT / NON-CERT.
Requisito padre: AVX-G5-02; ID collegati nella Master Matrix esistente.

Il simulatore serve a sviluppare e verificare le vere applicazioni EFIS NEXT,
non a costruire una grafica di volo alternativa. Una sola sessione fisica su
serverha deve alimentare PFD, MFD/Canvas, EMS e servizi di banco con stato,
tempo e guasti coerenti. Il completamento del SIL non rende il prodotto
approvato al volo e non sostituisce calibrazione, prove ambientali o hardware.

Questa specifica consolida il disegno da implementare. Non estende i permessi
di accesso, la durata dei launcher R1-20 o le ADR accettate. Il piano macchina
`../../config/simulation/development-plan-v1.json` registra dipendenze e
consegne; il suo controllo strutturale non è una prova dei moduli elencati.

## 1 Stato di partenza e lacune

| Componente | Stato dimostrato | Lavoro necessario |
|---|---|---|
| Fisica JSBSim 1.3.3 su serverha | C172P reference, quattro scenari; integrazione100Hz e frame20Hz | Pilotaggio, missione al suolo e modelli specifici validati |
| Mondo meteo e NAV | Modello analitico offline con fault, radiofari fittizi e prodotti SIM | Consumer reali, prodotti dinamici, procedure e database applicabili |
| Link diretto PFD | Due run60s/1201frame,RTTmax circa61.3ms,SSH ristretto | Producer indipendente dai consumer, fanout e durata qualificata |
| PFD reale | Sottoinsieme DDP SIM in namespace loopback, restore verificato | Touch aperto, mapping completo e nuova acceptance grafica |
| EDP e consumer motore | Codec/consumer offline reali verificati | Publisher live comune e pagine EMS alimentate dalla stessa sessione |
| Motore e impianti | C172P con canali mancanti UNAVAILABLE; demo C++ separate | Modello termico/elettrico/carburante e profili UL/ULS distinti |
| Archivio | Sessioni e hash su serverha | Catalogo SIL, replica NAS e restore della sessione completa |
| Multi-nodo | Non dimostrato dal link singolo | Blackbox di TEST, PFD/MFD/EMS insieme dopo gate power/release |

Riferimenti verificabili: evidence R1-14…R1-20 in `../status/evidence/` e
`../status/EFIS_NEXT_STATE.md`. I demo SystemSimulator/MissionSimulator
restano fixture utili, ma non sono prove di prestazioni reali e non possono
generare localmente stati indipendenti durante una sessione SIL distribuita.

## 2 Architettura e responsabilità

```text
Console istruttore e ingressi pilota di TEST
                   |
         comandi validati e registrati
                   v
serverha: orchestratore e clock -> FDM -> impianti -> sensori/guasti
                   |                            |
             scenario/mondo --------------------+
                   v
         frame atomico e registro verità
                   |
       normalizzatore/broker esclusivo di TEST
         |                  |                  |
     DDP canonico       EDP canonico       oggetti applicativi
         |                  |              NAV/WX/airframe
     PFD e MFD          EMS dedicato        servizi e viste reali
         +------------------+------------------+
                     evidenze consumer
                            |
                 archivio serverha -> NAS
```

Il disegno finale mantiene ADR-003/010: la Blackbox di TEST normalizza e
distribuisce, i display non interpretano MAVLink/CAN. Prima dei gate hardware,
lo stesso processo di broker test può essere eseguito in isolamento su
serverha; ciò è SIL software, non verifica della Blackbox fisica. Un eventuale
nuovo trasporto serverha→broker e i campi assenti nei contratti correnti
richiedono un ICD/ADR approvato prima dell'implementazione distribuita.
La chiave R1-20 rimane limitata al vecchio adapter bounded: nessuna nuova
facoltà di controllo, servizio generico o shell è implicita nel disegno.

Allocazioni: EMS su RPi4; FPV su un RPi4 distinto; PFD/MFD/Blackbox su RPi5.
Il server esegue fisica e archivio, non diventa router FLIGHT-DATA. Il nodo
sensori .89 resta una sorgente hardware separata; il SIL ne emula i campioni
solo nell'ambiente test. Nessun comando o fault raggiunge Pixhawk/CAN/servi.

## 3 Clock e ciclo di vita della sessione

Stati previsti: CREATED → VALIDATED → ARMED_TEST → RUNNING → STOPPING →
COMPLETE; qualunque violazione porta a FAILED e al ripristino del banco.
PAUSED è uno stato esplicito, non un flusso di vecchi frame resi freschi.

- Configurazione congelata prima dell'armo: ID run, revisioni, hash sorgente,
  runtime, modelli, dataset, seed, scenario, profilo, endpoint test e limiti.
- Un tick fisico100Hz produce verità; uscita snapshot20Hz. Queste frequenze
  riprendono il banco esistente, non qualificano il futuro carico massimo.
- Clock monotono locale per scheduling; tempo simulato da tick intero; UTC
  dello scenario separata dall'UTC di acquisizione. Mai sottrarre clock
  monotoni di host diversi per dichiarare latenza unidirezionale.
- LIVE a1x, niente seek o accelerazione nascosta. PAUSE annuncia pausa e
  lascia scadere le misure senza farle apparire volo attuale. Ripresa avvia
  nuova epoca di stream/istanza riconoscibile, conservando la sessione parent.
- OFFLINE consente seek, snapshot e velocità accelerate, senza socket di
  produzione. Seek/restore apre un segmento/epoca nuova e registra l'evento;
  non riutilizza sequenze vecchie in un flusso live.
- Il producer deve proseguire anche con un consumer lento o assente. Slot
  latest-state limitato per consumer, contatore esplicito degli scarti e
  nessun catch-up a raffica. La perdita del recorder di verità interrompe
  la run; la perdita di un display degrada quel display, non la fisica.
- Il vecchio limite lateness100ms resta invariato durante la migrazione.
  Le soglie consumer provengono da FreshnessBudget/ADR-017. Il progetto
  futuro deve misurare età completa inclusi coda/adapter/trasporto, non
  azzerarla a ogni hop. Nessuna soglia allargata per ottenere un PASS.

## 4 Contratto dei dati e dei comandi

Separare quattro livelli: verità fisica, uscita sensore, snapshot arbitrato,
stato presentato. Un fault del pitot non modifica artificialmente la TAS
fisica; un guasto reale motore modifica potenza, consumi e traiettoria.

Ogni frame conserva sessione/epoca, tick, sequence, tempo SIM, ricevimenti,
hash config e gruppi volo/motore/impianti/mondo. Ogni misura conserva valore
raw e ingegneristico, unità, validità, sorgente/nodo/canale, età, qualità,
revisioni di calibrazione/profilo e SIM persistente anche in STALE/LOST.
Assente è null/UNAVAILABLE, non zero plausibile. Autenticazione del canale
è distinta da CRC/hash e dal semplice source_node.

DDP e EDP restano distinti e usano codec canonici. Vietato aggiungere campi
ad hoc nei datagrammi esistenti: contratto versionato e vettori prima del
codice. Airframe, identità NAV/piano, meteo e sessione cross-protocollo hanno
lacune da censire; nessun JSON diagnostico è assunto già wire production.

Comandi ammessi nel solo control-plane SIL: start/stop/pause/resume, ingressi
normalizzati aileron/elevator/rudder/throttle/mixture ove modellata, trim,
freni, flap/carrello e interruttori impianto, selezione NAV/OBS/rotta, fault
e scenario meteo. Ogni comando richiede ID univoco, sessione/epoca, origine,
sequenza, tick di applicazione futuro bounded e ack APPLIED/REJECTED con
motivo. Duplicati sono idempotenti; comandi scaduti, non finiti, sconosciuti
o fuori inviluppo sono respinti e registrati. Range e unità sono per profilo.
Un'unica autorità controlla un asse alla volta; perdita lease comandi porta
a PAUSE/stop esplicito, non a un autopilota inventato.

Joystick, console browser ed eventuale adapter MSFS alimentano lo stesso
gate. Le applicazioni EFIS rimangono consultive; l'intento UI non diventa
automaticamente una posizione fisica di flap/carrello. Il piano non
autorizza nuovi endpoint di rete o autenticazioni: da progettare nel P01.

## 5 Fisica di volo e modelli aeromobile

JSBSim esistente è il backend autonomo. MSFS/SimConnect rimane un adapter
opzionale read-only: stessa normalizzazione e SIM, senza dipendenza Windows.
Non si estrae o redistribuisce contenuto commerciale senza diritti verificati.

Modelli richiesti: C172P reference per regressione; Asso 5 Jolly e Pioneer300
come profili distinti; Rotax912UL e912ULS come motori distinti. Somiglianza
di famiglia non prova masse, geometrie, CG, V-speeds o prestazioni equivalenti.
Configurare variante/seriale/installazione: elica, riduttore, serbatoi,
raffreddamento e carichi elettrici modificano il sistema risultante.

Il catalogo distingue SYNTHETIC_FIXTURE, REFERENCE_UNVALIDATED,
CORRELATED_WITH_REFERENCE e APPROVED_FOR_SPECIFIC_TEST. L'ultimo stato
richiede revisore, fonti, inviluppo e limiti del test, mai approvazione volo.
Ogni parametro conserva unità/fonte/revisione/pagina/licenza/incertezza;
campo necessario mancante blocca lo scenario, senza completamento AI.

La missione deve coprire cold-and-dark, avviamento, riscaldamento, taxi,
decollo, salita, crociera, virate/intercetti, discesa, avvicinamento,
riattaccata, atterraggio e shutdown. Distanze/tempi derivano dalla fisica,
non da una timeline fissa di dieci minuti. Pista, contatto suolo e vento
devono essere coerenti con la missione; collisioni/ground handling richiedono
modelli dedicati, non si assumono risolti dai replay attuali già in volo.

## 6 Motore carburante elettrico e airframe

Motore: RPM/albero-elica distinti ove applicabile, MAP, potenza/coppia,
consumo, temperature olio/refrigerante/teste/scarico, pressione olio e fuel.
Ogni cilindro modellato ha dinamica termica propria e accoppiamento al carico;
CHT/EGT aggregate C172P non si copiano in tutti i cilindri. Numero sonde,
tipo/unità e mapping derivano dall'installazione; mancanti restano tali.

Carburante: capacità utilizzabile/non utilizzabile, quantità indipendenti,
selettore, pompe, valvole, portate, perdite e starvation. Bilancio di massa
verificato su tutto il volo; il livello indicato include errore sonda e
calibrazione SIM separata. Ore/Hobbs/tach e manutenzione soltanto in archivi
test: nessun incremento delle ore operative dell'aereo.

Elettrico: batteria con capacità/SOC/resistenza interna e transitori,
alternatore/regolatore, bus principali/avionici/essenziali, fusibili/contattori
e utenze configurabili. Pompe, pitot heat, luci, avionica e attuatori
assorbono corrente; alternatore guasto scarica realmente la batteria nel
modello. La rappresentazione HMI deriva da topologia e stati, non da icone
verdi scollegate. Nessuna uscita commuta un'utenza reale.

Airframe: comando/posizione/feedback distinti per flap, carrello e trim;
velocità di movimento, detent, jam e sensori discordanti. Carrello fisso
non genera falsi sensori DOWN&LOCKED. Comandi e fault influenzano resistenza,
carico e impianto quando il modello lo prevede, altrimenti limite dichiarato.

## 7 Sensori e anomalie

Modellare catene indipendenti con frequenza, ritardo, bias, rumore seeded,
saturazione, quantizzazione e deriva; correlazioni deliberate per guasti
comuni. Pressioni statica/differenziale, IAS/CAS/TAS/GS, quota geometrica,
barometrica standard e indicata QNH restano quantità diverse. Heading TRUE,
MAG e ground track hanno convenzioni esplicite; GNSS loss non spegne VOR.

Fault fisici: perdita potenza, fuel leak/starvation, alternatore, bus,
raffreddamento, carrello/flap. Fault sensore: freeze plausibile, bias,
intermittenza, open/short, sensore invertito, discordanza e calibrazione
errata. Fault trasporto: delay/loss/burst/duplicate/reorder/restart/CRC.
Fault archivio: disco pieno, record troncato, checksum errato e restore.
Iniezioni solo su copie/namespace/storage test, mai sul recorder operativo.

Ogni fault ha ID, target, trigger temporale o condizionale, priorità,
intensità bounded, durata/rimozione, effetto atteso e osservatore indipendente.
Finestre [start,end), collisioni incompatibili respinte, eventi applicati
una volta. Si registrano causa ed effetto: una spia rossa da sola non
dimostra che il guasto sia nel modello né che il consumer lo rilevi.

## 8 Navigazione meteo terreno e database

NAV: piano versionato, leg/waypoint, sequenziamento, direct-to/nearest/
alternati, DTK/TRK/BRG/XTK, OBS/CDI/TO-FROM, VOR/NDB/DME/LOC/GS e perdita
identità/range. Sorgente, unità, convenzione angolare, validità e scala sono
visibili. RNAV e procedure richiedono specifica e dati applicabili dedicati;
non si dichiarano completi con il modello a cerchio massimo corrente.

METEO: atmosfera applicata al FDM e condizioni visuali coerenti; vento a
strati, raffiche/turbolenza, visibilità/nubi/pioggia. Ghiaccio e convettivo
avanzato sono modelli separati, disabilitati finché implementati e correlati.
METAR/TAF SIM distinguono osservazione, emissione, validità e ricevimento;
TAF FM/TEMPO/BECMG non è semplice persistenza. Radar dinamico con bbox/CRS,
timestamp, sequenza frame e fault delayed/offline/skew. Usare parser/cache/
adapter reali su directory SIM: niente sostituzione dei prodotti operativi.

TERRENO: DEM/texture/ostacoli/piste e mappe provengono dai pacchetti server
versionati e licenziati; regione iniziale Treviso–Belluno/Dolomiti, poi Nord
Italia. Coordinate e datum verticale espliciti e trasformazioni verificabili;
quota di uno scenario non si assume equivalente a EGM2008 senza verifica.
Pista prospettica e TAWS richiedono dati validi; gap terreno/ostacoli significa
UNKNOWN, non pianura sicura. Test: bordo tile/LOD, pianura, pendii, ostacoli,
cache incompleta/offline, destinazione/alternato e download ripreso.

## 9 Applicazioni servite e prove richieste

| Dominio | Input della sessione | Prova di chiusura software |
|---|---|---|
| PFD | Assetto/aria/quote/prue/health | Valori, scale, unità, SIM e STALE/LOST corretti |
| MFD e Canvas | Stesso frame, rotta e mondo | Nessun generatore indipendente, frame correlabili |
| EMS | EDP, impianti e profilo | Gauge/CHT/EGT/fuel/elettrico coerenti e indisponibili mascherati |
| NAV | GNSS/radio/piano/identità | Intercetti, source selection, loss/recovery e scale corrette |
| SVS e TAWS | Posa, DEM/ostacoli/piste | Datum, allineamento, gap e degradazione verificati |
| Meteo | Prodotti e cache SIM | Raw/parsed, expiry/skew, radar georiferito e offline |
| Blackbox | Snapshot/eventi/comandi test | Registrazione, replay e correlazione cause-effetti |
| Setup e profili | Copie versionate | Staging/validazione/rollback senza cambiare profilo reale |
| Emergency Assist | Guasti, performance e validità | SUSPECTED/SIM_ESTIMATE e checklist applicabili, nessun attuatore |
| Voce e radio assist | PTT/audio fixture e contesto | ACK distinto da sensore, grammar closed, nessuna trasmissione |
| FPV | Stream test e clock sessione | Freeze/loss/latency/overlay, quattro feed senza rubare risorse EMS |
| Accessori | Snapshot test normalizzati | Unità/validità/failure e compatibilità display piccolo |
| Health e manutenzione | Nodi/rete/storage/log | Fault containment, dati mancanti e ripristino tracciato |
| Standard e Professional | Capability manifest | Una codebase, combinazioni ammesse e feature ignote respinte |

Per FPV servono clip/stream sintetici marcati, non una nuova grafica esterna
del simulatore. Audio/telecamere/sonde/attuatori fisici restano gate hardware
separati. I test software possono usare ingressi fake senza fingere tali prove.

## 10 Console istruttore e archivio server

Console unica, separata dal cockpit: selezione modello/scenario/dataset,
check preflight, stato run e nodi, controlli pilota test, timeline fault,
meteo/radio, marker, metriche e stop/restore sempre evidente. Accesso solo
autenticato, con lease e audit; credenziali non nei log. Può essere usata
dal portatile, senza trasformarlo in source o relay obbligatorio.

Layout logico proposto sul server, da assegnare a storage reale dopo stima
spazio e verifica ownership: catalog/models, datasets, scenarios, runs con
manifest/frame/commands/events/products/consumer-evidence, reports, exports.
Oggetti immutabili content-addressed, indice ricostruibile e backup NAS.
Nessuna migrazione di /srv/stack o cancellazione dei run attuali in questa fase.

Il manifesto lega commit/runtime/model-tree/profili/config/dataset/hash,
licenze, tempi, seed, hardware/OS e limiti, risultato per gate e failure reason.
La copia NAS si dichiara verificata soltanto dopo checksum e restore in una
directory nuova, con replay offline e prodotti ricostruibili. Retention test
separata da log volo, pacchetti attivi e registrazioni esistenti; potature
richiedono piano e verifica copia. Dataset commerciali restano soggetti ai diritti.

## 11 Criteri di accettazione e indipendenza dei riferimenti

Una run ha almeno quattro risultati distinti: generazione fisica, integrità
trasporto, comportamento consumer e acceptance HMI. PASS del bridge non
dimostra frame disegnato. Registrare tick/epoca ricevuti, campioni scartati,
età, latenza misurabile, stalli e log di frame/UI dove disponibili.

Per ogni segnale/scenario congelare prima della prova riferimento indipendente,
tolleranza assoluta/relativa, inviluppo, warm-up e criterio temporale. Un valore
di tolleranza TBD blocca il gate di fedeltà; non si inventano soglie universalmente
valide per un Rotax o per il modello aerodinamico. I test di integrità hanno
invece criterio esatto: identità/schema/hash corretti, SIM conservato e nessuna
accettazione di comando/endpoint proibito. Copiare la stessa formula nel checker
non costituisce confronto indipendente.

Progressione target proposta: regressioni offline; live single-consumer60s;
multi-consumer10min; missione completa; soak2h su nodi autorizzati con carico
concorrenziale. Sono obiettivi da misurare, non qualifiche già raggiunte.
Per replay stesso runtime si verifica determinismo; confronto cross-platform
richiede tolleranze numeriche congelate, non promessa byte-identica.

Scenari minimi della release SIL integrata:

1. Missione nominale completa Treviso–Belluno con avviamento e shutdown.
2. Salita/virata/discesa e vento variabile con bilanci cinematici/energetici.
3. Engine-out con effetti su potenza/fuel/planata e assistenza SIM.
4. Perdita olio/surriscaldamento e differenze cilindri CHT/EGT.
5. Alternatore guasto, batteria in scarica e shedding carichi.
6. Fuel selector/pompa/perdita/starvation e bilancio per serbatoio.
7. Pitot/statico/bussola/GNSS con freeze, bias, loss e recovery indipendenti.
8. VOR/NDB/DME/LOC/GS e rotta/direct-to con identità e copertura degradate.
9. Meteo raw/parser/cache/radar con delayed/expired/offline/skew.
10. Terreno/pista/ostacolo e tile missing con TAWS UNKNOWN/SIM.
11. Carrello/flap jam e feedback discordante durante avvicinamento/riattaccata.
12. Rete lenta/consumer bloccato/restart: fisica continua e timeout visibili.
13. Recorder test pieno/troncato, backup e restore completo offline.
14. Emergenze/voce/PTT/ACK e audio assente, senza azioni hardware.
15. Quattro video test con freeze/disconnect e telemetria disallineata.
16. Setup/profili/unità/SKU e isolamento di ore/calibrazioni/credenziali reali.

Ogni scenario richiede nominale, fault, recovery, log oracle e gate per modulo.
Errori conservati; niente waiver automatico o cambi golden per rendere verde.

## 12 Sequenza di sviluppo e passaggio ai moduli finali

| Pacchetto | Consegna verificabile | Dipende da |
|---|---|---|
| P01 | ICD sessione/frame/comandi e test isolamento; mappa gap DDP/EDP/airframe | Specifica V1 |
| P02 | Producer a tick fisso indipendente, lifecycle/pausa/epoch e log truth | P01 |
| P03 | Ingressi pilota/console headless, lease/ack/idempotenza e replay comandi | P02 |
| P04 | Snapshot e fanout DDP/EDP con slow-consumer/restart/rejection | P02 |
| P05 | Motore/impianti/fuel/airframe e sensor model sintetici espliciti | P02 |
| P06 | NAV/WX dinamici, prodotti e contratti applicativi mancanti | P01,P02 |
| P07 | Catalogo modelli/dataset/licenze e restore server/NAS | P01 |
| P08 | Consumer reali PFD/MFD/EMS, profili coerenti, no generatori concorrenti | P03,P04,P05,P06 |
| P09 | SVS/TAWS/cache/route pack e scenari geografici con datum | P06,P07,P08 |
| P10 | Emergenze/voce/FPV/setup/accessori e fault trasversali | P05,P07,P08 |
| P11 | Correlazione modelli UL/ULS e aeromobili, oracle e tolleranze | P05,P07 |
| P12 | Missione/soak/multi-pannello, restore e matrice acceptance completa | P09,P10,P11 |

Primo incremento sicuro: P01, senza modificare wire esistente o avviare nodi.
P02 viene dopo i contratti, non aggirando il handshake R1-20 a richiesta.
P05 sintetico e P07 intake possono avanzare senza attendere manuali; P11 non
si chiude senza fonti e revisione. Test locali continuano durante lo sviluppo:
rimandare l'interazione hardware non significa rimandare tutte le verifiche.

Passaggio allo sviluppo finale per verticale, non riscrittura generale:
prima core/provenance/PFD/EMS; poi NAV/SVS/TAWS e WX/cache; quindi assistenza,
voce/FPV/accessori; infine release/backup/manutenzione/endurance. Ogni modulo
entra nella suite appena il suo contratto è pronto, senza attendere che
tutto il simulatore sia finito. Nessun modulo è DONE solo perché passa una
run sintetica: chiusura separata software, ARM64/HMI e hardware applicabile.

## 13 Blocchi e decisioni ancora necessari

- Fonti POH/AFM e manuali UL/ULS per variante/installazione; prestazioni e
  geometrie Asso/Pioneer non dedotte dalla somiglianza.
- Touch PFD, alimentazione MFD/Blackbox e golden/release prima di nuove
  acceptance fisiche. Nessuna nuova prova hardware è implicita nel piano.
- ICD per metadati cross-protocollo e domini mancanti, senza modifica
  unilaterale a formati congelati. Semantica pause/epoch da testare sui consumer.
- Configurazione storage/quote/replica dopo stima spazio; video e dataset
  grandi non inseriti nel Git. NAS restore SIL ancora da eseguire.
- HIL/autopilota/servi e validazione aeronautica esclusi da questa V1:
  richiedono decisione e piano separati, non estensione del forced-command.

Autorevoli: ADR-003/005/008/009/010/011/012/013/014/017,
`SERVERHA_HEADLESS_SIL_V1.md`, `SIL_LIVE_BENCH_V1.md`,
`SIL_RESTRICTED_SSH_V1.md` e Master Matrix. Le sezioni storiche dei vecchi
documenti non prevalgono sullo stato corrente o sulle evidenze più recenti.
