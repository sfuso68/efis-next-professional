# Preparazione attivazione EMS — 2026-10-19

Autorizzazione: preparare l'attivazione e consegnare il comando sudo esatto.
Nessuna autorizzazione dedotta a eseguire lo switch via agente: --activate NON
eseguito. Il servizio è ancora quello precedente, PID 949/NRestarts 0.

Implementato `tools/deployment/activate_ems_a0be9a5.py`, standalone stdlib,
pin sorgente a0be9a5/ELF/manifesti/profile, unità e validatore globale osservati.
File ore store invariato nel diff `src/core/engine_runtime_store.cpp` tra
62ba90b e a0be9a5; profilo attivo/candidato identico. Root nuovo target previsto
sotto /opt, non ancora creato. Nessuna modifica di servizio, runtime o rete.

17 test Linux ARM64 PASS in `arm64-tests.log`; Windows 16 PASS e uno SKIP per
symlink non autorizzati. Test fault sull'orchestrazione e file fixture isolati:
pre-gate e prepare failure senza restart; errori dopo mutazione e interrupt
richiedono recovery; recovery failure non mascherata; archivio malevolo respinto;
current regolare non sovrascritto, rename failure pulisce solo link temporaneo;
root helper non scrivibili da altri; un solo validatore globale isolato.

Preflight reale e lanciatore con verifica hash in memoria PASS, solo check-only.
Non testati su hardware: stop/start, rollback drill, sigillatura in /opt,
reboot o acceptance pannello della candidata. Tali esiti saranno acquisiti
solo dopo il comando operatore. La simulazione non è una qualifica del recovery.

SHA script consegnato e launcher nel runbook
`docs/deployment/EMS_A0BE9A5_ACTIVATION.md`; comando preflight non privilegiato
`check-signed-launcher.sh`. Validatore globale hash
`cbe02dd613140352ddea334d46de498708110436fc2d0a037aa741a4571fd368`.
Preflight e hash originali in `preflight.log` e `signed-launcher.log`.
