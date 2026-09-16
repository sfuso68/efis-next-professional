# EFIS NEXT — Emergency Assist: perdita di potenza motore V1

Stato: **SOFTWARE V1 PARZIALE / PROCEDURA OPERATIVA BLOCCATA / NON OPERATIVO**

Data: 2026-09-09

Requisiti collegati: `PAN-001`, `PAN-002`, `EMS-004`, `NAV-002`, `SV-003`.

## 1. Decisione

La perdita totale o significativa di potenza del motore è il primo scenario di
Emergency Assist. La V1 riduce il carico di lavoro e presenta informazioni al
pilota, ma non cambia modalità ArduPlane, non muove i servi, non aziona
manetta, pompa, rubinetti, flap, carrello, radio o transponder.

Le azioni operative specifiche rimangono segnaposto finché non vengono
acquisiti e approvati POH dell'esemplare, revisione applicabile OM-912,
configurazione dell'elica e impianti installati.

## 2. Obiettivo misurabile

Entro un secondo dall'attivazione manuale, il PFD deve mostrare senza menu
intermedi:

- stato e affidabilità di IAS, assetto, AGL e vertical speed;
- velocità di miglior planata soltanto se approvata nel profilo;
- prima azione approvata della procedura;
- migliore opzione di atterraggio calcolabile e due alternative;
- chiara indicazione quando nessuna soluzione è dimostrata raggiungibile;
- eventuale degrado di GNSS, air-data, terreno, vento o database.

Il tempo di risposta è un requisito UI/software, non una garanzia sulla
correttezza della diagnosi automatica.

## 3. Attivazione

### 3.1 Percorso primario

Pulsante fisico `EMERGENCY` dedicato:

- pressione breve: apre la pagina Emergency Assist;
- se il sistema ha già una condizione `POWER_LOSS_SUSPECTED`, la relativa
  scheda è preselezionata ma richiede conferma del pilota;
- pressione prolungata non comanda il Pixhawk nella V1.

### 3.2 Percorso secondario

Pressione prolungata del simbolo Emergency sul PFD. Il touchscreen non è
l'unico metodo di accesso e non deve essere richiesto per uscire dalla pagina.

### 3.3 Rilevamento automatico

Il rilevamento automatico genera esclusivamente il messaggio
`POTENZA MOTORE? — CONFERMA`. Non attiva una checklist e non modifica il volo.

Non è ammesso un trigger basato su un solo segnale. Una proposta di perdita di
potenza richiede almeno:

- stato `IN_FLIGHT` affidabile;
- calo RPM persistente rispetto al contesto, non un singolo campione;
- conferma da almeno un indicatore indipendente disponibile, ad esempio MAP,
  fuel flow, coppia/potenza stimata o andamento energetico;
- esclusione delle fasi e configurazioni in cui il minimo è atteso.

Le soglie numeriche sono parametri approvati per aeromobile/motore, non
costanti globali.

## 4. Macchina a stati

```text
IDLE
  -> SUSPECTED             rilevamento multi-segnale; solo prompt
  -> PILOT_ACTIVATED       pulsante fisico/touch

SUSPECTED
  -> PILOT_CONFIRMED
  -> DISMISSED
  -> SENSOR_DEGRADED

PILOT_ACTIVATED
  -> PILOT_CONFIRMED       selezione POWER LOSS
  -> OTHER_EMERGENCY
  -> CANCELLED

PILOT_CONFIRMED
  -> FLY_PRIORITY
  -> DATA_INSUFFICIENT

FLY_PRIORITY
  -> LANDING_OPTIONS
  -> CHECKLIST_ACTIVE
  -> IMMEDIATE_LANDING     inviluppo/AGL insufficiente

LANDING_OPTIONS <-> CHECKLIST_ACTIVE
  -> APPROACH_SUPPORT
  -> NO_REACHABLE_AIRPORT
  -> POWER_RECOVERED

POWER_RECOVERED
  -> MONITORING            il pilota decide se continuare/chiudere

qualsiasi stato
  -> DATA_DEGRADED
  -> CANCELLED
```

Nessuna transizione elimina automaticamente la procedura perché gli RPM sono
tornati normali: il recupero può essere temporaneo e richiede conferma.

## 5. Priorità della presentazione

### 5.1 FLY

Ordine fisso:

1. mantenimento del controllo e indicazione assetto;
2. IAS corrente, qualità e velocità target approvata;
3. AGL, sink rate e tempo stimato al terreno con margine;
4. terrain/obstacle alert con precedenza audio.

Se la velocità di miglior planata non è approvata, la UI mostra
`BEST GLIDE NON CONFIGURATA` e non usa il valore legacy di 65 kt.

### 5.2 NAVIGATE

Il sistema propone:

- aeroporto raccomandato;
- fino a due alternative;
- se nessun aeroporto supera i gate, indicazione esplicita e settori di
  atterraggio potenziali, marcati `NON VERIFICATI`;
- bearing, distanza, pista, superficie, vento, ostacoli e margine stimato
  all'arrivo.

### 5.3 COMMUNICATE

La V1 visualizza un testo MAYDAY precompilato con nominativo, posizione, quota,
persone a bordo e intenzioni. Frequenza e codice transponder sono suggerimenti
da confermare e impostare manualmente.

### 5.4 MANAGE

Mostra soltanto azioni provenienti dal pacchetto procedure approvato. Le
sezioni possibili sono:

- verifica/riavvio motore;
- gestione carburante e pompa;
- accensioni;
- configurazione elica, se applicabile;
- configurazione carrello/flap per il tipo di atterraggio;
- spegnimento e sicurezza prima dell'impatto/atterraggio.

Questo elenco definisce categorie dati, non istruzioni operative.

## 6. Modello di planata conservativo

Il calcolo non usa semplicemente un cerchio basato sul rapporto L/D di
brochure.

```text
H_usable = AGL
           - terrain_clearance_reserve
           - manoeuvre_loss
           - model_uncertainty
           - final_approach_reserve

still_air_range = max(0, H_usable) * conservative_glide_ratio
```

Il footprint viene poi corretto per:

- vento con direzione, intensità, età e qualità;
- polar/velocità configurata e peso, se disponibili;
- perdita in virata e limiti di bank;
- terreno e ostacoli lungo la rotta, non solo alla destinazione;
- quota pista e margine necessario per l'avvicinamento;
- incertezza GNSS, DEM, QNH e air-data.

Se manca un modello di planata approvato, il sistema può mostrare aeroporti
vicini ma non etichettarli `RAGGIUNGIBILI`.

## 7. Selezione dell'aeroporto

### 7.1 Gate obbligatori

Un aeroporto è candidato soltanto se:

- si trova entro il footprint conservativo;
- esiste almeno una traiettoria priva di conflitti terrain/obstacle noti;
- lunghezza, larghezza e superficie della pista sono compatibili con il
  profilo approvato;
- il margine stimato all'arrivo supera la riserva configurata;
- i dati essenziali non sono scaduti o mancanti.

### 7.2 Punteggio

Dopo i gate, il punteggio considera:

1. margine di quota all'arrivo;
2. vento e componente traversa/coda;
3. lunghezza, larghezza, superficie e pendenza;
4. ostacoli e complessità della traiettoria;
5. servizi, frequenza e meteo disponibili;
6. numero e severità delle informazioni mancanti.

Il più vicino non è necessariamente il migliore. Il pilota può sempre
selezionare un'alternativa.

## 8. Caso speciale: avaria dopo il decollo

Il sistema non suggerisce automaticamente l'inversione verso la pista. Senza
un inviluppo di rientro specifico, dimostrato per quell'aereo, peso, vento,
quota e configurazione, il ritorno viene escluso dalle raccomandazioni.

La modalità `TAKEOFF POWER LOSS` privilegia:

- controllo dell'assetto e velocità;
- settore di atterraggio disponibile con minima manovra;
- ostacoli immediati;
- istruzioni POH dell'esemplare.

Un futuro modello di turnback richiederà prove dedicate e margini separati; non
sarà derivato da una semplice geometria circolare.

## 9. Contratto minimo dei dati

Ogni campo dinamico porta:

- valore e unità;
- sorgente;
- tempo monotono e UTC quando disponibile;
- validità, qualità e age;
- flag `SIMULATED`;
- motivo del degrado.

Campi minimi dello scenario:

| Gruppo | Campi |
|---|---|
| Flight | attitude, IAS, GS, altitude, AGL, VS, track, position |
| Engine | RPM, MAP, fuel flow/pressure, oil pressure/temp, coolant/CHT |
| Configuration | gear, flap, trim, fuel selector/pump quando sensorizzati |
| Navigation | airport/runway, terrain, obstacles, airspace |
| Environment | wind, QNH, METAR/TAF con freshness |
| System | Pixhawk, GNSS, air-data, Blackbox, database e power health |
| Procedure | package ID, revision, source pages, applicability, hash |

## 10. Stati degradati

| Perdita | Comportamento obbligatorio |
|---|---|
| IAS | nessuna velocità corrente presentata come valida; target POH distinto dalla misura |
| GNSS | niente aeroporto “raggiungibile”; mantenere solo checklist e strumenti validi |
| DEM/ostacoli | niente rotta dichiarata libera; avviso `TERRAIN DATA UNAVAILABLE` |
| vento | footprint ridotto con incertezza; nessuna assunzione vento zero |
| meteo | aeroporto non escluso automaticamente, ma meteo marcato `UNKNOWN/STALE` |
| motore | diagnosi automatica sospesa; attivazione manuale sempre disponibile |
| Blackbox/DDP | PFD mostra perdita dati; nessun dato congelato deve sembrare vivo |
| PFD | funzione non può influire sul controllo Pixhawk; checklist cartacea resta necessaria |

## 11. Registrazione Blackbox

Eventi minimi:

- trigger manuale o sospetto automatico e motivazioni;
- snapshot dei dati e relative validity al trigger;
- scenario selezionato e revisione procedura;
- ogni azione mostrata, letta, confermata, saltata o ripetuta;
- classifica aeroporti e motivi di inclusione/esclusione;
- cambiamenti di sorgente, perdita sensori e degradazioni;
- recupero potenza, chiusura e motivazione;
- nessun dato personale audio: si registrano eventi, non il microfono.

Il replay deve ricostruire esattamente ciò che il pilota ha visto.

## 12. Strategia di test

### 12.1 Software/replay

1. attivazione manuale con tutti i dati validi;
2. calo RPM falso durante discesa/minimo;
3. perdita potenza confermata da più sorgenti;
4. perdita immediatamente dopo il decollo;
5. nessun aeroporto entro il footprint;
6. aeroporto vicino ma dietro terreno non superabile;
7. vento forte e meteo stale;
8. IAS, GNSS, DEM o Blackbox persi singolarmente e insieme;
9. potenza recuperata e persa nuovamente;
10. riavvio PFD durante procedura e ripresa coerente dal recorder.

### 12.2 Human factors

- riconoscimento della prima azione entro il tempo obiettivo;
- leggibilità a 1024x600, sole simulato, vibrazione e guanti;
- massimo una decisione primaria per schermata;
- nessuna sovrapposizione audio con stall/terrain warning;
- prova con piloti non coinvolti nello sviluppo;
- confronto con checklist cartacea approvata.

### 12.3 HIL e hardware

Solo dopo i test software:

- replay sincronizzato di Pixhawk, EMS e posizione;
- fault fisico M10, ASPD-DLVR, Ethernet e alimentazione display;
- misure di latenza trigger-display-audio-log;
- verifica che nessun pacchetto sia trasmesso al Pixhawk;
- endurance e power-cycle durante uno scenario attivo.

## 13. Gate prima dell'implementazione operativa

- POH dell'esemplare acquisito, revisionato e approvato;
- OM-912 applicabile al seriale installato;
- best glide e polar/inviluppo conservativo verificati;
- profilo massa, elica, flap, carrello e carburante congelato;
- database aeroporti/terrain/ostacoli con provenance e freshness;
- IAS e AGL con health end-to-end;
- wording e flusso approvati da piloti/istruttore;
- contratto eventi Blackbox e test di replay;
- checklist cartacea disponibile come fallback.

## 14. Riferimenti di progetto

- Audit fonti: `docs/safety/EMERGENCY_PROCEDURE_SOURCE_AUDIT.md`
- Concept generale: `docs/design/EMERGENCY_ASSIST_V1.md`
- FAA Airplane Flying Handbook, Chapter 18:
  https://www.faa.gov/sites/faa.gov/files/regulations_policies/handbooks_manuals/aviation/airplane_handbook/19_afh_ch18.pdf
- Garmin Smart Glide, riferimento funzionale:
  https://www.garmin.com/en-US/autonomi/smart-glide/
- ArduPilot airspeed setup:
  https://ardupilot.org/plane/docs/airspeed-parameters-setup.html
- ArduPilot soaring/speed-to-fly, riferimento algoritmico non operativo:
  https://ardupilot.org/plane/docs/soaring-speed-to-fly.html

## 15. Evidenza Software V1

`efis::emergency::EmergencyAssist` implementa la parte deterministica della
macchina a stati e i gate descritti in questo documento. I test coprono
attivazione manuale, dati primari mancanti, mancata esposizione di best glide
non approvata, planata SIM non autorevole, esclusione per pista/rotta/raggio,
package checklist incompleto, avanzamento/skip e sospetto automatico con due
segnali e dwell.

Nel PFD reale l'auto-detection resta disabilitata: non esistono ancora soglie
approvate per l'esemplare. Nello scenario software `engine-emergency` sono
abilitati soltanto dati e passaggi sintetici, marcati `SIMULATION • TEST DATA •
NOT FOR FLIGHT`. Nessun numero sintetico viene promosso nel profilo operativo.
