# EFIS NEXT — Contratto release immutabili V1

Stato: CANDIDATE / VERIFIED-SW ONLY / NON-CERT

## Obiettivo

Ogni processo operativo deve essere riconducibile a una directory immutabile,
un commit completo, un manifesto verificato e una sola unità systemd nominativa.
Un processo vivo da `build/dev` o un binario copiato in `/opt/.../bin` senza
manifesto non è una release identificabile.

Il contratto riguarda `PROD-005` e non autorizza deployment. I due EFIS legacy
restano separati e recuperabili.

## Regole obbligatorie

1. Il puntatore `current` è un symlink atomico verso una directory di release;
   non può essere una directory mutabile.
2. La directory contiene `release-manifest.json` conforme a
   `config/deployment/efis-release-manifest-v1.schema.json`.
3. Il manifesto lega uno o più ruoli, piattaforma, commit e SHA-256 di ogni
   artefatto.
4. `flight_approval` è sempre `false`; non può essere promosso dal packaging.
5. `simulation_mode` descrive il contenuto effettivo della release.
6. L'unità esegue un artefatto soltanto attraverso `current` e verifica prima
   manifesto, ruolo, piattaforma, presenza e hash.
7. Log, registrazioni, terrain, imagery, configurazioni locali e segreti sono
   persistenti fuori dalla directory immutabile.
8. Il rollback seleziona una release già verificata; non ricompila e non copia
   un singolo binario sopra quello attivo.
9. Un solo supervisore è autorevole per processo. Autostart grafico, shell e
   unità concorrenti devono risultare assenti prima dell'abilitazione.
10. La verifica software non sostituisce reboot, fault, rollback e acceptance
    sul nodo reale.

## Profili canonici

`config/deployment/node-release-policy-v1.json` definisce i cinque processi:

| Ruolo | Nodo | Scope | Unità | Puntatore |
|---|---|---|---|---|
| PFD | `efis-pfd-01` | user | `efis-pfd.service` | `/home/efis/efis-next-current` |
| MFD | `pi5` | system | `efis-next-mfd-direct.service` | `/home/pi/efis-next-current` |
| EMS | `efis` | system | `efis-ems.service` | `/home/pi/efis-next-current` |
| Recorder | `efis-pfd-01` | user | `efis-recorder.service` | `/home/efis/efis-next-current` |
| Blackbox | `efisblackbox` | system | `efis-blackbox-broker.service` | `/opt/efis-blackbox-current` |

PFD e recorder possono condividere la directory solo se lo stesso manifesto
dichiara e verifica entrambi gli artefatti; ciascuna unità conserva ruolo e
gate propri. MFD e PFD usano lo stesso eseguibile ma profili systemd distinti.

## Validazione fail-closed

Esempio EMS, prima di qualunque switch:

```sh
python3 tools/deployment/validate_release_manifest.py \
  --manifest /home/pi/efis-next-candidate/release-manifest.json \
  --release-root /home/pi/efis-next-candidate \
  --expected-role EMS \
  --expected-platform RPI4_AARCH64 \
  --check-files
```

Ogni errore produce exit code non zero e JSON con `status=FAIL`. Il validatore
rifiuta campi ignoti, percorsi assoluti o con `..`, duplicati, hash errati,
ruolo/piattaforma non corrispondenti, symlink nel percorso degli artefatti e
qualsiasi `flight_approval=true`.

## Packaging deterministico

`tools/deployment/build_release_manifest.py` genera soltanto il manifesto; non
installa unità, non cambia symlink e non riavvia processi. Release, commit, UTC,
ruolo, piattaforma, stato SIM e ogni artefatto sono parametri espliciti. Gli
hash vengono calcolati sui file della directory candidata e il risultato viene
validato prima della scrittura atomica. Un manifesto esistente non viene mai
sovrascritto implicitamente.

Esempio EMS di staging:

```sh
python3 tools/deployment/build_release_manifest.py \
  --release-root /home/pi/efis-next-ems-<commit> \
  --release-id ems-<commit> \
  --source-commit <sha40> \
  --built-utc 2026-09-20T06:00:00Z \
  --role EMS \
  --platform RPI4_AARCH64 \
  --simulation-mode \
  --artifact bin/efis-ems-qt:exec
```

`--simulation-mode` e `--no-simulation-mode` sono mutuamente esclusivi e
obbligatori: il packager non deduce né nasconde la provenienza dei dati.

## Preflight non eseguibile

`tools/deployment/prepare_release_transition.py` lega candidato, rollback,
manifesto e policy del nodo in un record conforme a
`config/deployment/efis-release-transition-plan-v1.schema.json`. Il record ha
sempre:

- `status=PREPARED_NOT_EXECUTED`;
- `execution_authorized=false` e `flight_approval=false`;
- decisione `NO_GO_UNTIL_SEPARATE_HARDWARE_AUTHORIZATION`;
- zero comandi mutanti;
- digest SHA-256 di manifesto e policy;
- nove gate obbligatori, inclusi supervisore unico, reboot e rollback drill.

Il preflight rifiuta target relativi, symlink, candidato uguale al rollback,
manifesto esterno alla release e profilo nodo/ruolo ambiguo. Esso non sostituisce
la conferma read-only dello stato corrente né autorizza lo switch.

I cinque drop-in sotto `deploy/systemd/release-v1/` sono gli asset canonici;
MFD, EMS e Blackbox sono stati installati e verificati il 2026-09-20. Usano
`ExecCondition`, così un manifesto assente o non valido rende
l'unità non avviabile senza generare un ciclo di restart su un binario non
verificato. Il validatore va installato separatamente e in sola lettura come
`/usr/local/lib/efis-next/validate_release_manifest.py`; la sua presenza non
autorizza l'abilitazione dei drop-in.

Gli eseguibili canonici sono sempre file regolari sotto `bin/`. Alias di build
come `build-rpi5 -> build/release` sono utili durante lo sviluppo, ma non sono
ammessi nel percorso verificato dal manifesto: il validatore rifiuta ogni
symlink intermedio e l'unità esegue esattamente lo stesso file controllato.

## Sequenza di futura attivazione

Questa sequenza richiede un ordine separato e una finestra di banco:

1. acquisire unità/processi/autostart correnti e salvare l'evidenza;
2. costruire in staging e generare tutti gli hash;
3. validare manifest e artefatti senza fermare il servizio corrente;
4. installare l'unità candidata disabilitata e verificarne la sintassi;
5. preparare il precedente target `current` come rollback;
6. fermare una sola unità, cambiare atomicamente il symlink, avviare;
7. verificare PID, `ExecStart`, hash, porte, renderer e provenance;
8. eseguire reboot e fault test specifici del nodo;
9. in caso di gate rosso, ripristinare il symlink precedente e documentare;
10. aggiornare `EFIS_NEXT_ACTIVE_RELEASES.md` solo con evidenza acquisita.

## Stato hardware 2026-09-20 e gap attuali

- PFD: release corretta, ma recorder ancora da `build/dev`.
- MFD: release `mfd-3432402-primary-v1`, supervisore systemd, rollback e reboot
  verificati.
- EMS: release `ems-7655799-primary-v1`, rollback e reboot verificati; le
  scritture runtime sono fuori dalla release; EDP Blackbox è `LIVE • SIM`.
- Blackbox: release `blackbox-7655799-primary-v1`, rollback, reboot e broker
  health verificati; `--engine-simulation` resta correttamente esplicito.

Nessuno di questi gap viene chiuso dalla sola presenza del contratto V1.
Il packager e il preflight riducono il rischio di staging ma non costituiscono
evidenza hardware, reboot o rollback. L'evidenza del rollout eseguito è in
`docs/status/evidence/release-immutability-20260920/`. Il successivo batch
PFD+recorder del 2026-09-21 ha completato manifest, rollback e reboot per tutti
i processi avionici gestiti; `PROD-005` è chiuso per il solo contratto di
update/rollback, senza implicare approvazione al volo.

## Batch controllato PFD + recorder

Il PFD e il recorder condividono intenzionalmente lo stesso puntatore
`/home/efis/efis-next-current`, ma conservano due servizi e due
`ExecCondition` indipendenti. Lo stesso manifesto deve dichiarare entrambi i
ruoli e i due artefatti canonici:

- `bin/efis-pfd-qt`;
- `bin/efis-recorder`.

Gli asset del batch sono:

- `deploy/scripts/stage_pfd_recorder_immutable_releases.sh` — staging
  fail-closed, senza switch o riavvio;
- `deploy/scripts/activate_pfd_recorder_immutable_release.sh` — installazione
  dei gate, switch atomico, prova candidato, rollback reale e ritorno al
  candidato;
- `deploy/scripts/verify_pfd_recorder_reboot_acceptance.sh` — verifica
  read-only successiva al reboot.

La release primaria è `pfdrec-d52e377-primary-v1`; il rollback indipendente è
`pfdrec-60b3526-rollback-v1`. Entrambe devono essere compilate nativamente su
ARM64. La seconda non riutilizza il recorder legacy da `build/dev`: compila
PFD e recorder dallo stesso commit `60b3526`, così `source_commit` resta vero
per entrambi gli artefatti.

Il recorder scrive esclusivamente sotto
`/home/efis/.local/state/efis-next/`; il PFD e i due servizi scrivono nel
journal. Il precedente drop-in `release.conf`, che scriveva il log dentro la
release, viene conservato nell'evidence bundle e rimosso dal set attivo. In
caso di errore lo script ripristina puntatore e drop-in precedenti prima di
riavviare i servizi legacy.

Lo staging emette `EXECUTION_AUTHORIZED=false`: il suo completamento non
autorizza lo switch. L'attivazione richiede una finestra di banco esplicita,
`get_throttled=0x0`, bus systemd utente disponibile e i due servizi nominativi.
`PROD-005` non può essere chiuso prima di manifest PFD/RECORDER PASS, prova di
rollback, ritorno al candidato, reboot acceptance, stato recorder crescente e
aggiornamento del registro canonico.

## Batch MFD leggibilità PFD 76afead

La correzione di leggibilità PFD validata in CI viene preparata prima sul MFD
RPi5, mai direttamente sul PFD primario. La sorgente candidata è il commit
`76afead639599a08cb187bf93f1127c84a4df9ed`, che include sia le correzioni HMI
del commit `89cf41d` sia il reset fail-closed della vecchia
`ConditionPathExists=.../build-rpi5` verso il solo artefatto canonico
`bin/efis-pfd-qt`.

Gli asset sono:

- `prepare_mfd_legibility_immutable_release.sh`: verifica archivio e rollback,
  compila nativamente ARM64, esegue i test mirati e produce manifesto MFD senza
  modificare servizio o puntatore;
- `activate_mfd_legibility_immutable_release.sh`: richiede nodo, architettura,
  supervisore unico e `get_throttled=0x0`, installa il gate systemd, esegue
  switch atomico, prova candidata, rollback reale e ritorno alla candidata;
- `verify_mfd_legibility_reboot_acceptance.sh`: verifica read-only dopo reboot
  processo, binario, manifesto, porta DDP, conflitti display, alimentazione e
  journal. L'acceptance visuale/touch resta esplicitamente `NOT_RUN`.

L'archivio riproducibile si genera con:

```sh
git archive --format=tar.gz --prefix=efis-next-76afead/ \
  --output=efis-next-76afead.tar.gz 76afead639599a08cb187bf93f1127c84a4df9ed
```

SHA-256 atteso:
`b318d9da3914826cdc4c5a07b5803be25d17973b4685df64633646a053ee740b`.
La candidata prevista è `/home/pi/efis-next-release-mfd-76afead-v1`; il
rollback resta `/home/pi/efis-next-release-mfd-3432402-v1`. Nessuno degli
script autorizza il volo e la preparazione termina con
`EXECUTION_AUTHORIZED=false`.
