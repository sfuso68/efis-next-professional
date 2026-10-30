# Collegamento SIL — preflight e consumer canonici R1-15

2026-10-25. Richiesta: collegare la sessione serverha ai veri EFIS NEXT.
Stato R1-15 storico: PARZIALE, nessun pannello attivato in quel blocco.

## R1-18 — temporizzazione verificata, NON ancora collegamento live

2026-10-26: --realtime usa deadline monotona prima dello step e fallisce se
il ritardo supera100ms. Manifesto con timing e verificatore fail-closed,
failure.json senza COMPLETE in caso di arresto. Non hard real time certificato.
Launcher serverha, dalla copia di sorgente approvata e isolata:

```sh
sh tools/simulation/run_serverha_world_sil.sh turn NOME_SESSIONE_NUOVO 60 --realtime
```

Non sovrascrive sessioni, nessuna rete nel container. Prova v1 60.006s,
6001step/1201frame/maxlateness3.589ms; copia finale v2 59 test nativi PASS.
Il nuovo replay sul PFD include GNSS loss20–30s: SVS invalido in quella
finestra è degradazione attesa. Non trasporta EMS/airframe/prodotti meteo.
Tocchi kernel ricevuti ma MENU non risponde secondo operatore: gate aperto.

Launcher PFD PINNED, NON generico: deploy/scripts/run_pfd_sil_bench_r118.sh,
sessione e SHA fissi, stesso binario e tool R1-17. Copia sotto
/home/efis/efis-sil-panel-r117, guardia root-owned /run/efis-sil-r118,
timer efis-sil-r118-restore930s e unità screen/feed RuntimeMax900s.
Stopfeed e servizio rollback verificati; namespace solo lo, recorder invariato.

```sh
bash /home/efis/efis-sil-panel-r117/run_pfd_sil_bench_r118.sh --preflight
bash /home/efis/efis-sil-panel-r117/run_pfd_sil_bench_r118.sh --activate
bash /home/efis/efis-sil-panel-r117/run_pfd_sil_bench_r118.sh --restore
```

Ultimo avvio UTC2026-10-26T13:35:11Z, durata15min: non stato permanente.
Nessun flusso live dal server, comando attuatori o approvazione al volo.
Evidenza: ../status/evidence/r1-18-realtime-pfd-20261026/verification.json.

## R1-17 — prova PFD .45 isolata e reversibile

Su richiesta esplicita del proprietario, verificato il collegamento parziale
su PFD .45 con alimentazione 0x0. Non utilizza MFD/Blackbox, non supera i
loro gate e non cambia la rete operativa. Nessun nuovo renderer: binario
installato 7051a47, stesso SHA e puntatore di release, nuova istanza soltanto.
Il tool diagnostico e i file replay sono sotto /home/efis/efis-sil-panel-r117.

`efis-sil-panel-replay` accetta un replay world già verificato e il suo SHA;
--check non apre socket. --loopback-900s ha destinazione/porta fisse
127.0.0.1:49152, node65000, rifiuto implicito dei consumer production47,
instance casuale, emissione reale 20 Hz, massimo 900 secondi, fail su ritardo
oltre100 ms senza burst di recupero. Replay 60 s ripetuto intenzionalmente:
non una sessione fisica live e non un modello guidabile con joystick.
Metadati SIM sempre, flight_approval=false. La verifica SHA non è firma o
autenticazione; il caricatore nativo si affida alla verifica semantica Python
originaria e al SHA atteso fissato nel launcher di banco, non è ingress remoto.

Launcher **specifico del nodo/inventario verificato**, non deploy universale:
`deploy/scripts/run_pfd_sil_bench_r117.sh`. Controlla utente/hostname, power0x0,
SHA del renderer e del tool, pointer/Wayland e preflight. Unità systemd ROOT
transitorie, User=efis, PrivateNetwork=yes e namespace condiviso con solo lo;
QSettings/cache/stato sotto work-v1 separati. Nessun IP forwarding/NAT/ACL.
La grafica può leggere i file terrain/texture già installati; HTTP esterno
non può raggiungere Internet. Non usare setup rete/attivazione profili per
questo test; limitarsi alle pagine e ai controlli di consultazione.

Timer root 930 s indipendente da credenziali sudo future, screen e feed con
RuntimeMaxSec900, stop del feed chiude la finestra e riavvia l'unità utente
originale. Recorder separato senza PartOf/BindsTo, PID1419 osservato invariato.
La copia guard.sh del timer è root:root0500 in /run/efis-sil-r117 root:root0700;
il timer non esegue in differita un sorgente rimasto scrivibile dall'utente.
Ripristino manuale e stop-feed end-to-end verificati; servizio del timer
esplicitamente invocato e risultato success. Non attesa tutta la lease per
qualificare la sua scadenza naturale. Nessun timer ricorrente o installazione
persistente: dopo la finestra si torna alla configurazione precedente.

Sul terminale PFD, utente efis:

```sh
bash /home/efis/efis-sil-panel-r117/run_pfd_sil_bench_r117.sh --activate
bash /home/efis/efis-sil-panel-r117/run_pfd_sil_bench_r117.sh --restore
```

Per ripristino locale con password sudo, se necessario:

```sh
sudo bash /home/efis/efis-sil-panel-r117/run_pfd_sil_bench_r117.sh --root-restore
```

Tool ARM64 SHA81bc503f6063e07ec57415a6fe8a4f383f5966411aaf5a994198f79a8f440d37;
renderer SHAa64edcf2d0fcf5f64f30d771926a60d4e0c2854763a2f082375c7630df828854.
Replay nominal-v1, generato su serverha da world-source-v6 togliendo soltanto
i fault e cambiando dataset_id in panel-nominal-WX-NAV-REFERENCE-v1:
frames SHA697ffb68a76d0b20c448e4d3b05194f1d3016f5615a95247f4589c900d4d8b80,
config SHA85cd161536b57be42ca19aa75ab0ce0df595bdb477cc017c277cf7766913bc7f.
Verificatore serverha PASS prima di trasferimento, hash nativo uguale.

Mapping: campioni di assetto/IAS ideale/quote/prua MAG/GNSS/OBS e CDI scelto;
normalizzazione altimetro del renderer e regolazione del pilota rimangono
quelle esistenti: non forzare il QNH del pilota fingendo un sensore. 10° VOR
e 2.5° LOC = cinque dot come convenzione di banco, non taratura HMI approvata.
VS baro/GNSS indipendenti assenti, EMS e airframe non trasportati. Metar/radar
del nuovo mondo NON collegati al consumer grafico; namespace impedisce live
meteo Internet e cache bench è separata. La grafica installata mostra ancora
label GPS1/impianti dimostrativi e valori0 sotto ENGINE FAIL: difetti legacy
OPEN, non validare quei blocchi né dichiarare acceptance completa. Profilo
C172P volutamente privo di V-speeds non validate: UI rifiuta il profilo per
limiti mancanti e mostra V-SPEEDS UNAVAILABLE anziché bande Asso/Rotax errate.

Cattura reale SIM e terreno con1326 tile/27 gap locali; avvio osserva stalli
fino a1.6 s, poi finestre a60 Hz con DDP20 Hz/reject0/gap0. Questi dati non
qualificano frametime worst-case o endurance. Touch da confermare umanamente.
Build ARM64/sei test nativi PASS; MSVC e CTest mirato5/5 PASS, CI candidata
ancora da verificare. Nessun golden modificato, main/renderer non aggiornati.
Evidenza: ../status/evidence/r1-17-pfd-sil-bench-20261025/verification.json.

## Verifica diretta dei nodi (sola lettura)

| Nodo | Runtime osservato | Alimentazione | Azioni effettuate |
|---|---|---|---|
| PFD .45 | servizio UTENTE efis-pfd, PID 1495, release pfdrec-7051a47-retention-v1 | 0x0 | inventario soltanto |
| MFD .49 | servizio efis-next-mfd-direct, PID 1328/restart 0, mfd-3432402-primary-v1 | 0x50000 | inventario/log kernel soltanto |
| EMS .36 | efis-ems, PID 948/restart 0, ems-a0be9a5-main-v1 | 0x0 | manifesto, puntatore e binario letti |
| Blackbox .47 | broker PID 1294/restart 0, blackbox-7655799-v1 | 0x50000 | inventario/log kernel soltanto |

Date SSH osservate: 2026-10-24T22:19:57/58Z (25 ottobre in Europe/Rome).
EMS era più avanti del vecchio registro: source completo
`a0be9a52494ea7ccb5a3890e94d03b671162383b`, simulation_mode=true;
manifesto SHA256 `9bd369facf5a348ec83d9dd6f9eaa388f7dc90f9b839861a62cd67528095e625`.
Eseguibile /proc/948/exe risolto nella release /opt/efis-next-ems-releases.
Questa è osservazione, NON un deployment compiuto in questo blocco.

Il primo controllo del PFD cercava una unità di sistema inesistente: NON era
una prova che il PFD fosse spento. L'unità utente è attiva e il processo è
stato riconfermato. Nessun servizio è stato fermato/riavviato o riconfigurato.

MFD: log kernel di questo boot riporta eventi recenti di sottotensione,
compresi 24 ottobre 23:06:45→47 e 25 ottobre 00:02:26→28 (orari come
stampati dal journal locale), seguiti da normalizzazione. 0x50000 non indica
da solo sottotensione ATTIVA, ma gli eventi confermano che il gate power non
è soltanto una vecchia osservazione di settembre. Blackbox mantiene il bit
storico; nessuna corrispondenza è stata restituita dalla query kernel limitata,
il che NON equivale a prova di alimentazione qualificata.

## Percorso attuale da non interrompere

Blackbox riceve ancora MAVLink del Pixhawk e CAN e pubblica DDP ai due RPi5,
EDP simulato all'EMS, node 47. PFD/MFD ascoltano 49002 e richiedono source 47;
EMS ascolta 10.77.10.12:49003 e richiede source 47. Non aggiungere un secondo
publisher alle stesse porte e NON fingere che il server sia il node 47.
Non aprire routing/NAT o aggirare firewall fra LAN servizio e FLIGHT-DATA.

## Implementato: adapter e verifica dei veri codec/consumer

`tools/simulation/jsbsim_wire_check.cpp`, target opzionale
`efis-sil-wire-check` con `EFIS_BUILD_SIL_WIRE_CHECK=ON` (Qt Core soltanto).
Legge un frames.jsonl già verificato dal verificatore Python R1-14; non è
un parser di ingresso production o una validazione crittografica del replay.
Converte un sottoinsieme semanticamente compatibile in FlightSnapshot e
EngineSnapshot e usa i codec DDP/EDP ESISTENTI, senza modifiche al wire.

Due istanze del consumer DDP canonico rappresentano il percorso PFD/MFD;
un consumer EDP canonico rappresenta EMS. Sono le classi protocollo reali,
NON processi renderer aperti sui pannelli e NON una prova di socket/LAN.
Il verificatore non contiene socket, endpoint o API di comando.

Identità di test fissa node=65000 e instance=11415, sequenza comune da 1,
clock comune dai millisecondi della sessione. È una fixture OFFLINE: un
publisher reale richiederà nuova identità d'istanza ad ogni avvio e timestamp
di emissione reale, mai il semplice timestamp di replay reinviato come fresco.
Consumer configurati come quelli production (source 47) devono rifiutare
questi messaggi: prova positiva di namespace separato, non autenticazione.

Mappati: roll/pitch, lat/lon e GS; RPM/MAP/olio/flow/quantità carburante e
slot EMS con validità esplicita. Gli aggregati CHT/EGT non sono trasmessi come
cilindri e i canali mancanti restano UNAVAILABLE/SIM. IAS e ALT indicate,
barometrica, heading e VS sensore indipendente NON inventati: CAS non è
automaticamente IAS, quota geometrica non è quota QNH, heading TRUE non è
automaticamente la convenzione del consumer. Servono adapter aria/compasso
e sensor error/fault model espliciti prima di una videata completa.
Health resta degradata/warning, non SYSTEM NORMAL con dati mancanti.

## Verificato

- GCC 14.2 + Qt Core su serverha: build/link PASS, cinque test black-box PASS.
- Quattro replay fisici R1-14: 1201 frame ciascuno passati ai tre consumer,
  sequenze coerenti, SIM conservato, cilindri assenti non resi validi.
- CRC alterato, duplicati e identità diversa rifiutati; tutti e tre i consumer
  diventano STALE/LOST secondo il budget a fine invio.
- MSVC: target opzionale reale del progetto compilato e linkato PASS.
- Windows: CTest 3/3 PASS, incluso nuovo gate black-box (5 test) e due suite
  Python preesistenti. Nessuna nuova cattura/acceptance QML o ARM64 dichiarata.
- Warning GCC preesistente in ddp_v2.cpp osservato, NON risolto/ignorato;
  warning lunghezza path MSVC su altro target registrato, build mirata PASS.

Prime configurazioni del container non disponevano di pkg-config/Make;
risolto usando CMake/Ninja/Qt Core già presenti, senza installazioni globali.
Sorgenti falliti v1/v2 e log preservati; v3 e binario legati ai checksum.

Build in un checkout completo con Qt Core e Python:

```text
cmake -S . -B build/sil-wire -DEFIS_BUILD_QT_RENDERER=OFF -DEFIS_BUILD_EMS_NODE=OFF -DEFIS_BUILD_SIL_WIRE_CHECK=ON
cmake --build build/sil-wire --target efis-sil-wire-check
ctest --test-dir build/sil-wire -R efis_jsbsim_wire_check_tests --no-tests=error --output-on-failure
```

## Per attivare davvero il collegamento

1. Completare mapping aria/heading, publisher di sessione e contratto airframe;
   testare age/replay/restart, non riusare questa fixture come demone.
2. Definire ingress SIM esplicito sulla Blackbox di test, senza MAVLink/CAN
   fisici arbitrati contemporaneamente. Conservare recorder e configurazione
   originali con rollback automatico e timeout della finestra.
3. Superare PWR-001 su MFD/Blackbox e i gate release/golden: nessuna deroga
   inferita da «colleghiamoli». Identificare PSU/cavi, alimentazione e finestra
   fisica sicura; nessun power-cycle o cambio cablaggio eseguito da questo lavoro.
4. Solo allora commutare esplicitamente i consumer a identità/porta SIM di test
   oppure distribuire dalla Blackbox di test autorizzata, mai impersonare node 47.
5. Avviare una sola sessione serverha e catturare le viste EFIS NEXT REALI
   contemporanee; perdita rete/motore/sensori e ritorno alla configurazione
   precedente con evidenze. Questi punti non sono ancora completati.
