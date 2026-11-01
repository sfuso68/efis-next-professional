# R1-33 — correlazione audit live e archivio completo V2

2026-11-01. DEVELOPMENT / NON-CERT. Nessun endpoint o autorizzazione di volo.
Questo blocco aggiunge verifica d'integrità e restore dati; **non** riesegue
la fisica dai comandi e non completa console, autenticazione o simulatore.

## Contratto di produzione

Prima di calcolare il config_sha256 del MANIFEST, inserire nel
RUN_CONFIGURATION la chiave `control_policy` prodotta da
`tools.simulation.control_audit.execution_policy(authority_id, lease_id,
lease_until_tick)`. La policy congela modalità LOCAL_OWNER_SYNC_V1,
authority/lease, orizzonte tick, tre proprietà FDM supportate, limite wall
massimo 5 s e nome fisso `control-audit.jsonl`. Quella policy è poi parte del
digest canonico della configurazione. Il backend e la queue restano fidati.

Abilitare l'executor con il sidecar **dentro la nuova directory della run**,
al nome fisso. Il risultato deve contenere i sei campi metadata schema/scope/
file/sha256/records/bytes; scope corrente `SYNC_SIDECAR_INTEGRITY_V1`.
La libreria executor può ancora essere usata isolatamente, ma una run senza
policy congelata non soddisfa questo contratto e non è esportabile V2.
Le run R1-32 con vecchio scope o prive di policy non vengono approvate per
retrocompatibilità e non sono migrate o riscritte automaticamente.

## Verificatore

`control_audit.verify_bytes` verifica JSONL bounded: 1 MiB, 1024 record,
64 KiB per riga, tipi/esatti campi, duplicati JSON, hash/conteggi metadata,
manifesto iniziale identico, sessione/epoch e policy congelata.
BOUND deve essere primo/unico. Ogni INTENT ha un solo APPLIED identico con
ACK corretto, proprietà e readback coerenti; ID e sequenze sono unici.
L'ordine di esecuzione per sequenza non deve essere crescente: i tick dovuti
possono ordinare diversamente le sequenze già ammesse. Per ogni step, invece,
sono obbligatorie le fasi rifiuti→intenti→esiti. Non iniziare un tick nuovo con
intenti precedenti irrisolti. I conflitti INVALID richiedono un intento sullo
stesso controllo/tick; gli UNSUPPORTED non sono spacciati per input eseguiti.
RELEASED è unico/ultimo, senza incertezze e con tutti gli intenti risolti.

Il verificatore trace correla il sidecar alla prima finestra fisica RUNNING:
i tick non superano la prima pausa/arresto dell'epoch iniziale. Dopo il cambio
epoch la lease precedente resta revocata. La configurazione e il risultato
devono entrambi dichiarare la modalità: togliere solo uno dei marcatori non
abilita un fallback legacy. I file risultato/truth/sidecar sono regolari,
non symlink e bounded a 64 KiB/64 MiB/1 MiB, con identità prima/dopo lettura.

Limitazioni esplicite: il sidecar non contiene eventi di ammissione o timestamp
wall per record. Il PASS **non prova** l'orizzonte al momento dell'ammissione,
il rispetto temporale della lease, autenticità, fedeltà aerodinamica o replay
esecutivo. È correlazione d'integrità di evidenze e proprietà, non un oracle
indipendente del FDM o una firma. Restano soglie/timing/endurance separati.

## Archivio e restore V2

L'opt-in `live=True` o CLI `--live-control` seleziona esattamente quattro
payload: truth.jsonl, result.json, probe-report.json e control-audit.jsonl.
Manifesto e completion TAR hanno schema V2; il restore ha receipt V2.
La V1 resta a tre payload e continua a rifiutare la nuova modalità.
Nessuna selezione automatica o fallback su file mancanti/versioni incoerenti.

Si riusano i gate TAR non compresso/USTAR: allowlist e ordine fissi, niente
traversal, link, device, sparse, PAX, header nascosti o payload dopo footer.
Limiti, hash, snapshot prima/dopo verifica e binding run/model/commit valgono
per tutti e quattro i file. Non includere modelli, credenziali o asset MSFS.
Destinazione solo nuova; mai overwrite o pulizia. Il restore scrive solo dopo
i controlli archivio/hash e produce receipt solo dopo verifica semantica
completa; un fallimento resta come directory incompleta, non viene eliminato.
Fsync dei file non equivale a qualifica power-loss/directory fsync/firma.

Esempio, esclusivamente su una nuova run V2 già creata secondo il contratto:

```sh
python3 -m tools.simulation.session_trace /percorso/run --live-control
python3 -m tools.simulation.session_archive pack --run /percorso/run --archive /percorso/nuovo.tar --live-control
python3 -m tools.simulation.session_archive restore --archive /percorso/nuovo.tar --directory /percorso/nuovo-restore --live-control
```

Non eseguire questi esempi contro dati operativi per provare una potatura.
Il formato verifica file di sessione, non rimpiazza backup NAS o off-site.

## Prove e prossima uscita

Fixture virtuale e test JSBSim reale coprono due comandi, un rifiuto,
pausa/epoch, correlazione, pack e restore V2. Mutazioni ricalcolano anche gli
hash per controllare la semantica, non soltanto il checksum. Il nuovo caso
nativo usa clock fixture: non è una nuova prova real-time o su pannello.

Passi ancora aperti: audit completo dell'ammissione, trasporto e lifecycle
autenticati, riarmo lease con dedup run-wide, console operativa, fanout,
replay fisico esecutivo, dataset/modelli validati, replica NAS SIL e acceptance.
Nessun rilassamento dei gate Windows/power né autorizzazione attuatori reali.
