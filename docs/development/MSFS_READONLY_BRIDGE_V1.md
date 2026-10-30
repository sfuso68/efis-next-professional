# MSFS 2024 — acquisizione locale read-only V1

Data: 2026-10-24. Candidato R1-13, AVX-G5-02 IN_PROGRESS.
ID PRJ-004 VERIFY-HW, EMS-002 e VER-001 PARTIAL: nessuna promozione a DONE.

## Confine e stato reale

PC osservato: `WIN-9MB3K78GSR2`, Ethernet di servizio `192.168.1.33`;
Steam, SDK installato dal proprietario in `D:\MSFS 2024 SDK`.
Header, DLL e import library trovati; nessun SDK/binario Microsoft copiato
nel repository. La sonda C++ preliminare locale ha aperto SimConnect e letto
12 frame del volo dichiarato Cessna 172 (posizione, quota circa 55.6 ft,
IAS 0 kt, pitch nativo -3°, bank 0°, 624 RPM). Questo NON valida il modello
del Cessna né un profilo Rotax/Pioneer/Asso.

Il successivo tool Python `tools/simulation/msfs_readonly.py` è verificato
offline con fixture e ABI mock. Il tentativo sul runtime installato, con
FS2024 non più osservato in esecuzione, restituisce `0x80004005`, exit 1,
frame FAULT con valori null e riepilogo FAIL. È prova del caso negativo,
NON prova positiva di acquisizione del nuovo tool. Rimane da eseguire con
il cockpit aperto, poi con variazioni controllate e chiusura/pausa FS.

Il tool non importa il core e non modifica servizi, rete o renderer. Usa
solo cinque API SDK: Open, Close, AddToDataDefinition,
RequestDataOnSimObject e GetNextDispatch. Open forza l'indice locale
`0xffffffff`, ignorando configurazioni SimConnect remote. Non contiene
SetData, eventi di comando, socket, publisher MAVLink/DDP/EDP o destinazioni
configurabili. Non installare alcun bridge IP/NAT e non modificare firewall.

## Avvio ripetibile

Da un checkout completo, Python x64 Windows (solo standard library):

```powershell
python tools/simulation/msfs_readonly.py `
  --dll 'D:\MSFS 2024 SDK\SimConnect SDK\lib\SimConnect.dll' `
  --duration-seconds 30 --hz 10 `
  --output output/msfs-session-001.jsonl
```

Scegliere un nuovo nome a ogni prova. Il file esistente non viene sovrascritto;
la directory deve già esistere. Senza `--output` i JSONL sono su stdout.
Non scaricare DLL da terzi. La CLI non esegue download/installazione SDK.
Durata finita 1..3600 s, output 1..20 Hz, parametri finiti; nessun auto-start
o servizio persistente. Ctrl+C chiude l'handle e registra USER_STOP/LOST/FAIL.
`PASS` indica soltanto acquisizione locale senza stale/lost osservati e almeno
tre frame; non prestazione EFIS, aerodinamica, frequenza garantita o volo.

## Contratto diagnostico

Schema `fuso-avionics.msfs-readonly.v1`, righe frame + summary. Non è un ICD
di trasporto operativo e nessun consumer DDP/EDP lo accetta implicitamente.

| Campo | SimVar | Unità richiesta |
|---|---|---|
| latitude_deg | PLANE LATITUDE | degrees |
| longitude_deg | PLANE LONGITUDE | degrees |
| altitude_msl_ft | PLANE ALTITUDE | feet |
| ias_kt | AIRSPEED INDICATED | knots |
| pitch_msfs_deg | PLANE PITCH DEGREES | degrees |
| bank_msfs_deg | PLANE BANK DEGREES | degrees |
| engine_1_rpm | GENERAL ENG RPM:1 | rpm |

Un solo blocco di sette FLOAT64, object USER=0, request/definition=1,
periodo SIM_FRAME. Parser little-endian bounded (64 KiB), dimensione/count,
identità/flags/entry verificati; valori non finiti o coordinate impossibili
sono FAULT atomico. I limiti POH non sono dedotti da questi controlli.

Ogni riga conserva SIM e `flight_approval=false`, UUID d'istanza, sequenza
di output SEPARATA dalla sequenza di blocchi ricevuti, UTC e monotono di
ricezione locale, tempo di emissione, age e unità/SimVar/raw per segnale.
UTC deriva dall'orologio host: non viene dichiarato sincronizzato/disciplinato.
Non sono timestamp della misura FS: un backlog può contenere misure più vecchie
di quanto dica l'età di ricezione. Nessun clock condiviso con i Raspberry.

Convenzioni pitch/bank sono native MSFS e volutamente non mappate al dominio
EFIS. Il campo `aircraft_model_verified=false` non deduce identità o prestazioni
dal volo scelto manualmente. Quota geometrica MSL non è automaticamente la
quota indicata corretta QNH, e non è una misura GNSS indipendente.

Prima del primo blocco WAITING; dopo 2000 ms senza alcun blocco LOST.
Dopo l'ultimo campione: SIMULATED fino a <500 ms, STALE a >=500 ms, LOST a
>=2000 ms. A STALE/LOST/FAULT `value=null`, mai riuso come numero corrente;
`raw` conserva soltanto l'ultimo valore per audit. Output ripetuto e messaggi
ignoti non aggiornano l'età né la sequenza sorgente. Recupero possibile al
ritorno di dati freschi, ma un run con perdita resta FAIL nel riepilogo.
Eccezione SDK, QUIT o pacchetto malformato sono terminali: niente reconnect
automatico o fallback simulatore. Un nuovo processo richiede una nuova sessione.
La pausa del simulatore non è ancora distinta da assenza dei campioni.

## Verifica e seguito

```text
python -B -m unittest tests.test_msfs_readonly -v
ctest --test-dir <build-con-Python> -R efis_msfs_readonly_tests --no-tests=error --output-on-failure
```

19 test offline: ABI fixed-width/mock, layout, confini freshness/recovery,
NaN/Inf/coordinate, richieste estranee/troncamenti, clock, eccezione/QUIT,
nessun dato/fallimento open, flusso normale/bloccato, JSON, limiti CLI,
preservazione file esistenti, cinque sole API e nessun import socket.
Mock ABI e fixture non sostituiscono una sessione positiva del nuovo client.

Da completare: acquisizione positiva estesa e fault reale di perdita,
identità aeromobile, tempo/pausa/sim-rate, mapping convenzioni, GS/TAS/VS/HDG,
gruppi EMS/elettrico/fuel e canali per cilindro effettivamente disponibili,
catalogo/replay server/NAS. Solo dopo progettare adapter canonico e publisher
SIL isolato verso una Blackbox di test secondo ADR-003/009/010/011; non
inserire dati direttamente nei display o mescolarli con sensori fisici.
Contenuti geografici/modelli FS non sono esportabili per il solo accesso SDK:
diritti d'uso e archiviazione rimangono una verifica separata.

Riferimenti primari:
- [SimConnect SDK](https://docs.flightsimulator.com/msfs2024/retail/programming-apis/simconnect/simconnect-sdk/)
- [Layout SIMOBJECT_DATA](https://docs.flightsimulator.com/msfs2024/retail/programming-apis/simconnect/api-reference/structures-and-enumerations/simconnect_recv_simobject_data/)
- Header locale SDK installato: SimConnect.h (enum e firme ABI, non redistribuito).
