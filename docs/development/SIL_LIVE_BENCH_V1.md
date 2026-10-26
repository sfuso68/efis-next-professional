# SIL live R1-19 — protocollo di banco, non rete operativa

2026-10-26. DEVELOPMENT / NON-CERT / flight_approval=false.
Implementazione: `jsbsim_live.py`, `live_bench_bridge.py`, modalità live di
`efis-sil-panel-replay`, launcher specifico `run_pfd_sil_live_r119.sh`.
Mantiene ADR-003/005/009/010/017: nessun MAVLink, CAN o comando attuatori;
nessuna modifica al wire DDP, al router operativo o alla configurazione Pixhawk.

## Percorso effettivo

serverha: JSBSim in container senza rete → stdout SSH autenticato → bridge
di banco Windows → stdin SSH autenticato dell'adapter ARM64 nel namespace
privato PFD → DDP canonico 127.0.0.1:49152/source65000 → renderer installato.

Il passaggio Windows è ancora una dipendenza del banco, NON architettura di
prodotto. Non è IP forwarding/NAT, non apre porte LAN di ascolto e non copia
chiavi private sui nodi. Tentativo read-only serverha→PFD: nessuna chiave shell
già autorizzata trovata; non sono stati modificati authorized_keys o permessi.
Il renderer non emette richieste: il challenge appartiene soltanto all'adapter
di test verso il processo fisico isolato. Nessun ritorno a Blackbox/Pixhawk.

## Contratto challenge

- HELLO vincola sessione, durata1–120s, SIM/non-flight e configurazione mondo
  canonica/SHA-256 atteso; i radiofari sono fittizi, il modello C172P reference.
- L'adapter emette una richiesta per frame con sequenza e nonce casuale128bit.
  Il server accetta soltanto schema/sequence/nonce: campi comando extra respinti.
- La richiesta viene verificata prima dello step che produce il frame. Il FDM
  mantiene integrazione100Hz e uscita20Hz; non cicla una registrazione.
- Il server lega la risposta al nonce; l'adapter esige stessa sessione, clock,
  sequenza e origine SIM. Tempo di richiesta/risposta massimo deriva da
  FreshnessBudget::network_age_margin=250ms; non confronta clock remoti.
- L'intero RTT è conservato come età dei campioni pubblicati, non azzerato al
  ricevimento; il consumer DDP conserva inoltre il proprio margine di rete.
- Accelerazione/replay a raffica e deriva temporale sono respinti. Deadline
  FDM oltre100ms causa FAILED/failure.json, non un recupero silenzioso.
- Timeout2s su richiesta source, inclusa riga parziale; native header10s,
  risposta250ms e riga65536byte. Il bridge usa code limitate a due messaggi,
  lettura strict JSON e timeout: non un buffer infinito di campioni vecchi.
- Le pipe sono fidate/autenticate da SSH; il parser Qt non è una nuova API
  production per ingress JSON non fidati. SHA e source_node non sono firme.

Il manifesto source attesta fisica/archivio, NON lo stato del display.
Il vecchio manifesto live-v1 manteneva il booleano storico
efis_nodes_connected=false: non interpretarlo come misura. Il writer finale
usa null/UNKNOWN per questo dato; occorrono bridge-report e log del renderer.

## Esecuzione ripetibile su questo banco già predisposto

Le directory/hashes sono PINNED nell'implementazione, non autoinstallazione
generica. Sorgente serverha r119-source-v3, adapter PFD r119-build-v4.
Ogni sessione richiede un nome NUOVO; directory e registrazioni mai sostituite.
Da checkout Windows, prima prova senza inviare UDP o cambiare il PFD:

```powershell
python -m tools.simulation.live_bench_bridge --ssh-key C:/Users/stefa/.ssh/codex_efis_pfd_ed25519 --directory output/NUOVA_PROVA --session NUOVA_SESSIONE --seconds 3
```

Per consultare il PFD in finestra di banco autorizzata:

```powershell
ssh -i C:/Users/stefa/.ssh/codex_efis_pfd_ed25519 efis@192.168.1.45 "bash /home/efis/efis-sil-panel-r117/run_pfd_sil_live_r119.sh --activate"
python -m tools.simulation.live_bench_bridge --ssh-key C:/Users/stefa/.ssh/codex_efis_pfd_ed25519 --directory output/NUOVA_PROVA_PFD --session NUOVA_SESSIONE_PFD --seconds 60 --loopback
```

Solo la seconda modalità invia UDP, solo a loopback nel namespace condiviso
con lo schermo. Verifica hash adapter, schermo attivo e PrivateNetwork=yes.
Il launcher controlla utente/hostname, power0x0, pointer/SHA renderer e tool,
socket Wayland, assenza di altri bench e preflight. Stesso binario7051a47,
config/settings/cache separate, simulatore locale DISABLED; recorder invariato.
Screen/feed RuntimeMax140s, timer root180s con guardia root-owned0500 sotto
directory0700. Fine feed ferma screen e riavvia l'unità utente originale.
Ripristino manuale locale:

```sh
bash /home/efis/efis-sil-panel-r117/run_pfd_sil_live_r119.sh --restore
```

## Limiti che NON si chiudono con questa prova

Il passaggio Windows e la richiesta sincrona possono causare overrun: un test
ha correttamente fallito dopo3frame. Adapter iniziale leggeva un byte per
poll/read; versione finale legge blocchi4096 con limite totale invariato e
respinge dati già accodati oltre la risposta. Nessuna soglia allargata. Una
prova riuscita non è qualifica di jitter/endurance né dimostra causalità unica
del ritardo: registrazioni iniziali fallite conservate.

DDP live include solo il mapping aria/assetto/GNSS/OBS/CDI già esplicitato in
R1-17. NON trasporta engine EDP, airframe, prodotti meteo, piano completo o
identità radio nella GUI. La fisica e il mondo contengono anche altri gruppi,
ma la loro presenza nel JSON non equivale a integrazione grafica.
Touch PFD segnalato FAIL, modelli Asso/Pioneer/Rotax non validati, startup
stall e UI legacy/golden aperti. MFD/Blackbox esclusi per power gate.

Prossimo gate software: producer continuo indipendente dai client e fanout
canonico EDP/airframe/meteo, con stesso clock/sessione e fault/endurance.
Prossimo gate di accesso: autorizzazione dedicata serverha→adapter SIL PFD
se si elimina Windows, non riuso forzato delle credenziali backup.
L'accettazione simultanea dei pannelli e il pilotaggio interattivo restano OPEN.
