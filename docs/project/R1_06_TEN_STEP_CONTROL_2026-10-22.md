# R1-06 — Dieci passi di controllo e confini di manutenzione

Data: 2026-10-22. Candidato, NON distribuito. DEVELOPMENT / NON-CERT.
ID: PRJ-001, PIX-002, PRJ-004, VER-001. Nessuna promozione di stato.
Branch: `work/project-control-audit-r106-v1`, base PR #57
`71732b5a8232cc7e5e3cab2757f079beb785426c`. Non include gli stack EMS #53/#56,
la presentazione radar #54 o la quarantena writer #52. Nessun merge implicito.

| Passo | Risultato | Verifica e limite |
|---|---|---|
| 1 | Ricontrollata CI #57 | Core 39672841039 e Project Control 39672840988 SUCCESS sullo SHA completo; non merge/deploy |
| 2 | Acquisito e ispezionato artifact EMS #56 | SHA ZIP verificato; STALE 800/1280, LOST 800, FAIL 1024 confrontati/ispezionati; revisione NON completa delle 18 viste, baseline non approvata |
| 3 | Inventariati i confini MAVLink dei tool hardware | Tre audit contenevano richieste esplicite; backup e writer non sono osservatori passivi |
| 4 | Audit Pixhawk passivo | Tolto COMMAND_LONG, guardia write, versione non osservata esplicita, chiusura connessione; fake MAVLink, nessun dispositivo |
| 5 | Audit GNSS e airspeed passivi | Tolte richieste stream, guardia write, dati spontanei conservati, chiusura connessione; nessuna garanzia sul rate ricevuto |
| 6 | Backup parametri riclassificato e protetto | CLI rifiuta prima della connessione senza --allow-parameter-request; nessun COMMAND_LONG; manifesto ACTIVE_MAINTENANCE_PARAMETER_REQUEST |
| 7 | Gate Project Control senza auto-esenzione | tools/project_status e workflow sorvegliati; percorsi relativi normalizzati; soli output documentali non ricorsivi |
| 8 | Test comportamentali ExecCondition | Esecuzione CLI reale dei cinque gate con fixture temporanee, 30 casi ruolo×scenario; nessun ExecStart/systemd eseguito |
| 9 | Regressioni ripetibili | 53 test Linux PASS; Windows 52 PASS/1 SKIP symlink; nuovi test in CI e runner autonomo |
| 10 | Stato, matrice, planning ed evidenza | Aggiornati nello stesso branch, cruscotto rigenerato; commit/PR candidati non equivalgono a integrazione |

## N11 — significato di passivo

`audit_pixhawk.py`, `audit_gnss.py`, `audit_airspeed.py` ascoltano soltanto i
messaggi già trasmessi. La guardia `connection.write` rifiuta una trasmissione
MAVLink inattesa sul collegamento pymavlink già creato. Non è una prova del
driver seriale, di tutte le versioni pymavlink o di assenza di handshake TCP:
la suite usa un fake e non apre socket/device. Una versione o una misura
assente rimane assente; non si aumenta autonomamente il rate degli stream.

Il vecchio fixture UDP `tools/hardware/test_parameter_tools.py` è adattato al
flag esplicito del backup ma NON rieseguito: richiede pymavlink e include il
writer legacy separato. Non è incluso nei 53 test e non si confonde con la
nuova suite fake né con una qualifica end-to-end della libreria.

`backup_parameters.py` **non è RX-only**: richiede PARAM_REQUEST_LIST solo
dopo consenso CLI esplicito, anche all'API collect. Non modifica parametri,
ma invia traffico; non è ammesso sul percorso observer ADR-015. Il nuovo flag
non autorizza un intervento hardware e non abilita un canale bidirezionale.
Firmware assente è NOT_OBSERVED, non un valore inventato.

`set_mavlink_parameter.py` resta un writer distinto: la quarantena è nella
PR #52, non in questo stack. Non viene eseguito né dichiarato sicuro per il
solo fatto che i tre audit siano corretti. Non sono stati testati Pixhawk,
M10, DLVR, interlock o calibrazioni reali.

## N12 — controllo uniforme

L'esenzione per tutta `tools/project_status/` è rimossa. Cambi a dashboard,
backup, packaging, attivazione, runner o al checker richiedono la matrice
nello stesso insieme di file cambiati. Anche `.github/workflows/` è sorvegliato.
Output generati sotto docs/status da soli non sono codice e non creano una
dipendenza ricorsiva. Il gate prova la presenza della matrice nel diff, NON
la qualità della revisione o la correttezza delle dichiarazioni inserite.

## N13 — prova più forte, non chiusura totale

La nuova suite legge le righe ExecCondition effettivamente versionate,
rimappa solo interprete/script/manifest/root in un ambiente temporaneo e
mantiene i flag di ruolo/piattaforma/file. Prova accettazione integra e
rifiuto per byte alterati, artefatto mancante, ruolo errato, piattaforma
errata e flight_approval=true. I file fixture non vengono eseguiti.
Non dimostra supervisione systemd, assenza di TOCTOU fra condizione e avvio,
proprietà root della release, architettura del binario o acceptance ARM64.

Prima esecuzione Linux: cinque casi integri rifiutati perché /tmp del container
era noexec; guardia corretta, non difetto del validatore. Ripetizione con
fixture in tmpfs /build:exec, sorgente read-only e rete disabilitata: PASS.
Nessun servizio host/avionico o permesso di produzione è stato modificato.

Golden: l'artifact 12380062640 del run 39671281524 ha SHA-256
`aa26d31779358a33b3a944efe9f710fab055e35fcbfae7719b82c0046bf3bd0d`.
La rimozione dei valori/lancette inutilizzabili spiega il drift osservato
STALE/LOST/FAIL. A 800×480 la sovrapposizione nel logo è già nella baseline.
Questo NON approva l'estetica né tutte le immagini: #56 resta bloccata dal
golden, senza modificare PNG, manifesti o tolleranze. Il vecchio riferimento
HMI Windows non diventa prova Linux/ARM64; N13 rimane parzialmente aperto.

## Prossima sequenza, senza interventi sui nodi

1. CI finale di questo candidato e revisione di ciascuno stack; nessun merge
   cumulativo senza coerenza delle dipendenze #55→#57→R1-06 e #53→#56.
2. Correggere il logo responsive e completare revisione delle 18 golden #56;
   solo poi proporre baseline aggiornata, con render verificabile e review.
3. R1-05b: radar DPC prodotto+timestamp+immagine qualificati. Il legacy senza
   observation time resta UNKNOWN TIME, non si riabilitano pixel con mtime.
4. R1-07: verticale EDP reale con validità per canale e simulazione distinta;
   hardware, fonti motore e calibrazioni restano gate separati.
5. Attivazioni ancora bloccate dalla qualifica alimentazione e acceptance;
   nessun reboot per cancellare lo storico undervoltage.

Evidenza: `docs/status/evidence/r1-06-controls-20261022/verification.json`.
