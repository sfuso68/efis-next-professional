# EFIS NEXT — Emergency Assist / “Panic” V1

Stato: **CONCEPT / NON IMPLEMENTATO / NESSUNA AUTORITÀ DI VOLO**

Data: 2026-09-09

Profilo: Professional; subset consultivo valutabile per Standard

## 1. Obiettivo corretto

Ridurre il carico cognitivo del pilota durante un'emergenza, presentando poche
azioni ordinate, dati affidabili, opzioni di navigazione e checklist specifiche
per l'aeromobile. Il nome mostrato all'utente sarà `EMERGENCY ASSIST`; “PANIC”
resta il nome informale della funzione.

La funzione non sostituisce il pilota, il POH/AFM, l'addestramento o un sistema
certificato. In V1 non invia comandi al Pixhawk e non comanda attuatori.

## 2. Principio operativo

L'interfaccia deve seguire sempre l'ordine:

1. **FLY** — assetto, velocità, quota/AGL e margine dal terreno;
2. **NAVIGATE** — rotta sicura, area raggiungibile, aeroporto o campo;
3. **COMMUNICATE** — frequenza, MAYDAY, posizione e transponder suggerito;
4. **MANAGE** — checklist approvata, impianti e configurazione.

Ogni consiglio deve indicare origine e affidabilità dei dati. Dati mancanti,
stale o simulati non possono essere presentati come certi.

## 3. Tre livelli distinti

### Livello A — Emergency Advisory V1

Realizzabile dopo la specifica delle procedure, senza cambiare il confine
read-only:

- attivazione con pulsante fisico dedicato e, come percorso secondario, comando
  touch a pressione prolungata;
- scheda a tutto schermo, leggibile con una mano e senza menu profondi;
- identificazione proposta dell'evento, sempre confermabile/correggibile dal
  pilota;
- istruzioni sintetiche visive e vocali tratte da procedure approvate;
- best glide, glide range conservativo, vento, terreno, ostacoli e aeroporti;
- aeroporto consigliato più alternative, con pista, superficie, lunghezza,
  orientamento, quota stimata all'arrivo e meteo con freshness;
- frequenza utile e azione transponder mostrate come suggerimenti, non eseguite;
- timer, pulsanti `FATTO`, `SALTA`, `RIPETI`, `CAMBIA EMERGENZA` e `ESCI`;
- registrazione completa su Blackbox dell'attivazione, dei dati usati, della
  revisione checklist e delle conferme del pilota.

### Livello B — Stabilize Assist futuro

Non appartiene a P2 e non è autorizzato dal presente documento. Se approvato,
deve richiedere al Pixhawk una modalità prevalidata; EFIS, PFD e Blackbox non
devono calcolare o inviare posizioni grezze dei servi.

Vincoli minimi:

- comando fisico cablato direttamente a ingresso Pixhawk/RC validato, non
  dipendente da Linux, Qt, Ethernet, Blackbox o touchscreen;
- `AP MASTER` e `AP DISCONNECT` fisici, con override immediato del pilota;
- annunci separati `ASSIST DISPONIBILE`, `RICHIESTO`, `AP ATTIVO`, `DEGRADATO`;
- attivazione inibita a terra e fuori dall'inviluppo approvato;
- scelta fra STABILIZE/FBWA/altro soltanto dopo tuning, HIL e prove in volo;
- nessuna dipendenza da Internet o da un modello generativo;
- perdita Blackbox/PFD/rete incapace di cambiare modalità o posizione servi.

### Livello C — Emergency Glide / Autoland

Fuori scope V1. I soli assi rollio e beccheggio non consentono di promettere un
atterraggio automatico. Servirebbero almeno architettura validata per manetta,
direzionale/coordinazione, flap, carrello, frenata, guida terminale, meteo,
runway/terrain integrity, ridondanza e un programma formale HIL/flight test.

## 4. Casi di emergenza V1

Ordine proposto di sviluppo:

1. perdita di potenza motore / planata;
2. disorientamento o assetto anomalo;
3. rischio stallo o velocità non sicura;
4. guasto elettrico / undervoltage / alternatore;
5. fumo o incendio motore/elettrico;
6. peggioramento meteo, IMC involontario o terreno;
7. perdita parziale di sensori/PFD;
8. emergenza medica o incapacità del pilota.

Il motore di regole può proporre un caso, ma il pilota deve poterlo selezionare
manualmente. Non deve eseguire automaticamente una checklist basandosi su una
sola misura.

## 5. Motore decisionale

Il nucleo safety-related deve essere deterministico, versionato e testabile:

```text
snapshot validato + profilo aeromobile + fase di volo + procedure approvate
    -> classificazione conservativa
    -> azioni prioritarie
    -> opzioni di navigazione con margini
    -> messaggi visuali/vocali
    -> evento Blackbox riproducibile
```

Un modello AI generativo non decide velocità, assetto, modalità autopilota o
comandi. In futuro potrà soltanto riformulare spiegazioni già approvate, fuori
dal percorso critico e con contenuto di fallback identico offline.

## 6. Input e gate di validità

Input minimi: attitude/heading, IAS e validità air-data, quota e AGL, vertical
speed, posizione/GNSS, vento, RPM/MAP/fuel flow, stato elettrico, flap/carrello,
terrain/ostacoli, aeroporti, meteo con timestamp, stato Pixhawk e autopilota.

Regole inderogabili:

- niente glide range se posizione, quota o DEM non sono affidabili;
- usare margini conservativi configurati per aeromobile, non prestazioni
  nominali ideali;
- vento/meteo assenti o stale devono ridurre la fiducia, non essere assunti a
  zero;
- IAS non valida impedisce consigli numerici di velocità presentati come
  misura corrente;
- checklist e limiti provengono da POH/AFM/manuale approvato e hanno revisione;
- qualunque conflitto di sorgenti deve essere visibile e registrato.

## 7. Layout proposto

### Barra superiore

`EMERGENCY ASSIST` | caso selezionato | tempo dall'attivazione | qualità dati | `ESCI`

### Colonna sinistra — FLY

- istruzione corrente in caratteri grandi;
- IAS target/configurata e IAS reale con stato fonte;
- assetto, AGL, sink rate e avviso terreno;
- massimo due azioni immediate.

### Centro — NAVIGATE

- mappa decluttered con glide footprint conservativo;
- aeroporto raccomandato e 2 alternative;
- rotta, ostacoli, vento e margine stimato all'arrivo;
- chiara indicazione `NESSUN AEROPORTO ENTRO IL MARGINE` quando applicabile.

### Colonna destra — CHECKLIST / COMM

- una sola azione evidenziata alla volta;
- frequenza, testo MAYDAY precompilato e codice transponder suggerito;
- conferma manuale per ogni cambio di configurazione;
- nessun controllo remoto di radio/transponder nella V1.

### Audio

Frasi brevi, prioritarie e non sovrapposte. Gli avvisi terrain/stall mantengono
precedenza. Il pilota può silenziare i promemoria, non gli avvisi critici.

## 8. Servi rollio e beccheggio disponibili

Registro hardware consultato:

- `AP-SERVO-01`: TruTrak Digital Servo Basic-B, P/N 8100-006, candidato rollio;
- `AP-SERVO-02`: TruTrak, modello e P/N non ancora identificati, candidato
  beccheggio/rollio.

Questi componenti non sono ancora assegnabili al controllo:

- per AP-SERVO-01 mancano pinout DB15 definitivo, specifica elettrica di
  pilotaggio, relazione comando/coppia, prova al banco e dimensionamento
  meccanico sull'aereo;
- per AP-SERVO-02 mancano persino identificazione, connettore e manuale;
- la descrizione disponibile indica fasi stepper/torque control, mentre le
  uscite tipiche ArduPlane sono funzioni servo configurate: la compatibilità
  elettrica diretta non è dimostrata;
- non sono verificati frizione/slip clutch, coppia di breakout, ritorno manuale,
  fine corsa, direzione, gioco, carichi aerodinamici e comportamento a corto.

Pertanto è vietato collegarli direttamente alle uscite Pixhawk o alimentarli
per prova senza schema e banco limitato in corrente.

## 9. Architettura attuazione futura

```text
Pulsante fisico Emergency/Stabilize
        -> ingresso dedicato Pixhawk/RC
        -> logica ArduPlane prevalidata
        -> interfaccia/driver servo qualificato
        -> servo rollio e servo beccheggio

Blackbox <- telemetria read-only e registrazione dello stato
PFD      <- DDP e annunci; nessuna autorità attuatori
```

Alimentazione: ramo autopilota dedicato dietro `AP MASTER`, protezione distinta
per asse rollio e beccheggio, ritorni di corrente separati dall'elettronica
Pixhawk/RPi e disconnessione meccanica/elettrica accessibile al pilota.

## 10. Piano di verifica prima di qualunque comando

1. identificazione fotografica completa di entrambi i servi;
2. acquisizione dei manuali e chiusura pinout/drive/coppia;
3. banco isolato e limitato in corrente, senza superfici di volo;
4. misura di corrente, temperatura, coppia, verso, gioco e disinnesto manuale;
5. progetto meccanico e analisi carichi per Asso V Jolly/Pioneer 300;
6. cablaggio con AP master/disconnect e protezione indipendente per asse;
7. SITL per logica e failure modes;
8. HIL con sensori e attuatori reali, inclusi fault di rete/alimentazione;
9. taxi e prove a terra con elica in sicurezza;
10. piano prove in volo progressivo con safety pilot e aree/quote definite.

## 11. Criteri di non attivazione

La stabilizzazione resta bloccata se uno solo dei seguenti elementi è aperto:

- calibrazione e pre-arm Pixhawk;
- direzione superfici e autorità meccanica;
- tuning roll/pitch/TECS richiesto dalla modalità scelta;
- IAS/GNSS/AHRS necessari non validi;
- alimentazione o stato servo non noto;
- assenza di override/disconnect dimostrato;
- HIL, fault injection, rollback parametri o flight-test plan incompleti.

## 12. Decisioni da prendere con il proprietario

1. prima emergenza da sviluppare: **perdita potenza motore** (raccomandata) o
   disorientamento/assetto anomalo;
2. funzione del pulsante: apertura sola pagina V1 oppure predisposizione futura
   a pressione prolungata per richiesta di stabilizzazione;
3. posizione fisica di `AP MASTER`, `AP DISCONNECT` e pulsante Emergency;
4. conferma che l'aereo target iniziale sia Asso V Jolly/Pioneer 300 con Rotax
   912 UL e disponibilità di POH/checklist specifici;
5. fotografie complete del secondo TruTrak, del DB15 e delle leve/frizioni di
   entrambi i servi;
6. presenza o assenza di attuatori per manetta, direzionale, flap e carrello.

## 13. Riferimenti tecnici

- ArduPilot Plane, Flight Modes: https://ardupilot.org/plane/docs/flight-modes.html
- ArduPilot Plane, FBWA: https://ardupilot.org/plane/docs/fbwa-mode.html
- ArduPilot Plane, Servo Functions: https://ardupilot.org/plane/docs/servo-functions.html
- ArduPilot Plane, Failsafe: https://ardupilot.org/plane/docs/apms-failsafe-function.html
- Garmin Smart Glide (riferimento funzionale, non equivalenza di qualifica):
  https://www.garmin.com/en-US/autonomi/smart-glide/
