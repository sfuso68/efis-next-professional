# FUSO AVIONICS NEXT — motore SIL senza grafica su serverha

2026-10-25, R1-14 / AVX-G5-02 IN_PROGRESS. DEVELOPMENT / NON-CERT.

## Decisione del proprietario e confine

La grafica da verificare è ESCLUSIVAMENTE quella delle applicazioni EFIS NEXT,
simultaneamente su PFD/MFD/EMS. Nessun investimento in un visualizzatore FS o
FlightGear come condizione per i test. MSFS/SimConnect rimane un adapter
opzionale, non blocca il motore autonomo e non è stato corretto in R1-14.

Destinazione: una sola sessione fisica headless su serverha, un solo clock e
un solo identificativo di run; normalizzazione/registrazione e distribuzione
attraverso una Blackbox di TEST, consumer reali con SIM visibile e perdita dati
esplicita. Non tre simulatori indipendenti, né una nuova UI dimostrativa.

## R1-16 — estensione meteo/radio/NAV offline (2026-10-25)

Implementati `sil_world.py`, fixture versionata
`config/simulation/treviso-belluno-world-reference.json`, suite offline e
suite fisica esplicita `tests.jsbsim_world_integration` (senza backend FALLISCE,
non salta i test). La configurazione è stretta, bounded, senza chiavi ignote,
con unità e identità SIM, epoch UTC/seed e finestre fault semiaperte.
Nessun file nav/prodotto operativo viene scritto o sostituito.

`jsbsim_session --world FILE` applica soltanto proprietà atmosferiche presenti
nel catalogo FDM: pressione SL in psf, delta-T Rankine, vento NED in ft/s.
Il primo tentativo usava la proprietà documentata storicamente T-sl-dev-F,
assente nel runtime: ha fallito chiuso senza crearla. Corretto usando il
catalogo reale 1.3.3 e delta-T. Pressione/temperatura/densità effettive FDM
sono registrate e la pressione effettiva alimenta il modello altimetro.
Nessuna modifica XML aeromobile, controllo hardware o pacchetto globale.

Il nuovo gruppo `world` condivide frame/tempo/sessione del volo/EMS:

- atmosfera secca troposferica di riferimento, vento interpolato per quota,
  perturbazioni armoniche deterministiche, nubi/visibilità/pioggia di scenario;
- IAS ideale da CAS con bias installazione ESPRESSO, pressione/altitudine
  standard e indicata distinte, prua magnetica con declinazione configurata;
- VOR radiale MAG con declinazione della stazione (non quella dell'aereo),
  CDI a fondo scala 10°, TO/FROM e cono/ambiguità; DME obliquo separato;
- NDB bearing TRUE/relativo; ILS corso frontale con LOC ±2.5° fondo scala,
  GS ±0.7°, inviluppo di addestramento dichiarato, nessun back-course valido;
- GNSS con bias configurato, perdita/recovery senza spegnere VOR; route a
  cerchio massimo sferico, sequenziamento, XTK positivo a destra, DTK TRUE,
  direct-to da origine acquisita, ETA verso WP con closure attuale;
- METAR ZZZZ e TAF con RMK SIM TRAINING, nessun dewpoint inventato;
  previsione SOLO PERSISTENZA. Osservazione/validità distinte dal presente,
  fault ritardo/offline/clock-skew; niente presentazione di EXPIRED/SKEW;
- cella radar gaussiana traslante dBZ, bbox EPSG:4326 e righe nord→sud,
  PNG RGBA separato con sidecar. Immagine iniziale archiviata; l'overlay
  dinamico dei renderer NON è collegato dal presente blocco.

Queste sono approssimazioni DIDATTICHE, non modelli radio certificati o meteo
predittivo. La copertura usa raggio configurato e semplice orizzonte geometrico,
senza terreno, rifrazione, multipath, interferenze o diagrammi antenna. Gli
impianti LOC/GS sono co-localizzati nella fixture: non un aeroporto pubblicato.
Le coordinate LIPH/LIDB sono riferimenti; radiofari/frequenze sono FITTIZI.
Nearest restituisce punti rotta, NON tutti gli aeroporti; ETA non è quella di
tutto il piano e non assume vento futuro noto. Il pilota di banco NON vola
automaticamente gli intercetti NAV. Stato GNSS NON equivale a RAIM/SBAS.

Configurazione completa e SHA nel manifesto; world ricalcolato nel verificatore
replay, PNG/sidecar legati agli hash. Si verifica integrità/riproducibilità,
non firma/autenticazione né taratura reale. Replay fra piattaforme con libm
diversa non qualificato: confronto numerico attuale esatto/fail-closed.
Nessun collegamento nuovo dei gruppi world ai codec DDP/EDP: il tool R1-15
continua a verificare il suo sottoinsieme precedente, non questo nuovo NAV.

Comando serverha, con nome nuovo e directory sorgente verificata:

```sh
sh /home/sfuso/avionix-sil-r114-20261025/world-source-v6/tools/simulation/run_serverha_world_sil.sh cruise world-prova-nuova-001 60
```

Il runner è finito, senza rete/porte o demoni; non ha cambiato il runner R1-14.
Test registrato nel CTest ordinario: `efis_sil_world_tests`. Verifiche: 66 test
Windows, 53 serverha (6 di integrazione fisica), CTest mirato 4/4 PASS; stesso
runtime produce byte identici, vento/calmo cambia fisica e mutazione NAV con
frame hash aggiornato viene respinta. Non una nuova acceptance QML/ARM64.

Restano integrazione mondo→snapshot/consumer HMI e prodotti→cache reale SIM
separata, publisher/rollback isolati, TAF FM/TEMPO/BECMG, dataset reali con
licenze/cicli, nearest aeroporti/alternati e RNAV/procedure, terrain masking,
ghiaccio e meteo stocastico, missione completa, catalogo/replica NAS e test
end-to-end/endurance. Attivazioni power/golden sempre bloccate, nessun DONE.

Riferimenti primari consultati (non evidenza di conformità del modello):
[JSBSim atmosfera e binding delta-T](https://jsbsim-team.github.io/jsbsim/classJSBSim_1_1FGStandardAtmosphere.html),
[JSBSim vento NED](https://github.com/JSBSim-Team/jsbsim/discussions/1099),
[FAA AIM navigation aids](https://www.faa.gov/air_traffic/publications/atpubs/aim_html/chap1_section_1.html).

## R1-14 storico — implementato e realmente eseguito

Runtime isolato su `sfuso@serverha` / `192.168.1.6`:

```text
/home/sfuso/avionix-sil-r114-20261025/
  wheels/       JSBSim 1.3.3 + NumPy 2.3.4, CPython 3.11 x86_64, hash fissati
  runtime/      installazione locale, NON pacchetti Python di sistema
  source-v3/    codice/test R1-14, archivio sorgente con SHA-256
  runs/         quattro sessioni di riferimento, ciascuna con manifest.json e frames.jsonl
```

Server osservato: x86_64, 15 GiB RAM totali, circa 11 GiB disponibili,
111 GiB disco libero. Queste misure non sono una qualifica di endurance.
Container preesistente Python 3.11 fissato all'image ID
`sha256:db3ff2e1800a8581e2c48a27c3995339d47bdf046da21c7627accd3d51053a93`.
Ogni run: rete NONE, root filesystem e runtime/sorgenti read-only, tmpfs,
UID/GID utente, cap-drop ALL, no-new-privileges, massimo 2 CPU / 768 MiB /
64 processi. Nessun servizio persistente, porta esposta, modifica Docker Compose,
firewall, router, pacchetto globale o riavvio. Non è installato alcun renderer.

Codice: `tools/simulation/jsbsim_session.py` e runner
`tools/simulation/run_serverha_sil.sh`. Modello incluso JSBSim C172P, sempre
`REFERENCE_UNVALIDATED`, non Asso/Pioneer/Rotax. Modelli/engine/systems XML
348 file legati al digest
`5d635503a63b58f37aa466d4f5596fbb8f079ee1f431caed443c8a5ed3af0a8b`.
Nessun modello di terzi redistribuito nel Git: soltanto codice, hash e log.

Integrazione fisica 100 Hz; frame diagnostici completi a 20 Hz. Sequenza e
tempo interi derivano dallo stesso contatore; volo/motore/airframe condividono
lo stesso frame. Un pilota di banco regola throttle, elevator e aileron nel
FDM; non sovrascrive posizione, assetto o velocità dopo l'inizializzazione.
Nessun comando raggiunge Pixhawk/servi. Quattro scenari da 60 s, 1201 frame:

| Scenario | Comportamento osservato |
|---|---|
| cruise | da 5000 ft / 90 kt CAS; errore quota massimo circa 0.59 ft |
| turn | roll target 20° fra 20 e 40 s; bank a 35 s circa 18.78°, prua cambia |
| engine-out | miscela tagliata a 20 s; flusso finale 0 gph, quota finale circa 4518 ft |
| sensor-loss | CAS indisponibile fra 20 e 40 s, TAS e dinamica continuano; recovery successivo |

Le cifre sono risultati del modello di riferimento, NON prestazioni dell'aereo
reale. Il pilota di banco non è un autopilota validato e non controlla la rotta
Treviso–Belluno. Scenari iniziano già in volo, non sono missioni complete dal
parcheggio all'atterraggio. Vento variabile, navigazione/radio, DEM e collisioni,
ghiaccio, meteo e pilotaggio manuale live restano aperti.

## Campioni e limiti EMS

Schema JSONL `fuso-avionics.physics-sil.v1`, SEMPRE SIM e flight_approval=false.
Proprietà JSBSim verificate contro il catalogo prima della lettura; assente o
non finita diventa UNAVAILABLE/null, mai zero plausibile. CAS, TAS, GS, VS,
posizione e assetto derivano dallo stesso FDM. Convenzioni esplicite: heading
TRUE, quota geometrica MSL (NON indicata QNH), CAS (NON misura pitot grezza).
Conversione NED down→VS positiva in salita; Fahrenheit→Celsius conservando raw.

Motore: RPM, MAP, olio, flow e due quantità carburante dal modello. CHT/EGT
aggregate sono mantenute in gruppo separato: NON copiate nei quattro cilindri.
Gli slot 18 canali EMS esistono ma CHT1..4/EGT1..4, fuel-pressure, bus/current
non modellati sono esplicitamente UNAVAILABLE. C172P a carrello fisso non è
prova di sensori di lock del carrello: feedback lock UNAVAILABLE. Non c'è
ancora il modello impianto elettrico/sonde dei due motori richiesti.

## Sessioni, replay e comandi ripetibili

Da serverha, esempio con NOME NUOVO (il runner rifiuta gli esistenti):

```sh
sh /home/sfuso/avionix-sil-r114-20261025/source-v3/tools/simulation/run_serverha_sil.sh cruise prova-nuova-001 60
```

Gli altri scenari ammessi sono turn, engine-out e sensor-loss. Run finito,
nessun demone da arrestare. La CLI Python offre anche `--realtime`, ma il
runner di riferimento genera replay accelerati: non costituisce un servizio
streaming per i display. Il verificatore `--verify` non importa JSBSim e
controlla digest, frame count, identità/tempo, SIM, gruppi, validità e valori.
Il digest rileva modifiche accidentali: non è firma o autenticazione.
Manifest COMPLETE soltanto a fine run; fallimento produce failure.json senza
manifest completo; directory/sessioni precedenti mai sovrascritte.

`role_view(PFD/MFD/EMS)` verifica proiezioni OFFLINE della stessa sessione:
NON è il consumer delle applicazioni e NON dimostra un pannello collegato.
I file sono conservati su serverha; backup NAS e indicizzazione nel catalogo
server esistente NON implementati né dichiarati verificati.

## Verifiche e stato dei gate

- 18 test unitari nuovi offline + 19 regressioni MSFS: 37 PASS Windows.
- 18 offline + 8 integrazione con JSBSim reale: 26 PASS serverha.
- Ripetizione nominale byte-identica nello stesso ambiente/versione/config.
- Derivata quota/VS e integrale fuel/flow verificati; guasti cambiano davvero
  la traiettoria, non soltanto il numero sullo strumento.
- Replay alterato, chiavi duplicate, JSON non finito, gruppi mancanti,
  indisponibile non mascherato e file esistenti rifiutati.
- CTest locale: due gate Python registrati, 2/2 PASS; nessuna nuova build
  renderer o acceptance ARM64. Golden Linux non modificata o aggirata.
- Quattro sessioni permanenti e requirements wheel hash verificati offline.

Tentativi iniziali conservati: dipendenza NumPy mancante risolta solo nel
runtime isolato; root modello da esplicitare nell'installazione --target;
C172X/C172P senza pilota di banco non tengono il trim, quindi non accettati
come scenario nominale; helper diagnostico chiamato inspect.py interferiva
con stdlib, risolto usando source-v2/v3 separati senza cancellare la copia
precedente. Nessuna modifica ai dati XML per far passare i test.

## Prossima integrazione necessaria: display REALI

1. Adapter tipizzato FDM→core, con conversioni aria/quote e heading deliberate,
   nessuna equivalenza fra GNSS/baro o CAS/IAS inventata; definire airframe.
2. Un'unica istanza di test normalizza e registra; DDP/EDP dai codec canonici,
   identità/istanza/sequenza/età/SIM comuni. Codec, fault e consumer loopback
   verificati prima della rete. Nessun generatore locale indipendente ai display.
3. Isolamento esplicito della sorgente fisica e canale SIM opt-in. Serverha
   NON deve diventare gateway verso FLIGHT-DATA; niente NAT/bridge/firewall
   aggirati (ADR-003/009/010/011). Nessun comando agli attuatori.
4. Distribuire una candidata coerente a Blackbox di test + PFD/MFD/EMS solo
   dopo i gate release/power; aprire le pagine EFIS NEXT reali e confrontare
   valori/tempo. Timeout, restart, pause e disconnect devono essere visibili.
5. Completare profili Asso/Pioneer e due motori, impianti/sonde, scenari di
   missione, server catalog/NAS, endurance e acceptance umana. Le risorse
   sufficienti a una prova breve non provano carico worst-case del server.

Nessuno dei punti 1–5 è dichiarato chiuso da questa consegna. PRJ-004
VERIFY-HW, EMS-002 e VER-001 PARTIAL; AVX-G5-02 IN_PROGRESS.
Fonti: [JSBSim ufficiale](https://github.com/JSBSim-Team/jsbsim),
[API Python](https://jsbsim-team.github.io/jsbsim/python/FGFDMExec.html).
