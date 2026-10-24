# Collegamento SIL — preflight e consumer canonici R1-15

2026-10-25. Richiesta: collegare la sessione serverha ai veri EFIS NEXT.
Stato: PARZIALE, collegamento dei pannelli NON attivato.

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
