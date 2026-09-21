# EFIS NEXT — Checkpoint release immutabile PFD + recorder

Data: 2026-09-20 / chiusura hardware 2026-09-21
ID: `PFD-001`, `PROD-005`
Stato: HARDWARE PASS / DEVELOPMENT / NON-CERT

## Audit read-only del nodo

Il nodo `efis-pfd-01` (`192.168.1.45`, ARM64) è stato letto senza modificare
servizi o release:

- puntatore attivo: `/home/efis/efis-next-avionics-60b3526`;
- PFD: `efis-pfd.service`, attivo, `NRestarts=0`, eseguibile dalla release;
- recorder: `efis-recorder.service`, attivo, eseguibile legacy dal checkout
  mutabile `build/dev`;
- spazio libero: 31 GiB;
- alimentazione/termica: `get_throttled=0x0`, 53,2 °C.

L'audit conferma il gap registrato e non costituisce activation acceptance.

## Implementazione software

1. Il drop-in PFD usa ora working directory immutabile e journal, senza
   scritture sotto `efis-next-current`.
2. Il drop-in recorder conserva record e status sotto
   `~/.local/state/efis-next`.
3. Lo staging produce una release dual-role PFD/RECORDER per `d52e377` e una
   rollback release indipendente da `60b3526`.
4. Il rollback non incorpora il recorder legacy: entrambi i binari provengono
   dallo stesso commit dichiarato.
5. L'attivatore verifica nodo, architettura, user bus, power history, manifest,
   hash, processi, porta DDP, recorder status e journal.
6. Il fault handler ripristina puntatore e drop-in precedenti.
7. La prova obbligatoria commuta candidato → rollback → candidato.
8. Il verificatore post-reboot controlla enable/active, PID/eseguibili, porta
   DDP, avanzamento recorder, hash, temperatura e throttling.

## Verifica software

- 25 test Python mirati: PASS;
- test di symlink: SKIP su Windows per privilegio OS, coperto dalla CI Linux;
- `git diff --check`: PASS;
- sintassi shell: da ripetere su Linux/ARM64 prima dello staging.

## Verifica hardware reale

Con autorizzazione esplicita sono stati trasferiti gli archivi sorgente,
compilati nativamente su ARM64 e prodotti due manifest dual-role:

- primaria `pfdrec-d52e377-primary-v1`;
- rollback `pfdrec-60b3526-rollback-v1`.

Manifest, ruoli, piattaforma e digest dei due binari sono PASS. Il drill
candidato → rollback → candidato è PASS; un errore iniziale di proprietà dello
storage recorder è stato intercettato dal gate e il recupero automatico ha
ripristinato correttamente il runtime precedente. Dopo la correzione, PFD e
recorder sono entrambi partiti dai binari canonici della release e lo status
recorder è avanzato fuori dall'albero immutabile.

Il reboot acceptance del 2026-09-21 è PASS:

- boot ID da `0e51e200-330a-42f6-a539-6992a640aae9` a
  `be165369-0f3a-4477-a095-b45ccfab6f32`;
- PFD e recorder `active/running`, binari e drop-in canonici;
- porta DDP `49002` attiva e telemetria `LIVE`, circa 35 fps;
- recorder status crescente;
- `get_throttled=0x0`, temperatura 57,6 °C;
- `flight_approval=false` invariato.

Il PFD registra `NRestarts=1` al boot: il journal dimostra che è l'unico retry
previsto dall'`ExecStartPre` mentre viene creato `wayland-0`, seguito da avvio
stabile con `Result=success`; non è un crash applicativo. Le evidenze complete
sono in `evidence/release-immutability-20260920/pfd-recorder/`.

`PROD-005` soddisfa ora il criterio update/rollback dei processi avionici
gestiti. `PFD-001` resta `PARTIAL` per acceptance visuale/touch, terrain e fault
reali, non per il contratto di release.
