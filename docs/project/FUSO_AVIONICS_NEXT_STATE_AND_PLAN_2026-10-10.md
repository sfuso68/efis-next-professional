# FUSO AVIONICS NEXT — situazione e piano operativo

Data: 2026-10-10. DEVELOPMENT / NON-CERT / FLIGHT APPROVAL: NO.

## Attendibilità del quadro

Audit documentale del checkout `be43d7f4ea105c4b00b40cf1476a9b2c31d7a482`, non audit live dei nodi. GitHub non raggiungibile dal controllo odierno: main, PR aperte, CI e sincronizzazione NAS non riconfermati. Le osservazioni hardware restano storiche, con le date nei registri. Non confondere release candidata, codice integrato e release attiva. Nessun deploy, merge, reboot o modifica hardware effettuato.

La matrice censisce 71 requisiti: DONE 4, VERIFY-HW 11, PARTIAL 38, TODO 14, BLOCKED 2, LEGACY 2. Indice ponderato 46,8%, NON prontezza al volo. I due LEGACY non sono nuove funzionalità. VERIFY-HW non significa collaudato. Anche i DONE hanno attività di mantenimento e criteri da sorvegliare.

## Situazione per sottosistema

| Parte | Base esistente / evidenza | Mancanza decisiva |
|---|---|---|
| Core | C++20 separato da Qt, snapshot/provenance, test e sanitizer registrati | Arbitraggio, freshness e recovery live multi-sorgente |
| PFD RPi5 .45 | Release retention `7051a47`, archivio 33,8 GB verificato, rollback/reboot storici | Soak 24 h, startup retry, fix stall meteo non distribuito, acceptance grafica |
| MFD RPi5 .49 | `3432402` attiva storica; touch 5 punti, NAV/setup/EMS/Canvas osservati | PFD poco leggibile; candidata `76afead` solo software; alimentazione `0x50000`, FPS sotto 40, tema/FPV aperti |
| EMS RPi4 .36 | Release HMI `62ba90b`, circolari/CHT/EGT/fuel/carrello/flap; simulazione | Misure reali, calibrazione, limiti motore approvati, allarmi e ore reali |
| Sensor RPi4 .89 | Bookworm ARM64, I2C1 abilitato, gruppo i2c, alimentazione sana il 3 ottobre | Inventario 4/4, retro/PSU/DIP, channel map, driver/servizio, riferimenti di calibrazione |
| Pixhawk / GNSS / airspeed | Acquisizione e monitor read-only, prove USB/fault pregresse | Backup parametri, GNSS/airspeed/calibrazione e validità end-to-end da riconfermare |
| Blackbox .47 | Ingest, distribuzione e recording progettati; gate fail-closed pregressi | Inventario runtime unico, power, carico/disco/power-loss e recovery reali |
| Rete | FLIGHT-DATA separata da VIDEO; monitor passivo, firewall verificati storicamente | Ethernet osservatori, jitter/fault; Waveshare 5P insufficiente; MokerLink candidato non qualificato |
| Synthetic vision | DEM e texture reali, cache 1.326 tile, pipeline GPU e C9–C12 software | 27 gap, dettaglio vicino, prestazioni >=40 FPS/p95 <=25 ms, prove target e TAWS |
| Navigazione | DB aeroporti/navaid, pagine NAV, profili e piano volo | FlightPlan canonico, validità/ciclo dati, radio reali e source failover |
| Meteo | METAR/TAF/radar, freshness e cache, test async/race software | Provider/TLS/georeferenziazione reali e prove ARM64 senza stall |
| FPV RPi4 dedicato | Architettura separata definita | Nodo e quattro camere, pipeline/latency/bandwidth, auto-revert e recording ancora da costruire/provare |
| Voce / emergenze | Assistenza consultiva e prototipi; fraseologia e simulazione | Voce naturale offline, riconoscimento affidabile, priorità audio e conferme; attuazione PAN-002/003 BLOCKED |
| Profili aereo/motore | 447 parametri/28 domini e workflow, SIM distinta | Dati specifici ASSO 5 e 912 UL/ULS separati, manuale/revisione/pagina, validazione reale; nessuna equivalenza automatica Pioneer |
| Serverha .6 / NAS .22 | Servizi/caching, replica Git e restore storico verificato | Allineamento attuale, backup fuori sito, CA, dataset licenze e restore periodico |
| Portatile .39 | QGC observer e firewall; integrità MAVLink pregressa | Burst >1 s: integrità PASS non equivale a timing PASS; migrazione Ethernet e regressione |
| Industrializzazione | Release immutabili e rollback esistenti | Alimentazione aeronautica, enclosure, cablaggi, EMI/vibrazioni, HIL, qualifiche e acceptance banco |

## Nuovo planning: chiudere gate, non accumulare blocchi numerati

| Ordine | Obiettivo / requisiti | Autonomo ora | Interazione necessaria | Uscita |
|---|---|---|---|---|
| 0 | Baseline/governance PRJ-001/004 | Cruscotto, indice, backlog ed evidence review | Accesso GitHub/NAS per riconferma | HEAD/PR/CI/repliche con digest e conflitti espliciti |
| 1 | Power + MFD PWR-001/MFD-001/PFD-002 | Revisionare procedure/fixture | PSU/cavo, boot pulito, build ARM64/rollback e osservazione pannello | `0x0`, nessun nuovo evento, acceptance IAS/ALT/HSI/NAV/touch |
| 2 | PFD recorder/meteo PFD-001/BBX-001/WX-002 | Test I/O/fault e gate release | Attivazione autorizzata e soak 24 h | >=12 GiB liberi, nessuno stall/restart, recording integro |
| 3 | Sensori reali PRJ-002/EMS-004/AIR-001/GNSS | Driver contract e test sintetici | Inventario retro/PSU/DIP, cablaggio e riferimento misura | 4/4 schede, mapping firmato, calibrazione e fault/recovery tracciati |
| 4 | Verticale G2 CORE-003/004/PRJ-003/NET | Replay/validator e fixture | Sorgenti, switch e fault fisici | Parità consumer, provenance/aging, jitter misurato e recovery |
| 5 | SV/NAV/WX | Cache/LOD/georef test e audit licenze | Benchmark reale/route e confronto visivo | Coverage senza gap richiesti, budget GPU e dati stale inequivocabili |
| 6 | FPV/voce/emergenze | Specifiche e simulazione isolated | Camere/audio e prove operatore | Latency misurata, reversione sicura, conferme senza autorità implicita |
| 7 | HIL e prodotto VER/PROD | Matrici fault e pacchetto test | Soak/full-load/power-loss/EMI/vibrazione | Acceptance di banco; nessuna approvazione al volo automatica |

Work-in-progress massimo: un gate di integrazione hardware e un blocco software indipendente. Niente date artificiali: stimare dopo inventario e baseline live. Ogni lavoro ha ID, dipendenze, responsabile, evidenza, criterio di chiusura. Non attendere hardware per preparare fixture, ma non promuovere test sintetici a verifica reale. Servi e comandi autopilota restano fuori dal percorso consultivo finché non esiste safety case e autorizzazione separata.

## Controllo interattivo e collaborazione

Aprire `docs/status/PROJECT_DASHBOARD.html`: ricerca su tutti i campi, filtri area/stato, dettaglio evidenza/rischio/prossimo passo/acceptance, handoff filtrato scaricabile. Snapshot offline e sola lettura: non è telemetria live, non verifica le macchine. Il digest della matrice rende identificabile la versione consultata.

Rigenerare dal root del repository:

```sh
python -m tools.project_status.generate_status
python -m tools.project_status.generate_dashboard
python -m tools.project_status.generate_status --check
python -m tools.project_status.generate_dashboard --check
```

Codex: repository, implementazione, test, evidenze, aggiornamento matrice nello stesso cambiamento. ChatGPT: discussione, scelte, requisiti e revisione del planning usando handoff e documenti aggiornati. Proprietario: approvazioni, misure e acceptance operatore. Non serve trasferire tutto in chat: serve una sola fonte versionata condivisa. Le conversazioni non si sincronizzano automaticamente e non provano che un commit sia distribuito.

Per ogni ripresa fornire repository, branch/HEAD, digest matrice, ID da lavorare, evidenze recenti e autorizzazioni. Formato: «Leggi stato/matrice/ADR; lavora sugli ID …; non mutare hardware; registra software vs hardware vs aperto». Decisioni della chat diventano proposte documentali prima di cambiare i gate.

Monitoraggio continuo: prima fase rigenerazione ad ogni aggiornamento e check prima delle PR; seconda fase job CI per output coerenti; terza fase dashboard servita read-only su serverha con snapshot datato e indicatore di stale; quarta fase audit ricorrente con notifiche solo per cambiamenti, fallimenti o interventi necessari. Queste ultime tre fasi sono proposte, NON installate oggi. Nessun merge/deploy automatico dal monitor. CI, health nodi e planning sono tre livelli distinti: non sommare una macchina online a un requisito DONE.

## Verifica di questo incremento

Implementati documentazione e viewer offline, non software avionico. Verifiche riproducibili registrate in EFIS_NEXT_STATE.md. Nessuna nuova evidenza hardware, nessun push/merge dichiarato. Restano aperti accesso remoto e pubblicazione del cruscotto su serverha.

### Aggiornamento operativo della stessa giornata

Accesso serverha ripristinato; viewer e timer cinque minuti installati e
verificati. Sorgente dedicata read-only GitHub main, deploy key confinata,
host key verificata, HTTP localhost:8766 accessibile via tunnel SSH.
Due refresh reali GITHUB_SYNC_OK su `1cc0a786…`; test sintetico failure
conserva ultima vista e mostra ERROR. Browser/reboot non qualificati.
Il mirror NAS resta separato e storicamente arretrato: audit replica è il
prossimo lavoro autonomo. Nessuna promozione di requisito a DONE implicita.
