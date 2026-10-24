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

## Implementato e realmente eseguito

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
