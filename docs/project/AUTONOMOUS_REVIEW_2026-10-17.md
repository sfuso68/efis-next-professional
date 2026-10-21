# Ripresa piano A1–A7 — 2026-10-17

La matrice rimane autorevole; non è una dichiarazione di completamento EFIS.

| Blocco | Consegna verificata | Cosa resta |
|---|---|---|
| A1 | Pacchetto, installazione versionata, attivazione/rollback HTTP reali del 16 ottobre | reboot/power-loss e bootstrap chiavi fuori perimetro |
| A2 | Restore NAS e bundle pre-migrazione con refs/hash verificati | off-site e disaster recovery completo |
| A3 | Policy proposta e inventario: 9 bundle, 498.108.584 byte; nessuna cancellazione | approvare quota/retention, destinazione cifrata e budget; nuovi branch non replicati |
| A4 | 37 test contratti Python e test nativi freshness/arbitraggio/DDP/EDP/vettori PASS | ERAP/source identity, sorgenti reali e jitter/fault fisici |
| A5 | Cache edge già 12 PASS; nuova validazione PNG radar 256×256, max 4 MiB, decodifica completa; CTest 3/3 PASS | payload provider/georef reali, coverage Nord Italia e ARM64, cache timestamp prodotto |
| A6 | 42 test product/review PASS; motore/runtime/persistenza/profili ricompilati e testati | manuali applicabili e seriali UL/ULS: UNRESOLVED, nessun limite operativo attivato |
| A7 | 37 test HMI/fixture/golden tools PASS; decoding PNG None accelerato con test pixel | catture fresche rendering e acceptance fisica touch/leggibilità/FPS su ogni display |

## Evidenze e ripetibilità

`docs/status/evidence/autonomous-20261017/verification.json` conserva il
primo lotto: 136 test Python PASS e 9/10 nativi Windows PASS, un eseguibile
non avviato per policy applicazioni. NON rimuovere quel fallimento dal report.
`consolidated.json` collega il successivo gruppo HMI 37 PASS (un test aggiunto),
sei test runner/storage PASS e la verifica indipendente del profilo su Linux.

La policy Windows non è stata disattivata. Il test PFD profili, stesso hash
sorgente, è stato compilato in container su serverha con GCC 14.2/Qt 6.8.2
ed eseguito senza rete: PASS. Immagine e archivio privati restano sul server,
non sono stati pubblicati; nessun servizio produttivo sostituito. Non è ARM64.
Il Dockerfile ripete l'harness sul source.tar fissato; apt non è uno snapshot
immutabile, quindi per ripetizione identica usare l'image ID registrato.

Comando aggregato dal root del repository (Qt runtime nel PATH):

```sh
python -m tools.project_status.run_autonomous_checks --output output/checks.json --ctest /percorso/ctest
```

Senza CTest il gate nativo è NOT_RUN, non PASS. Ogni gruppo ha timeout e log;
nessun comando legge bus o comanda hardware. Una policy OS può rendere il
report INCOMPLETE: usare una piattaforma controllata diversa, non aggirarla.

## Difetto radar corretto, non distribuito

Prima una firma PNG più lunghezza >512 byte bastava a pubblicare CURRENT,
e la cache era caricata senza decodifica. Ora PNG troncati/falsi, dimensioni
non 256×256 e cache invalida sono rifiutati; download buffer limitato a
4 MiB+1 e abort oltre soglia. Fixture HTTP usano PNG reali, non firma finta.
Restano invariati isolamento prodotto/tile e rifiuto della risposta superata.
Nessuna accettazione di dati provider implicita. Il fix non è stato attivato
sui PFD/MFD e richiede il normale processo release/rollback/acceptance.

## Lavori non chiudibili automaticamente

- Power, cablaggi, retro/DIP delle quattro Sequent, channel map e calibrazione.
- Fonti controllate specifiche aeromobile/912 UL/ULS, approvazione limiti.
- Qualifica switch, bus, jitter e fault con sorgenti reali; HIL e EMI/vibrazioni.
- Meteo provider/licenze/TLS/georef, coverage terreno, FPV reale e audio cockpit.
- Approvazione off-site/quota e di qualsiasi potatura; nessun delete dedotto.
- Merge/release e accettazioni operatore: nessuna promozione implicita a DONE.

Non inventare altri blocchi numerati per far apparire chiuso il progetto:
le prossime modifiche devono partire da uno di questi gap e da un criterio
misurabile, non dal numero di passi eseguiti.

## Aggiornamento A5 del 2026-10-19: cache METAR/TAF

Completato il blocco software di integrità/persistenza cache: rifiuto payload
malformati e limiti JSON, stazione coerente, conservazione ultima generazione
su fault rete/fsync/rename, riapertura in processo separato, freshness soltanto
di fetch esplicita. 28 test PASS Linux serverha e 28 ARM64, non deployment o
acceptance hardware. Evidenza `weather-cache-20261019`, runbook
`docs/deployment/WEATHER_CACHE_INTEGRITY.md`. WX-001 PARTIAL invariato.
Tutte le attivazioni restano bloccate su richiesta del proprietario fino alla
verifica alimentatori. Prossimo gap A5: payload/TLS e tempi effettivi provider,
licenze/georeferenziazione e cache radar persistente, senza attribuire al fetch
l'autorità temporale di un'osservazione meteorologica.

Aggiornamento 2026-10-21: writer per stazione serializzati con flock locale,
timeout e recovery dopo errore/morte del processo; 35 test PASS Linux serverha
e 35 ARM64. Chiusa questa sola lacuna software fra writer cooperanti; non è
power-loss o qualifica provider. Evidenza `weather-cache-lock-20261021/`.
Le attivazioni restano bloccate e nessun servizio produttivo è stato aggiornato.

## Modalità sviluppo prima della campagna integrata — 2026-10-21

Richiesta del proprietario: avanzare sulle parti sviluppabili e concentrare
successivamente i collaudi quando l'integrazione sarà più completa. Si rinviano
prove hardware, campagne estese e acceptance; restano controlli minimi di
sintassi/build pertinenti e piccoli controlli mirati. Non si disabilita la CI
e non si trasformano sviluppi non collaudati in DONE.

| Sequenza di sviluppo | Lavoro software autonomo | Dipendenze da non aggirare |
|---|---|---|
| WX-001/002 | Resolver per connessione e refresh multi-stazione implementati; poi adapter provider e cache radar persistente | Payload/licenze, tempi osservativi e georeferenziazione reali non approvati |
| NAV-001/002/003 | Consolidare modello piano volo, source identity e annunci invalidi | Ricevitori e database correnti da qualificare; nearest non significa raggiungibile |
| EMS-002/004, CFG-001 | Completare workflow setup, serializzazione mapping e calibrazioni, errori canale visibili | Nessun mapping, coefficiente o limite reale inventato; schede/sonde non approvate |
| SV-001/002/003 | Manifest copertura e gap, preparazione pacchetti area/rotta, failure delle tile | Dataset/proiezione/ostacoli e soglie TAWS non qualificati; performance da misurare |
| FPV-001/002/003 | Contratto viewer/health, simulazione stream e fallback prima del decoder reale | RPi4 VIDEO distinto da EMS, rete VIDEO separata, hardware/capacità da identificare |
| VOICE/PAN-001 | Asset voce versionati, interfacce consultive, stato ack distinto da verifica | PTT, niente comandi radio/servi; procedure specifiche richiedono fonti approvate |
| BBX/CORE/NET | Persistenza, gestione fault e diagnostica integrate con i moduli sviluppati | No nuove scritture Pixhawk, bus o bridging senza gate separato |

Questa è una coda, non un'affermazione di implementazione di tutte le aree.
Mantenere un blocco software attivo alla volta, pacchetti piccoli e review
separate; dopo integrazione software fissare campagne core/replay, HMI,
rete/storage e infine banco fisico su PSU qualificate. Attivazioni ancora
bloccate: spostare i test non concede autorità hardware o approvazione al volo.
