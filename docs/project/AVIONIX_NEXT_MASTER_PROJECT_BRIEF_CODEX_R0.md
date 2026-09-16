# AVIONIX NEXT — MASTER PROJECT BRIEF PER CODEX

**Documento principale di pianificazione hardware e software**  
**Revisione:** R0  
**Data:** 16 settembre 2026  
**Stato:** baseline di pianificazione da verificare contro repository e hardware reale  
**Progetto:** EFIS Next / EFIS Next Plus / ASSO 5 EMS  
**Repository noto:** `sfuso68/efis-evolved`

---

## 0. Istruzione iniziale per Codex

Codex deve leggere integralmente questo documento prima di proporre o applicare modifiche.

Questo file non autorizza a modificare simultaneamente tutto il progetto. Serve a:

1. ricostruire lo stato reale del software e dell'hardware;
2. confrontarlo con l'architettura approvata;
3. identificare lacune, conflitti e dipendenze;
4. costruire un backlog ordinato e verificabile;
5. sviluppare per fasi con criteri di uscita espliciti;
6. mantenere aggiornato un unico punto di riferimento.

Prima di implementare, Codex deve:

- trovare e leggere `AGENTS.md`, `CLAUDE.md`, README, documenti architetturali e istruzioni locali;
- identificare la working copy effettivamente utilizzata;
- eseguire una ricognizione Git non distruttiva;
- inventariare componenti, servizi, test e documentazione già esistenti;
- confrontare la realtà del repository con questo documento;
- segnalare le differenze senza cancellare o riscrivere il lavoro precedente;
- produrre un piano differenziale, non una riscrittura generale.

Operazioni vietate senza autorizzazione esplicita:

- `git reset --hard`;
- force push o `--force-with-lease`;
- rebase di branch condivisi;
- merge automatici tra `main`, `sv3d-fondamenta` o altri branch di lavoro;
- cancellazione di file non tracciati;
- sostituzione massiva dell'architettura esistente;
- aggiornamenti di distribuzione o firmware non reversibili;
- collegamento di segnali dell'aeromobile basato soltanto sul nome di un morsetto;
- attivazione di comandi verso motore, flap, trim, pompe, accensioni o paracadute durante le fasi iniziali.

---

## 1. Scopo del progetto

AVIONIX NEXT è una piattaforma avionica modulare destinata inizialmente a LSA, VDS, velivoli sperimentali, dimostratori e piccole serie.

Il sistema deve integrare progressivamente:

- EFIS primario su display da 7 pollici;
- visualizzazione PFD e strumenti di volo;
- synthetic vision;
- navigazione e cartografia;
- acquisizione di sensori aria e assetto;
- EMS per Rotax 912 ULS;
- integrazione MGL EMS-2;
- acquisizione distribuita ASSO 5;
- registratore/black box;
- diagnostica e manutenzione;
- integrazione Pixhawk/autopilota;
- gestione rete Ethernet;
- replay e simulazione;
- predisposizione alla futura industrializzazione.

Il progetto deve essere completato prima su Raspberry Pi, mantenendo però interfacce e contratti che permettano la migrazione verso Compute Module e sistemi industriali o avionici.

---

## 2. Famiglia di prodotto approvata

### 2.1 EFIS Next

Linea basata sull'ecosistema Raspberry Pi.

Varianti previste:

| Variante | Piattaforma | Obiettivo |
|---|---|---|
| EFIS Next RPi5 | Raspberry Pi 5 | sviluppo, prototipo e prima configurazione funzionante |
| EFIS Next CM | Raspberry Pi Compute Module | prodotto integrato con carrier dedicata, eMMC e connettori bloccabili |
| EFIS Next Dual | RPi5/CM + RPi4 gateway | separazione fra display, acquisizione e black box |

### 2.2 EFIS Next Plus

Linea futura basata su SBC/SOM industriali, rugged o hardware avionico.

Varianti previste:

| Variante | Piattaforma | Obiettivo |
|---|---|---|
| Next Plus Industrial | SBC/SOM industriale | temperatura estesa, storage robusto, watchdog e ciclo di vita |
| Next Plus Rugged | computer embedded e carrier protetta | installazioni gravose e connettori bloccabili |
| Next Plus Avio | hardware avionico o qualificabile | percorso di conformità e verifica più strutturato |

Candidate tecnologiche già considerate, ma non ancora selezionate:

- AAEON UP Squared Pro 7000;
- Advantech RSB-3720;
- Toradex Verdin iMX8M Plus;
- CompuLab CL-SOM-iMX8Plus;
- computer avionici dedicati da valutare dopo il benchmark reale.

La selezione Next Plus deve avvenire soltanto dopo avere misurato sulla linea Raspberry Pi:

- CPU e GPU;
- RAM;
- tempi di avvio;
- temperature;
- assorbimento;
- latenza;
- carico rete;
- velocità storage;
- frequenza e dimensione delle registrazioni.

---

## 3. Principi architetturali obbligatori

1. **Separazione hardware/software.** La GUI non accede direttamente a GPIO, I2C, SPI o periferiche Sequent.
2. **Acquisizione indipendente dal display.** Un blocco o riavvio del RPi5 non deve fermare la lettura locale e la registrazione RPi4.
3. **Nessun punto unico di guasto per le funzioni essenziali.** Motore, MGL, autopilota e comandi cablati devono continuare a funzionare anche senza Ethernet o senza EFIS.
4. **Dati sempre qualificati.** Ogni valore include origine, unità, timestamp, sequenza e qualità.
5. **Nessun valore congelato presentato come corrente.** Dopo timeout il dato passa a `stale` o `invalid`.
6. **Configurazione esterna e versionata.** Sensori, curve, calibrazioni, soglie e assegnazioni non devono essere dispersi nel codice.
7. **Replay obbligatorio.** Ogni funzione deve poter essere provata con registrazioni riproducibili.
8. **Portabilità.** Compilazione e test su ARM64 e x86-64 quando le dipendenze lo consentono.
9. **Fail-safe dei comandi.** La prima versione acquisisce e visualizza; non controlla direttamente funzioni vitali.
10. **Tracciabilità.** Requisiti, implementazioni, test e anomalie devono essere collegabili.
11. **Aggiornamenti controllati.** Nessun aggiornamento automatico durante l'impiego; ogni release ha versione e rollback.
12. **Sicurezza informatica proporzionata.** Servizi minimi, credenziali non incluse nel codice, rete di manutenzione controllata e log degli accessi.

---

## 4. Stato hardware consolidato

Le colonne indicano ciò che è confermato e ciò che richiede verifica fisica.

| Sottosistema | Componente | Stato | Azione richiesta |
|---|---|---|---|
| EFIS grafico | Raspberry Pi 5 | prototipo esistente | inventariare modello, RAM, OS, storage, cooling e periferiche |
| Nodo sensori | Raspberry Pi 4 | dedicato e avviato | verificare OS, hostname, storage, alimentazione e accesso SSH |
| Display | Newhaven 7.0 pollici HDMI HR RSXP CTU | unità già posseduta/usata | registrare codice completo, risoluzione, touch e cablaggio |
| Sensori prototipo | ESP32-S3 | già utilizzato | inventariare firmware e protocollo corrente |
| IMU prototipo | modulo Adafruit | già utilizzato | identificare modello e limiti prestazionali |
| Air data | sensore DLV per anemometro | già utilizzato | identificare codice, range e calibrazione |
| EMS indipendente | MGL EMS-2 | già disponibile | fotografare etichetta, connettore, firmware e sensori installati |
| Engine analog | Sequent SM-I-018 | **1 disponibile** | fotografare fronte/retro, SKU, `VER`, DIP e morsetti |
| Stati digitali | Sequent SM-I-011 | **2 disponibili** | identificare entrambe e verificare ingressi LV/HV |
| Termocoppie | Sequent SM-I-019 | **1 disponibile** | verificare revisione e avvertenza canali 5–8 |
| Fuel/airframe analog | seconda SM-I-018 | **non disponibile** | funzione rinviata; mantenere ID e software predisposti |
| Trim | Ray Allen T2-10A | acquistato | registrare etichetta, colori fili, corsa e feedback |
| Autopilota | Pixhawk 6X nella baseline | modello/possesso da confermare | identificare baseboard, firmware e cavo Ethernet corretto |
| Rete | Waveshare Gigabit Switch 5P | già disponibile | confermare SKU/revisione, alimentazione e caratteristiche reali |
| Black box | hardware/software esistente da auditare | stato non consolidato | trovare implementazione reale nel repository e sull'hardware |
| Server/NAS | Synology e repository bare noto | esistente | verificare percorso, remoti, branch e backup |
| Portatile manutenzione | sistema esistente da auditare | stato non consolidato | inventariare software, repository e strumenti installati |

### 4.1 Correzione rispetto alla P4 storica

La revisione P4 precedente riportava due SM-I-018 e una SM-I-011. L'inventario reale più recente comunicato dall'utente è:

- `1 × SM-I-018`;
- `2 × SM-I-011`;
- `1 × SM-I-019`.

Questo documento prevale sulla P4 per la quantità effettivamente disponibile. La seconda SM-I-018 FUEL/AIRFRAME è rinviata.

---

## 5. Architettura fisica di riferimento

```text
                    +---------------------------+
                    | Raspberry Pi 5            |
                    | EFIS / UI / SV / mappe    |
                    +-------------+-------------+
                                  |
                           Ethernet / API
                                  |
              +-------------------+-------------------+
              | Waveshare Gigabit Switch 5P          |
              +--------+-----------+----------+-------+
                       |           |          |
                       |           |          +--> Black box / manutenzione
                       |           +-------------> Pixhawk 6X / MAVLink
                       +-------------------------> Raspberry Pi 4 ASSO5
                                                       |
                                                       +--> Sequent via I2C P0
                                                       +--> RS-485 Modbus futuro
                                                       +--> MGL RS-232 isolata futuro

MGL EMS-2 ------------------------------------------------> indicazione indipendente
Comandi motore/flap/trim ---------------------------------> circuiti cablati indipendenti
```

Il Waveshare 5P è un concentratore del banco e un candidato al prototipo, non un elemento essenziale per mantenere motore, MGL e autopilota operativi.

---

## 6. Nodo sensori RPi4 — stato e configurazione

### 6.1 Stato rete

- indirizzo rilevato: `192.168.1.89`;
- considerarlo DHCP e provvisorio;
- non hardcodarlo nel codice;
- hostname desiderato: `asso5-sensor`;
- utente suggerito in precedenza: `avionix`, da verificare;
- non registrare password o chiavi private nei documenti o nel repository.

### 6.2 Sistema operativo

Sistema previsto:

- Raspberry Pi OS Lite Legacy 64 bit;
- Debian 12 Bookworm;
- funzionamento headless;
- nessun desktop;
- servizi gestiti da `systemd`.

Il sistema è stato installato, ma versione, kernel e architettura devono essere confermati mediante:

```bash
hostnamectl
cat /etc/os-release
uname -a
dpkg --print-architecture
ip -brief address
timedatectl
```

Non eseguire un upgrade di distribuzione durante la prima fase.

### 6.3 Ordine di impilamento

Dal basso verso l'alto:

| Livello | Scheda | Ruolo | Stack ID |
|---:|---|---|---:|
| 0 | Raspberry Pi 4 | ASSO5-SENSOR-NODE | — |
| 1 | SM-I-011 n. 1 | ENGINE/POWER STATES | 2 |
| 2 | SM-I-011 n. 2 | AIRFRAME/AUX STATES | 4 |
| 3 | SM-I-018 | ENGINE analog/universal | 0 |
| 4 | SM-I-019 | TC8 EGT/CHT | 3 |

L'ID `1` resta riservato alla futura SM-I-018 FUEL/AIRFRAME.

Impostazione teorica dei bit, da verificare sulla serigrafia reale:

| Scheda | ID2 | ID1 | ID0 |
|---|---|---|---|
| SM-I-018 ENGINE, ID 0 | OFF | OFF | OFF |
| SM-I-011 ENGINE/POWER, ID 2 | OFF | ON | OFF |
| SM-I-019 TC8, ID 3 | OFF | ON | ON |
| SM-I-011 AIRFRAME/AUX, ID 4 | ON | OFF | OFF |

Durante P0 I2C:

- `TX`, `RX` e `TERM` RS-485 disattivati;
- eventuale `RS485EN` rimosso quando previsto dalla revisione;
- ingressi e termocoppie scollegati;
- un solo punto di alimentazione;
- nessun watchdog attivato prima del test specifico;
- SM-I-019 in cima per ridurre l'influenza termica della RPi4.

---

## 7. Mappa preliminare degli ingressi

Queste mappe sono funzionali, non autorizzano ancora il cablaggio sul velivolo.

### 7.1 SM-I-018 ENGINE

| Ingresso | Segnale proposto | Interfaccia/condizione |
|---:|---|---|
| 1 | pressione olio ridondante | sensore indipendente; uscita da verificare |
| 2 | pressione carburante ridondante | sensore indipendente; uscita da verificare |
| 3 | MAP | sensore ratiometrico protetto e filtrato |
| 4 | tensione bus | partitore e protezioni; mai 14 V diretto |
| 5 | corrente batteria | trasduttore Hall |
| 6 | corrente alternatore | trasduttore Hall |
| 7 | temperatura olio ridondante | sonda indipendente e curva verificata |
| 8 | temperatura testa 1 | sonda indipendente |
| 9 | temperatura testa 2 | sonda indipendente |
| 10 | aria carburatore/aspirazione | sensore da scegliere |
| 11–16 | riserve | disabilitate finché non assegnate |

La modalità esatta disponibile per ogni canale dipende dalla revisione SM-I-018. Non progettare la carrier prima di avere fotografato SKU e `VER` e confrontato il manuale corretto.

### 7.2 SM-I-011 n. 1 — ENGINE/POWER STATES

| DI | Stato proposto | Regola |
|---:|---|---|
| 1 | accensione A selezionata | soltanto contatto ausiliario; mai P-lead diretto |
| 2 | accensione B selezionata | soltanto contatto ausiliario; mai P-lead diretto |
| 3 | pompa 1 alimentata | feedback, non comando |
| 4 | pompa 2 alimentata | feedback, non comando |
| 5 | alternatore/regolatore | uscita stato documentata; mai B+ diretto |
| 6 | starter comandato | segnale bobina relè protetto, non linea di potenza |
| 7 | RPM digitale | solo uscita di un condizionatore dedicato |
| 8 | allarme o riserva | segnale documentato |

### 7.3 SM-I-011 n. 2 — AIRFRAME/AUX STATES

Assegnazioni candidate:

- flap UP/DOWN o stati/finecorsa;
- trim/finecorsa esterni;
- canopy o porta;
- paracadute armato tramite contatto isolato e sicuro;
- luci;
- stato autopilota esterno;
- ingresso manutenzione;
- riserve.

Per segnali aeromobile a 12/14 V utilizzare soltanto i morsetti **LV 3–32 V**. I morsetti HV non sono utilizzati.

### 7.4 SM-I-019 TC8

| TC | Assegnazione candidata | Vincolo |
|---:|---|---|
| 1–4 | EGT cilindri 1–4 | sonde K dedicate; non in parallelo con MGL |
| 5–6 | CHT/temperature testa | verificare errata se PCB `VER2.0` |
| 7 | riduttore o riserva | sonda K dedicata |
| 8 | vano motore o riserva | sonda K dedicata |

Usare cavo di estensione e connettori compensati dello stesso tipo della termocoppia. Verificare polarità, giunto freddo, sonda aperta e temperatura campione.

### 7.5 Fuel/Airframe analogico

Rinviato per assenza della seconda SM-I-018.

Il software deve comunque prevedere un modulo opzionale con stato `not_installed` e ID riservato, senza generare allarmi operativi continui.

---

## 8. MGL EMS-2 e Rotax 912 ULS

L'MGL EMS-2 resta:

- indicatore motore indipendente;
- riferimento di confronto durante calibrazione e sviluppo;
- sorgente dati verso ASSO5 tramite RS-232 isolata;
- backup utilizzabile quando RPi4, RPi5 o Ethernet non sono disponibili.

Interfacce previste:

| Interfaccia | Uso |
|---|---|
| RS-232 OUT | acquisizione operativa iniziale dei dati MGL |
| CAN MGL | segmento dedicato a 250 kbit/s, separato da DroneCAN |
| CAN passivo | possibile ascolto di laboratorio quando è presente un RDAC |
| RDAC virtuale | ricerca futura, solo banco, non baseline iniziale |

Regole:

- non collegare la stessa sonda resistiva o termocoppia contemporaneamente a MGL e Sequent senza circuito attivo progettato e verificato;
- CAN MGL, DroneCAN e CANopen non sono lo stesso bus;
- un solo host attivo sul segmento MGL;
- la prima fase usa RS-232 isolata e mantiene il CAN MGL separato;
- sensori, curve e cablaggio MGL reali devono essere inventariati prima della progettazione filo-per-filo.

---

## 9. Pixhawk e autopilota

La baseline documentale usa Pixhawk 6X, ma Codex deve verificare il modello realmente posseduto, la baseboard e il firmware scelto.

Responsabilità del Pixhawk:

- sensori di volo/autopilota;
- stabilizzazione e navigazione quando autorizzate;
- fornitura dati al RPi5 tramite MAVLink;
- funzionamento autonomo rispetto all'EFIS grafico.

Collegamenti previsti:

- Ethernet/MAVLink come canale principale del banco;
- TELEM2 UART diretto come possibile canale di riserva;
- CAN/DroneCAN per dispositivi compatibili;
- nessuna unione con CAN MGL.

La porta Ethernet della baseboard deve essere collegata solo con cavo/adattatore corretto. Non realizzare una RJ45 improvvisata.

L'autopilota completo, inclusi servo, attuatori, override manuale, envelope protection e prove in volo, è un workstream separato e non deve essere attivato durante P0 del nodo sensori.

---

## 10. Alimentazione, protezioni e cablaggio

### 10.1 Principio

Nessun computer o modulo a 5 V viene alimentato direttamente dal bus 12/14 V dell'aeromobile.

Architettura preliminare:

```text
Bus avionica 14 V
  -> fusibile/CB dedicato
  -> protezione polarità e load dump/transitori
  -> filtro EMI
  -> DC/DC dedicato
  -> ramo 5 V protetto
  -> carico
```

### 10.2 Rami separati

| Ramo | Carico | Vincolo |
|---|---|---|
| EFIS | RPi5, display, storage | alimentazione dedicata e shutdown controllato |
| Sensor node | RPi4 e stack Sequent | un solo punto 5 V; verificare corrente e brownout |
| MGL | EMS-2 | ramo indipendente |
| Pixhawk | flight controller e sensori | power module dedicato |
| NET-SW | Waveshare 5P | fusibile, TVS, filtro e test di riavvio |
| Attuatori | flap/trim | circuiti, CB e cut-off indipendenti |

### 10.3 Vincoli

- non collegare fra loro le uscite di due convertitori 5 V;
- non alimentare contemporaneamente RPi via USB-C e HAT senza schema verificato;
- misurare assorbimento normale e di spunto;
- provare brownout durante avviamento motore;
- dimensionare AWG/mm² dopo lunghezze e correnti reali;
- prevedere serracavo, protezione da sfregamento e connettori con ritenzione;
- separare fasci accensioni, alternatore, termocoppie, analogici e dati;
- definire schema delle masse e schermature prima del cablaggio definitivo;
- comandi flap e trim restano cablati e indipendenti dal software nella prima versione.

---

## 11. Rete Ethernet

Il Waveshare Gigabit Switch 5P è la dorsale del banco/prototipo.

Assegnazione proposta:

| Porta | Apparato | Funzione |
|---:|---|---|
| P1 | Pixhawk 6X | MAVLink/UDP |
| P2 | RPi5 | EFIS, synthetic vision, configurazione |
| P3 | RPi4 ASSO5 | dati EMS, diagnostica, log |
| P4 | black box/recorder | registrazione e scarico dati |
| P5 | display backup o PC manutenzione | uso secondario o a terra |

Piano indirizzi di prodotto, da applicare soltanto dopo il banco LAN domestico:

| Apparato | IP proposto |
|---|---|
| Pixhawk | `10.41.10.2` |
| ASSO5/RPi4 | `10.41.10.10` |
| RPi5 | `10.41.10.20` |
| Recorder | `10.41.10.30` |
| Display backup | `10.41.10.40` |
| PC manutenzione | `10.41.10.100` |

La rete di volo non deve dipendere da un server DHCP. La configurazione definitiva richiede indirizzi statici o fallback controllato.

Ethernet può trasportare:

- dati EFIS ed EMS;
- MAVLink;
- diagnostica;
- configurazione;
- replay e download log;
- mappe e aggiornamenti a terra.

Ethernet non deve essere l'unico percorso per:

- accensioni;
- pompe carburante;
- flap e trim;
- paracadute;
- allarmi motore essenziali;
- controllo primario dell'autopilota.

---

## 12. Architettura software target

### 12.1 Livelli

```text
Hardware fisico
  -> driver vendor / device adapter
  -> Hardware Abstraction Layer
  -> acquisizione grezza
  -> normalizzazione e calibrazione
  -> health e allarmi
  -> data bus interno
  -> recorder / rete / replay
  -> applicazioni EFIS e manutenzione
```

### 12.2 Servizi/nodi logici

I nomi seguenti sono funzionali. Codex deve riutilizzare i moduli esistenti se equivalenti.

| Servizio | Responsabilità |
|---|---|
| `asso5-board-scan` | inventario Sequent, ID, revisioni, firmware e configurazione |
| `asso5-sequent-reader` | polling I2C/Modbus, timestamp, timeout e qualità link |
| `asso5-mgl-reader` | acquisizione RS-232 MGL e validazione pacchetti |
| `asso5-signal-converter` | curve sensori, unità, filtri e calibrazioni versionate |
| `asso5-health` | range, stale, open/short, rate-of-change e divergenze |
| `asso5-publisher` | pubblicazione dati normalizzati e diagnostica |
| `asso5-recorder` | log grezzo e normalizzato indipendente dal display |
| `efis-ingest` | ricezione RPi5 da ASSO5, Pixhawk e altre sorgenti |
| `efis-core` | stato avionico coerente e arbitraggio delle sorgenti |
| `efis-ui` | PFD, EMS, MFD, allarmi e interazione pilota |
| `sv3d-engine` | terreno, assetto, ostacoli e synthetic vision |
| `map-nav` | cartografia, aeroporti, spazi aerei e piano di volo |
| `blackbox` | registrazione di sistema e replay |
| `maintenance-api` | diagnostica, configurazione, export e aggiornamenti a terra |
| `simulator-replay` | dati sintetici, fault injection e riproduzione log |

### 12.3 Contratto dati minimo

Ogni campione deve includere almeno:

```text
schema_version
node_id
boot_id
source_id
channel_id stabile
sequence
timestamp_utc
timestamp_monotonic
value
unit
quality
age_ms
calibration_id/version
hardware_revision
```

Qualità minima:

- `valid`;
- `stale`;
- `invalid`;
- `not_installed`;
- `open_circuit`;
- `short_circuit`;
- `out_of_range`;
- `degraded`;
- `test`.

Un dato simulato non può mai essere pubblicato come `valid` reale.

### 12.4 Configurazione

Configurazioni richieste:

- nodi e interfacce;
- mappa segnali;
- tipo sensore;
- numero di parte e seriale;
- curva e calibrazione;
- unità;
- soglie e isteresi;
- timeout e frequenza di polling;
- routing rete;
- logging;
- profilo velivolo;
- feature flag per hardware non installato.

Usare YAML, TOML o altro formato già presente e validabile. Evitare percorsi assoluti e nomi di device hardcoded.

### 12.5 Registrazione

Il sistema deve registrare:

- dati grezzi;
- dati normalizzati;
- qualità e allarmi;
- configurazione attiva;
- versione software e commit Git;
- versione hardware;
- boot ID;
- eventi di rete;
- riavvii e watchdog;
- temperatura e carico di sistema;
- errori storage.

Requisiti:

- rotazione;
- limite spazio;
- checksum o controllo integrità;
- recupero dopo spegnimento inatteso;
- export per intervallo temporale;
- replay deterministico;
- sincronizzazione temporale documentata.

### 12.6 Frequenze iniziali

| Dato | Periodo iniziale |
|---|---:|
| stati digitali | 250 ms |
| analogici motore | 500 ms |
| EGT/CHT | 1 s |
| MGL RS-232 | secondo protocollo, atteso circa 1 Hz |
| health nodo | 1 s |
| telemetria di sistema | 1–5 s |

Le frequenze devono essere configurabili e misurate; non generare traffico inutile.

---

## 13. Software esistente da auditare

Lo sviluppo precedente include lavoro su:

- Raspberry Pi 4;
- Raspberry Pi 5;
- synthetic vision;
- black box;
- server/NAS;
- portatile di sviluppo/manutenzione;
- strumenti EFIS;
- branch `sv3d-fondamenta`;
- branch di lavoro indicato storicamente come `work/claude-strumenti-p1`.

Questi elementi non vanno ricreati prima dell'audit.

Codex deve produrre una tabella reale:

| Componente | Percorso | Linguaggio/framework | Branch | Stato build | Test | Ultima modifica | Debito/blocco |
|---|---|---|---|---|---|---|---|

Devono essere cercati almeno:

- eseguibili e servizi;
- file di configurazione;
- script deploy;
- unità `systemd`;
- pipeline CI;
- test;
- simulatori;
- driver sensori;
- protocollo dati;
- UI e strumenti;
- synthetic vision e dataset;
- cartografia;
- logging/black box;
- server e API;
- installer e aggiornamenti;
- documentazione generata;
- dipendenze non versionate;
- credenziali o segreti da rimuovere.

---

## 14. Governance Git e baseline

Repository GitHub noto:

```text
git@github.com:sfuso68/efis-evolved.git
```

Repository bare NAS noto:

```text
~/git-repos/efis.git
```

Branch storici:

- `main`;
- `sv3d-fondamenta`;
- possibili branch `work/*` da enumerare.

La divergenza storica tra `main` e `sv3d-fondamenta` non deve essere considerata attuale senza nuova verifica.

Prima attività Git:

```bash
git status --short --branch
git remote -v
git branch --show-current
git branch -a
git tag --list
git log --oneline --decorate --all -30
git fetch --all --prune
```

Confronti obbligatori:

```bash
git rev-list --left-right --count main...sv3d-fondamenta
git merge-base main sv3d-fondamenta
```

Codex non deve fare merge soltanto per ridurre una divergenza. Prima deve identificare:

- lavoro esclusivo synthetic vision;
- cambiamenti più recenti di `main`;
- conflitti architetturali;
- test disponibili;
- possibile strategia di integrazione;
- checkpoint o tag di sicurezza.

---

## 15. Workstream di sviluppo

### WS0 — Baseline, governance e documentazione

**Obiettivo:** stabilire la verità su repository, hardware, versioni e documenti.

Deliverable:

- report Git/NAS/GitHub;
- inventario repository;
- inventario hardware;
- indice documentale;
- registro decisioni ADR;
- backlog unico con dipendenze;
- matrice requisiti-test.

Criterio di uscita:

- nessun lavoro non localizzato;
- branch e working copy identificati;
- inventario approvato;
- documenti in conflitto segnalati.

### WS1 — Contratto dati e architettura condivisa

**Obiettivo:** congelare il confine fra hardware, servizi e UI.

Deliverable:

- ICD Avionix Next;
- schema dati versionato;
- regole qualità/stale;
- naming canali;
- policy tempo e sequenze;
- profilo configurazione velivolo;
- compatibilità ARM64/x86-64.

Criterio di uscita:

- replay dello stesso dataset su RPi4/RPi5 e host di sviluppo;
- test di compatibilità schema;
- nessun accesso hardware dalla UI.

### WS2 — ASSO5-SENSOR-NODE RPi4

**Obiettivo:** acquisizione Sequent affidabile e indipendente.

Fasi:

1. identificazione OS e I2C;
2. inventario delle quattro schede;
3. installazione CLI vendor;
4. lettura ripetuta a ingressi scollegati;
5. adapter hardware;
6. modalità simulata;
7. health e timeout;
8. recorder locale;
9. publisher rete;
10. servizio `systemd`;
11. soak test;
12. migrazione I2C → RS-485 Modbus quando utile.

Criterio di uscita P0:

- tutte le schede identificate;
- seconda SM-I-018 correttamente `not_installed`;
- servizio automatico e recuperabile;
- un'ora senza errori I2C non gestiti;
- perdita rete senza perdita dell'acquisizione locale.

### WS3 — EFIS RPi5 e interfaccia grafica

**Obiettivo:** integrare dati di volo, motore e sistema in un'interfaccia leggibile e deterministica.

Attività:

- audit strumenti esistenti;
- PFD;
- EMS;
- MFD;
- annunci e priorità allarmi;
- modalità giorno/notte;
- gestione perdita sorgenti;
- profili schermo 7 pollici;
- startup e shutdown;
- benchmark rendering;
- test screenshot/replay;
- accessibilità manutenzione.

Criterio di uscita:

- nessuna misura scaduta mostrata come corrente;
- frame rate e latenza definiti e misurati;
- allarmi prioritizzati e riproducibili;
- test con perdita RPi4, Pixhawk e rete.

### WS4 — Synthetic vision, mappe e navigazione

**Obiettivo:** consolidare il lavoro del branch synthetic vision senza perdere le modifiche nuove di `main`.

Attività:

- audit `sv3d-fondamenta`;
- inventario engine grafico, shader, terrain e dataset;
- benchmark RPi4/RPi5;
- caching e gestione storage;
- geodesia e riferimenti;
- fonti terreno, aeroporti, spazi aerei e ostacoli;
- licenze e redistribuzione dati;
- modalità offline;
- aggiornamento dataset a terra;
- avvisi di copertura o dati scaduti;
- replay voli registrati.

Criterio di uscita:

- merge/integrazione pianificata e testata su branch dedicato;
- prestazioni misurate;
- dataset e licenze documentati;
- fallback senza synthetic vision.

### WS5 — Air data, AHRS e sensori di volo

**Obiettivo:** trasformare il prototipo ESP32-S3/Adafruit/DLV in una catena misure tracciabile.

Attività:

- identificare modelli esatti;
- acquisire dati grezzi;
- definire frequenze e timestamp;
- calibrazione pressione, temperatura e zero;
- IAS/TAS/altitudine e compensazioni;
- calibrazione IMU e magnetometro;
- allineamento assi;
- fusione sensori;
- confronto con Pixhawk;
- rilevamento divergenza;
- scelta futura di IMU professionale;
- gestione guasti e modalità degradata.

Criterio di uscita:

- procedure di calibrazione ripetibili;
- log grezzi disponibili;
- error budget documentato;
- confronto statico e dinamico con riferimento.

### WS6 — EMS Rotax 912 ULS e MGL

**Obiettivo:** integrare il motore mantenendo MGL indipendente.

Attività:

- inventario MGL e sonde;
- RS-232 isolata;
- parser e checksum;
- mappa segnali Rotax;
- sensori indipendenti per Sequent;
- MAP e correnti;
- RPM con condizionatore dedicato;
- EGT/CHT senza paralleli impropri;
- calibrazione e confronto MGL/ASSO5;
- fuel flow solo dopo lo schema carburante reale;
- CAN MGL passivo soltanto in laboratorio;
- RDAC virtuale come ricerca futura.

Criterio di uscita:

- MGL resta operativo con ASSO5 spento;
- dati RS-232 validati;
- errori e timeout corretti;
- sensori e curve documentati;
- nessun circuito vitale dipende da Linux.

### WS7 — Pixhawk/autopilota

**Obiettivo:** integrare telemetria e successivamente stabilizzazione, mantenendo confini di sicurezza.

Attività:

- confermare hardware e firmware;
- Ethernet e UART backup;
- MAVLink routing;
- sincronizzazione tempo;
- definire sorgenti autorevoli;
- gestione doppio link e deduplicazione;
- servo e alimentazioni separate;
- modalità e override manuale;
- bench/HIL;
- flight test incrementali;
- auto-trim futuro con limiti e timeout.

Criterio di uscita iniziale:

- sola telemetria stabile;
- nessuna autorità sui comandi prima di HIL e approvazione del piano di prova.

### WS8 — Black box, server e portatile

**Obiettivo:** consolidare registrazione, sincronizzazione, analisi e manutenzione.

Attività:

- trovare implementazioni esistenti;
- definire ownership dei log;
- sincronizzazione da RPi4/RPi5;
- export e compressione;
- replay;
- checksum;
- retention;
- backup NAS;
- strumenti analisi volo;
- dashboard manutenzione;
- pacchetto aggiornamento offline;
- recovery e rollback.

Criterio di uscita:

- un volo simulato è registrato, trasferito, verificato e riprodotto integralmente.

### WS9 — Elettrico, carrier, enclosure e cablaggi

**Obiettivo:** trasformare il banco in un prototipo installabile.

Deliverable:

- schema alimentazioni;
- schema I/O filo-per-filo;
- distinta componenti;
- connettori e pinout;
- identificatori fili;
- sezioni AWG/mm²;
- masse e schermature;
- protezioni e fusibili;
- enclosure;
- dissipazione;
- fissaggi e serracavi;
- procedura installazione e ispezione.

Criterio di uscita:

- design review elettrica e meccanica;
- nessun collegamento basato su assunzioni non verificate;
- test brownout, termico e vibrazionale del prototipo.

### WS10 — Qualità, test e safety case

**Obiettivo:** costruire evidenze verificabili, anche se il primo prodotto non è certificato.

Deliverable:

- requisiti di sistema;
- requisiti software/hardware;
- hazard log;
- FMEA/FMECA;
- fault tree per funzioni critiche;
- matrice di verifica;
- unit/integration/HIL test;
- report prove ambientali;
- release notes;
- known issues;
- manuale installazione;
- manuale manutenzione;
- SBOM;
- configuration index.

### WS11 — EFIS Next CM

**Obiettivo:** industrializzare la linea Raspberry Pi dopo la baseline RPi funzionante.

Requisiti carrier candidati:

- Compute Module;
- eMMC;
- alimentazione protetta;
- watchdog esterno;
- RTC;
- Ethernet;
- CAN e seriali isolate;
- connettori bloccabili;
- monitor tensione/temperatura;
- identificazione hardware;
- recovery boot;
- gestione termica.

### WS12 — EFIS Next Plus

**Obiettivo:** portare lo stesso software su hardware industriale/avionico.

Gate di ingresso:

- baseline Next congelata;
- benchmark reali disponibili;
- contratto dati stabile;
- test replay automatici;
- requisiti ambientali definiti.

---

## 16. Strategia di test

### 16.1 Livelli

| Livello | Contenuto |
|---|---|
| Unit | conversioni, filtri, parser, timeout, qualità, config |
| Component | adapter Sequent/MGL/Pixhawk e recorder |
| Integration | RPi4↔RPi5, rete, logging, replay |
| HIL | segnali simulati, Pixhawk, MGL e fault injection |
| Bench electrical | range, open/short, sovrarange protetto, brownout |
| Environmental | temperatura, vibrazione, EMI preliminare |
| Ground aircraft | velivolo fermo, motore spento e poi acceso |
| Taxi | disturbi dinamici, rete e registrazione |
| Flight incremental | solo dopo approvazione e checklist dedicata |

### 16.2 Fault obbligatori

- scheda assente;
- indirizzo duplicato;
- bus I2C bloccato;
- perdita RS-485;
- perdita Ethernet;
- switch spento;
- RPi4 riavviato;
- RPi5 bloccato;
- storage pieno;
- filesystem in sola lettura;
- timestamp errato;
- pacchetto MGL corrotto;
- perdita Pixhawk;
- dato congelato;
- valore fuori scala;
- sonda aperta;
- corto a massa;
- brownout;
- temperatura CPU elevata;
- avvio senza rete;
- recupero dopo spegnimento improvviso.

### 16.3 Prove ingressi

| Classe | Stimolo minimo | Fault |
|---|---|---|
| 0–3,3/10 V | 0, 25, 50, 75, 100% FS | aperto, massa, sovrarange protetto |
| 4–20 mA | 4, 8, 12, 16, 20 mA | <3,6 mA, aperto, >20 mA protetto |
| resistivo | decade, almeno 8 punti | aperto e corto |
| digitale 14 V | 0, soglia, 12, 14, 16 V | polarità errata e filo aperto |
| frequenza | minimo, nominale, massimo | assenza impulsi e duty anomalo |
| termocoppia K | ambiente, medio, massimo utile | polarità inversa e sonda aperta |

---

## 17. Sicurezza funzionale e limiti d'impiego

La piattaforma Raspberry Pi/Sequent non è automaticamente un apparato avionico certificato.

Prima di definire dichiarazioni commerciali o installazioni occorre stabilire:

- categoria e impiego previsto;
- normativa applicabile a VDS/LSA/sperimentale;
- funzioni informative rispetto a funzioni che comandano il velivolo;
- livello di integrità richiesto;
- responsabilità del pilota e degli strumenti indipendenti;
- requisiti ambientali;
- evidenze di sviluppo e verifica;
- manuali e limitazioni operative.

Gli standard aeronautici possono essere usati come riferimento metodologico, ma non dichiarati come conformità senza piano, dati e valutazione competente. Valutare con professionista qualificato:

- RTCA DO-160 per ambiente;
- RTCA DO-178C per software;
- RTCA DO-254 per hardware elettronico;
- SAE ARP4754A per sviluppo di sistema;
- SAE ARP4761A per safety assessment;
- RTCA DO-326A/ED-202A e documenti collegati per cybersecurity avionica.

---

## 18. Dati mancanti e decisioni aperte

### Bloccanti per il banco immediato

- versione OS reale del RPi4;
- hostname e utente effettivi;
- fotografie e revisioni delle quattro Sequent;
- impostazione reale dei DIP;
- tipo e potenza dell'alimentatore usato;
- modello microSD e capacità;
- risultato `i2cdetect` e CLI vendor.

### Bloccanti per il cablaggio motore

- codice di ogni sonda Rotax/VDO;
- sensori attualmente collegati all'MGL;
- cablaggio e firmware MGL EMS-2;
- scelta delle sonde indipendenti;
- strategia EGT/CHT;
- pickup RPM e condizionatore;
- schema masse;
- lunghezze dei fasci.

### Bloccanti per fuel/airframe

- seconda SM-I-018 oppure nuova architettura equivalente;
- schema carburante reale;
- ritorno, pompe e posizione fuel flow;
- tipo galleggiante/livello;
- posizione flap e finecorsa;
- feedback e installazione T2-10A.

### Bloccanti per EFIS completo

- audit software RPi4/RPi5/black box/server/portatile;
- contratto dati esistente;
- framework grafico e dipendenze;
- stato del branch synthetic vision;
- licenze cartografia e dataset;
- modello esatto IMU/DLV;
- requisiti prestazionali e display;
- hardware Pixhawk reale e firmware scelto.

---

## 19. Roadmap con gate

### Gate G0 — Verità del progetto

Uscita richiesta:

- Git e repository allineati o divergenze documentate;
- inventario hardware aggiornato;
- mappa software completa;
- documenti indicizzati;
- nessuna perdita di lavoro.

### Gate G1 — Nodo sensori P0

Uscita richiesta:

- RPi4 stabile;
- quattro Sequent identificate;
- servizio in modalità reale/simulata;
- logging e health;
- soak test di un'ora.

### Gate G2 — Contratto dati e replay

Uscita richiesta:

- schema versionato;
- RPi4 pubblica;
- RPi5 riceve;
- black box registra;
- replay riproduce.

### Gate G3 — EFIS integrato al banco

Uscita richiesta:

- PFD/EMS/MFD;
- Pixhawk telemetria;
- MGL RS-232;
- synthetic vision integrata;
- fault di rete e nodi gestiti.

### Gate G4 — Prototipo elettrico installabile

Uscita richiesta:

- schemi filo-per-filo;
- alimentazioni e protezioni;
- enclosure e connettori;
- test brownout/termici/vibrazionali preliminari;
- manuale installazione banco/velivolo fermo.

### Gate G5 — Prove dinamiche

Uscita richiesta:

- motore, rullaggio, disturbi e registrazione verificati;
- anomalie chiuse o accettate;
- piano di volo incrementale approvato.

### Gate G6 — Baseline EFIS Next

Uscita richiesta:

- HW/SW/config congelati;
- test e documenti completi;
- recovery e aggiornamento controllati;
- criteri di impiego dichiarati.

### Gate G7 — Compute Module e Next Plus

Uscita richiesta:

- benchmark;
- carrier CM;
- porting industriale;
- parità funzionale mediante replay;
- piano di qualificazione.

---

## 20. Documentazione da mantenere nel repository

Struttura suggerita, da adattare senza duplicare ciò che esiste:

```text
docs/
├── master/
│   ├── AVIONIX_NEXT_MASTER_PROJECT_BRIEF.md
│   ├── PROJECT_STATUS.md
│   ├── DECISION_LOG.md
│   └── ROADMAP.md
├── requirements/
│   ├── SYSTEM_REQUIREMENTS.md
│   ├── SOFTWARE_REQUIREMENTS.md
│   └── HARDWARE_REQUIREMENTS.md
├── architecture/
│   ├── SYSTEM_ARCHITECTURE.md
│   ├── DATA_CONTRACT.md
│   ├── NETWORK_ICD.md
│   └── SIGNAL_ICD.md
├── hardware/
│   ├── INVENTORY.md
│   ├── POWER.md
│   ├── WIRING.md
│   ├── BOM.md
│   └── ENCLOSURE.md
├── software/
│   ├── COMPONENT_MAP.md
│   ├── BUILD_AND_DEPLOY.md
│   ├── CONFIGURATION.md
│   └── UPDATE_AND_RECOVERY.md
├── verification/
│   ├── TEST_PLAN.md
│   ├── TRACEABILITY.md
│   ├── HIL_PLAN.md
│   └── REPORTS/
├── safety/
│   ├── HAZARD_LOG.md
│   ├── FMEA.md
│   └── LIMITATIONS.md
└── operations/
    ├── INSTALLATION.md
    ├── MAINTENANCE.md
    └── TROUBLESHOOTING.md
```

Ogni documento deve contenere revisione, data, stato, autore/strumento, dipendenze e changelog.

---

## 21. Protocollo di aggiornamento del Master Brief

Questo file deve diventare il punto di ingresso per Codex.

Ad ogni modifica materiale:

1. aggiornare lo stato del relativo workstream;
2. registrare la decisione in `DECISION_LOG.md` o ADR;
3. collegare issue, commit e test;
4. aggiornare hardware/BOM se cambia un componente;
5. aggiornare ICD se cambia un'interfaccia;
6. incrementare revisione del documento;
7. non cancellare decisioni storiche: marcarle come superate e indicare da cosa;
8. mantenere distinti `CONFIRMED`, `PROPOSED`, `PLANNED`, `DEFERRED` e `BLOCKED`.

Formato stato consigliato:

```text
STATUS: CONFIRMED | PROPOSED | PLANNED | IN_PROGRESS | BLOCKED | DEFERRED | VERIFIED
EVIDENCE: file / commit / foto / test report
OWNER: persona o agente
NEXT_ACTION: attività concreta
```

---

## 22. Riferimenti interni da leggere

Codex deve cercare e leggere questi file prima di pianificare modifiche correlate:

1. `AVIONIX_NEXT_MASTER_PROJECT_BRIEF_CODEX_R0.md` — questo documento.
2. `CLAUDE_CODE_ASSO5_SENSOR_NODE_HANDOFF_R0.md` — dettaglio operativo del nodo RPi4.
3. `AVIONIX_NEXT_ASSO5_EMS_Rotax912ULS_P4.docx` — architettura EMS storica, mappe I/O e collaudi; usare con la correzione inventario di questo Master Brief.
4. `AVIONIX_NEXT_Linee_Prodotto_Documento_Riferimento_R0.docx` — decisione ufficiale sulle linee Next e Next Plus.
5. `EFIS_NEXT_ALLINEAMENTO_CLAUDE.md` — procedura NAS/GitHub e protezione dei branch.
6. `LSA_Rotax912ULS_Falken_IFM_Impianto_Elettrico_P0.pdf` — riferimento elettrico precedente, da verificare rispetto alle decisioni ASSO 5.
7. README, documentazione e codice effettivamente presenti in `sfuso68/efis-evolved`.

Precedenza in caso di conflitto:

1. evidenza hardware reale più recente e dichiarazione esplicita dell'utente;
2. questo Master Brief e sue revisioni successive;
3. documento linee prodotto;
4. handoff specialistico più recente;
5. P4 e documenti storici;
6. assunzioni non verificate.

Il codice esistente non prova da solo che una decisione architetturale sia ancora approvata: Codex deve segnalare la discrepanza e proporre la riconciliazione.

---

## 23. Riferimenti tecnici esterni ufficiali

### Raspberry Pi

- Raspberry Pi OS: <https://www.raspberrypi.com/software/operating-systems/>
- Documentazione Raspberry Pi: <https://www.raspberrypi.com/documentation/>
- Hardware Raspberry Pi 4: <https://www.raspberrypi.com/products/raspberry-pi-4-model-b/>
- Compute Module: <https://www.raspberrypi.com/products/compute-module-4/>

### Sequent Microsystems

- SM-I-018 prodotto: <https://sequentmicrosystems.com/products/sixteen-analog-digital-inputs-8-layer-stackable-hat-for-raspberry-pi>
- SM-I-018 driver: <https://github.com/SequentMicrosystems/16univin-rpi>
- SM-I-011 prodotto: <https://sequentmicrosystems.com/products/eight-hv-digital-inputs-for-raspberry-pi>
- SM-I-011 driver: <https://github.com/SequentMicrosystems/8inputs-rpi>
- SM-I-019 prodotto: <https://sequentmicrosystems.com/products/eight-thermocouples-daq-8-layer-stackable-hat-for-raspberry-pi>
- SM-I-019 driver: <https://github.com/SequentMicrosystems/smtc-rpi>

Usare sempre manuale e schema corrispondenti alla revisione fotografata della singola PCB.

### MGL Avionics

- EMS-2 manuale ufficiale: <https://www.mglavionics.co.za/Infinitec/Manuals/Blaze%20EMS-2.pdf>
- MGL CAN Protocol: <https://www.mglavionics.co.za/Docs/MGL%20CAN%20Protocol.pdf>
- Pagina prodotti Blaze: <https://mglavionics.co.za/Infinitec/Blaze.html>

### Pixhawk, PX4, MAVLink e ArduPilot

- Pixhawk 6X: <https://docs.px4.io/main/en/flight_controller/pixhawk6x>
- Pixhawk 6X wiring: <https://docs.px4.io/main/en/assembly/quick_start_pixhawk6x>
- PX4 Ethernet: <https://docs.px4.io/main/en/advanced_config/ethernet_setup>
- MAVLink: <https://mavlink.io/en/>
- ArduPilot Plane: <https://ardupilot.org/plane/>
- ArduPilot QuadPlane: <https://ardupilot.org/plane/docs/quadplane-support.html>

### Rete

- Waveshare Gigabit Switch 5P: <https://www.waveshare.com/gigabit-switch-5p.htm>

### Trim

- Ray Allen Company: <https://www.rayallencompany.com/>

### Dati geografici e navigazione — candidati da verificare

- OpenStreetMap copyright/licenza: <https://www.openstreetmap.org/copyright>
- openAIP: <https://www.openaip.net/>
- Copernicus Data Space: <https://dataspace.copernicus.eu/>

La selezione dei dataset deve includere copertura, accuratezza, aggiornamento, funzionamento offline e licenza di redistribuzione.

### Normativa e standard

- ENAC: <https://www.enac.gov.it/>
- EASA: <https://www.easa.europa.eu/>
- RTCA: <https://www.rtca.org/>
- SAE standards: <https://www.sae.org/standards>

I testi completi di molti standard sono soggetti a licenza. Non copiare o redistribuire documenti protetti senza autorizzazione.

---

## 24. Primo incarico completo per Codex

Codex deve eseguire soltanto analisi e pianificazione finché l'utente non approva il piano.

### Passo 1 — Ricognizione

Produrre:

- percorso working copy;
- branch e HEAD;
- stato file modificati/non tracciati;
- remoti;
- divergenza branch;
- component map del software;
- documenti trovati;
- toolchain e dipendenze;
- servizi deployabili;
- test esistenti;
- hardware già configurato nel codice.

### Passo 2 — Gap analysis

Per ogni workstream indicare:

- cosa esiste;
- cosa funziona;
- cosa è incompleto;
- cosa è duplicato;
- cosa è obsoleto;
- cosa manca;
- dipendenze;
- rischi;
- test necessari;
- decisioni richieste all'utente.

### Passo 3 — Piano esecutivo

Creare un backlog ordinato per gate con:

- ID attività;
- descrizione;
- input;
- output;
- dipendenze;
- rischio;
- criterio di accettazione;
- file previsti;
- test;
- stima relativa `S/M/L/XL`;
- stato.

### Passo 4 — Prima proposta di implementazione

La prima modifica consigliata deve essere piccola, reversibile e utile a stabilire la baseline. In assenza di blocchi, la priorità è:

1. WS0 baseline;
2. WS2 Fase A inventario RPi4/Sequent;
3. WS1 audit del contratto dati;
4. definizione del primo verticale completo: sensore simulato → RPi4 → rete → RPi5 → black box → replay.

### Output richiesto a Codex

```text
AVIONIX NEXT — PROJECT AUDIT

REPOSITORY
- path:
- branch:
- head:
- working tree:
- remotes:
- divergence:

EXISTING SYSTEMS
- RPi4:
- RPi5:
- synthetic vision:
- black box:
- server/NAS:
- laptop/tools:
- Pixhawk/MAVLink:
- EMS/MGL/Sequent:

CONFLICTS WITH MASTER BRIEF
- ...

BLOCKERS
- ...

RECOMMENDED GATES
- G0:
- G1:
- G2:
- ...

FIRST SAFE CHANGE
- scope:
- files:
- tests:
- rollback:

QUESTIONS REQUIRING USER DECISION
- ...
```

Codex deve terminare l'audit prima di iniziare modifiche estese.

---

## 25. Registro iniziale delle decisioni

| ID | Decisione | Stato |
|---|---|---|
| D-001 | completare prima la linea Raspberry Pi | approvata |
| D-002 | mantenere due linee: EFIS Next e EFIS Next Plus | approvata |
| D-003 | separare RPi5 grafico e RPi4 acquisizione/black box | approvata |
| D-004 | mantenere MGL EMS-2 indipendente | approvata |
| D-005 | usare Waveshare 5P al banco/prototipo, non come funzione vitale unica | approvata |
| D-006 | usare Sequent per il prototipo ASSO 5 | approvata |
| D-007 | inventario Sequent reale: 1×018, 2×011, 1×019 | confermata dall'utente |
| D-008 | rinviare FUEL/AIRFRAME analogico | approvata |
| D-009 | RPi4 sensor node a `192.168.1.89` DHCP provvisorio | confermata |
| D-010 | mantenere ID 1 riservato alla futura SM-I-018 | approvata |
| D-011 | nessun accesso diretto hardware dalla GUI | approvata |
| D-012 | nessun comando vitale tramite Ethernet/Sequent nella prima fase | approvata |
| D-013 | migrazione industriale solo dopo baseline e benchmark | approvata |

---

## 26. Changelog

### R0 — 16 settembre 2026

- creato Master Project Brief per Codex;
- unificate linee prodotto, EMS, RPi4, RPi5, rete, Pixhawk, black box e percorso industriale;
- corretto inventario Sequent rispetto alla P4;
- rinviato FUEL/AIRFRAME analogico;
- inseriti workstream, gate, criteri di test e fonti ufficiali;
- definita procedura di audit prima dello sviluppo.

