# FUSO AVIONICS NEXT — Runbook HMI e Synthetic Vision per nodo

Stato: **PREPARED-SW / NOT-RUN**  
Ambito: `I-04`, `I-05`, `AVX-G3-01`, `AVX-G3-03`, `PRJ-004`, `SV-001`  
Prodotto: DEVELOPMENT / NON-CERT / `flight_approval=false`

## 1. Scopo e confine di sicurezza

Questo runbook unifica, senza sostituirli, i contratti C8/A20/P20 per
l'acceptance HMI e C9/C10/C11/C12 per Synthetic Vision. Stabilisce l'ordine
operativo sui tre pannelli e impedisce che deployment, fault injection o firma
dell'operatore vengano confusi con una semplice verifica software.

Il documento non autorizza:

- installazione o attivazione di release;
- modifica di servizi, rete, cache, tile o configurazioni runtime;
- distacco Ethernet, reboot, fault injection o prove elettriche;
- compilazione dei risultati come `PASS` senza osservazione reale;
- uso del terreno per navigazione o separazione ostacoli.

Ogni azione precedente richiede una finestra hardware esplicitamente
autorizzata. In caso di dubbio si interrompe la prova lasciando il bundle in
stato `NOT_RUN` o incompleto. Non si riducono le soglie per ottenere un PASS.

## 2. Nodi e responsabilità

| Ruolo | Nodo di banco noto | Piattaforma | Sessioni | Trasporto |
|---|---|---|---|---|
| PFD | `efis-pfd-01` / `192.168.1.45` | RPi5 AArch64 | HMI + SV | DDP/UDP 49002 |
| MFD | `pi5` / `192.168.1.49` | RPi5 AArch64 | HMI + SV | DDP/UDP 49002 |
| EMS | `efis` / `192.168.1.36` | RPi4 AArch64 | HMI soltanto | EDP/UDP 49003 |

Gli indirizzi sono riferimenti di banco, non identità autorevoli. Prima della
prova l'operatore deve registrare hostname, indirizzi osservati, architettura,
boot ID, release attiva e commit completo. Synthetic Vision non viene eseguita
sull'EMS. EMS e VIDEO-CORE restano nodi RPi4 fisicamente distinti secondo
ADR-013.

## 3. Gate prima della sessione

La sessione non parte se manca una sola delle seguenti condizioni:

1. branch e working tree della postazione di raccolta sono identificati;
2. il commit candidato è un SHA Git completo di 40 caratteri;
3. PFD e MFD dichiarano lo stesso commit candidato C12 e dispongono di rollback;
4. la release EMS è identificata e coerente con il commit sorgente dichiarato;
5. i manifest e i relativi artefatti superano il validatore di release;
6. lo spazio destinato alle evidenze è sufficiente e le directory non esistono;
7. le tile `N45E012` e `N46E012` e i relativi manifest sono disponibili;
8. data/ora UTC sono corrette sui nodi e sulla postazione di raccolta;
9. l'operatore ha approvato separatamente deployment, reboot e fault previsti;
10. `flight_approval=false` è conservato in ogni contratto e manifesto.

Il commit sorgente della sessione deve essere letto dalla release realmente
installata, non dalla working copy o dall'ultimo commit visibile su GitHub.

## 4. Preparazione fail-closed sulla postazione di raccolta

Usare una working copy pulita dello stesso commit candidato e creare due radici
nuove. Non riutilizzare o cancellare directory di sessioni precedenti.

```sh
cd /percorso/efis-next-professional
SOURCE_COMMIT=<sha-completo-realmente-installato>
SESSION_UTC=$(date -u +%Y%m%dT%H%M%SZ)

python3 tools/ui/manage_hmi_hardware_session.py init \
  --source-commit "$SOURCE_COMMIT" \
  --output "output/hmi-acceptance/$SESSION_UTC"

python3 tools/terrain/manage_synthetic_vision_session.py init \
  --source-commit "$SOURCE_COMMIT" \
  --output "output/sv-acceptance/$SESSION_UTC"
```

Esito atteso: i bundle HMI e SV nascono `NOT_RUN`, non sovrascrivibili e con
`flight_approval=false`. La sola inizializzazione non prova alcun nodo.

## 5. Preflight read-only per ciascun nodo

Prima di modificare lo stato del banco, acquisire in un file per ruolo:

```sh
date -u
hostname
hostname -I
uname -m
cat /proc/sys/kernel/random/boot_id
systemctl --no-pager --full status <servizio-display>
systemctl show <servizio-display> -p ActiveState -p SubState -p NRestarts
readlink -f <link-release-attiva>
vcgencmd get_throttled
vcgencmd measure_temp
df -hT
```

Verificare inoltre il manifesto della release con il validatore canonico. Non
dedurre il commit dal nome di una directory e non proseguire se il link attivo,
il manifesto o gli hash divergono.

## 6. Sequenza I-04 — Acceptance HMI

Eseguire un ruolo alla volta, mantenendo tutti gli altri in stato osservabile.
Per PFD e MFD usare DDP; per EMS usare EDP. Ogni ruolo deve coprire almeno 60
minuti e produrre gli otto artefatti richiesti:

1. `SCREENSHOT`: DAY/NIGHT e stati `LIVE`, `STALE`, `DEGRADED`, `LOST`,
   `FAIL`, `SIM` sul pannello reale;
2. `FRAME_TIMING`: FPS minimo almeno 40 e p95 frame-time non oltre 25 ms;
3. `THERMAL`: serie termica e `get_throttled=0x0` per l'intera prova;
4. `TOUCH_LOG`: target reali di almeno 44 px e risultato operatore;
5. `BOOT_LOG`: cold boot, reboot, servizio, release e restart;
6. `TRANSPORT_REPLAY`: replay DDP o EDP coerente con il ruolo;
7. `FAULT_RECOVERY`: perdita Ethernet autorizzata, fail-closed e recovery;
8. `OPERATOR_SIGNOFF`: leggibilità DAY/NIGHT e touch firmati.

Ogni `<ruolo>/run.json` mantiene esattamente `role`, `node`, `release`,
`measurements` e `artifacts`. I percorsi degli artefatti sono relativi alla
radice della sessione; file vuoti, symlink, percorsi assoluti o traversal sono
vietati.

Ordine consigliato: EMS, MFD, PFD. Il PFD viene provato per ultimo per evitare
che attività di raccolta premature interrompano il display primario del banco.

## 7. Sequenza I-05 — Synthetic Vision PFD/MFD

Questa sequenza riguarda soltanto i due RPi5. Per ogni ruolo, con cache fredda
e poi calda, percorrere i checkpoint congelati della rotta Treviso–Belluno:

1. `LIPH_DEPARTURE`;
2. `VENETO_PLAIN`;
3. `PREALPS_ENTRY`;
4. `LIDB_APPROACH`;
5. `LIDB_FINAL`.

Ogni checkpoint viene osservato in DAY e NIGHT. La prova deve attraversare il
confine `N45E012` → `N46E012`, dimostrare il nuovo LOD basato sull'impronta,
assenza di seam/popping e continuità dell'orizzonte. Il fault missing-tile deve
essere autorizzato, etichettato e seguito dal recovery; una tile mancante non
può produrre indicazioni `terrain safe`.

Per ciascun ruolo depositare nella sua directory `artifacts/`:

- `frame-timing.csv`;
- `thermal.csv`;
- `tile-transition.json`;
- `missing-tile-fault.json`;
- `checkpoint-screenshots.json` e dieci immagini reali;
- `operator-signoff.json`.

Assemblare soltanto dopo la raccolta reale:

```sh
python3 tools/terrain/assemble_synthetic_vision_run.py \
  --bundle "output/sv-acceptance/$SESSION_UTC" \
  --role PFD \
  --release-id "$SOURCE_COMMIT" \
  --operator "<nome operatore>" \
  --confirmation I_CONFIRM_REAL_SYNTHETIC_VISION_OBSERVATION

python3 tools/terrain/assemble_synthetic_vision_run.py \
  --bundle "output/sv-acceptance/$SESSION_UTC" \
  --role MFD \
  --release-id "$SOURCE_COMMIT" \
  --operator "<nome operatore>" \
  --confirmation I_CONFIRM_REAL_SYNTHETIC_VISION_OBSERVATION
```

Il comando deve fallire se la serie non copre 3600 secondi, se FPS/p95/frame
massimo non rispettano 40/25/100 ms, se il bordo supera 1 m, se esistono
campioni mancanti, fallback non etichettati, throttling o digest incoerenti.

## 8. Finalizzazione e verifica indipendente

La firma viene inserita solo dall'operatore che ha osservato la prova. Prima si
finalizza HMI, poi Synthetic Vision:

```sh
python3 tools/ui/manage_hmi_hardware_session.py finalize \
  --bundle "output/hmi-acceptance/$SESSION_UTC" \
  --operator "<nome operatore>" \
  --confirmation I_CONFIRM_REAL_HARDWARE_OBSERVATION

python3 tools/terrain/manage_synthetic_vision_session.py finalize \
  --bundle "output/sv-acceptance/$SESSION_UTC" \
  --operator "<nome operatore>" \
  --confirmation I_CONFIRM_REAL_SYNTHETIC_VISION_OBSERVATION

python3 tools/terrain/manage_synthetic_vision_session.py verify \
  --bundle "output/sv-acceptance/$SESSION_UTC"
```

Conservare exit code, output integrale, commit, release, boot ID, timestamp,
hostname, SHA-256 dei manifesti e digest dei bundle. Copiare le evidenze su
storage controllato senza alterare gli originali.

## 9. Stop condition e rollback

Interrompere senza finalizzare se si verifica uno dei seguenti eventi:

- identità o commit di un nodo non coincide con il piano;
- servizio, manifest o hash non validi;
- `get_throttled` diverso da `0x0`, reset inatteso o temperatura anomala;
- frame-rate o frame-time fuori soglia;
- freeze, schermata nera, sovrapposizione critica o touch non affidabile;
- dato non valido rappresentato come plausibile;
- terreno mancante senza indicazione esplicita;
- fault/recovery non osservabile o traffico inatteso verso il flight controller.

Il rollback usa esclusivamente la release immutabile già validata per il nodo e
richiede autorizzazione operativa separata. Dopo il rollback acquisire manifest,
stato servizio, boot ID, hash, `NRestarts`, termica e trasporto. Un rollback
riuscito non converte la sessione fallita in PASS.

## 10. Matrice di completamento

| Blocco | Software | Hardware richiesto | Stato iniziale |
|---|---|---|---|
| C8 session manager HMI | verificato | PFD + MFD + EMS | `PREPARED-SW / NOT-RUN` |
| A20/P20 HMI evidence | verificato | otto artefatti per tre ruoli | `VERIFY-HW` |
| C9 benchmark SV | verificato | PFD + MFD, DAY/NIGHT | `PREPARED-SW / NOT-RUN` |
| C10/C11 sessione/assembler | verificato | due run completi e firma | `VERIFY-HW` |
| C12 LOD/proiezione | verificato | deployment gemello e benchmark | `VERIFY-HW` |

Un esito PASS chiude soltanto la sessione documentata. Non costituisce
certificazione, autorizzazione al volo o approvazione di TAWS/obstacle
clearance.
