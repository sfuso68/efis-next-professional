# Chiusura blocco preflight read-only — 2026-10-19

Implementato `tools/deployment/check_rollout_readiness.py`, testato con 15
unit test su Windows e Linux ARM64. La CI Project Control esegue lo stesso
modulo. Non contiene comandi di attivazione; stdout JSON, exit 2 se bloccato,
0 soltanto per controlli automatici PASS, authorization sempre false.

Riferimento applicazioni a0be9a5; base del checker main dopo merge #49
4aea1e4. Pin di hash per tutti i manifesti candidati E precedenti, checksum
artefatti, piattaforma fisica/architettura, set completo dei ruoli, puntatore
current ed eseguibili dei PID. Un semplice servizio active non basta.

Esecuzione reale sui quattro nodi: `mfd.json`, `pfd.json`, `ems.json`,
`blackbox.json`. Nessun restart, reboot, switch, permission change o scrittura
bus; file del checker e report solo in nuove directory di staging.

- PFD: controlli automatici PASS; MFD canary e acceptance manuale aperti.
- MFD: tutti i controlli tranne alimentazione PASS (0x50000 storico).
- EMS: tutti tranne privilegio non interattivo PASS; sudo locale operatore
  è distinto e non viene richiesto NOPASSWD globale.
- Blackbox: tutti tranne alimentazione PASS (0x50000 storico). Una prima
  versione non poteva leggere `/proc/1301/exe` per ownership differente;
  report iniziale conservato. Fallback testato, limitato a sudo/readlink di
  quel PID, risolve la lettura senza eseguire Python/servizi come root.
- Sensore .89 ancora SSH timeout al controllo; non interrogato alcun bus.

Serverha osservato: GITHUB_SYNC_OK su 4aea1e4; backup report del
20261019T175111Z con main 4aea1e4, restore PASS e refs NAS preservati.
La lettura del report non è un nuovo restore completo eseguito da questo tool.

Non eseguiti attivatori storici. Da revisione del codice: il MFD storico punta
a 76afead e arma recovery prima del gate power; un gate fallito può provocare
restart del precedente servizio. Il nuovo blocco non riusa quell'attivatore.
Il suo sostituto transazionale deve essere verificato separatamente prima di
esecuzione root; test del solo preflight non valgono come test del recovery.

Regressione locale: 40 test eseguiti, 39 PASS e un test legacy SKIP per
disponibilità symlink Windows; non contato come PASS. I 15 test nuovi passano
senza skip anche su ARM64. Workflow YAML parsato con PyYAML 6.0.2 sul MFD,
generatori status/dashboard e relativi --check PASS; diff --check pulito.

Runbook operativo: `docs/deployment/ACTIVATION_UNBLOCK_2026-10-19.md`.
Foto/identità PSU e percorso di alimentazione, piano approvato cold/warm/load,
finestra operatore, rollback/reboot/pannelli restano aperti. Stato requisiti
invariato e nessuna approvazione al volo.
