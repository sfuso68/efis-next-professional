# MFD a0be9a5: attivatore candidato, NON attivato

Ambito PROD-005/PWR-001, DEVELOPMENT/NON-CERT, flight_approval=false.
`tools/deployment/activate_mfd_a0be9a5.py` è standalone e separato dall'EMS:
non si cambia lo script EMS già consegnato con SHA fissato. Duplicazione
temporanea intenzionale; generalizzazione richiede revisione separata.

## Preparato e verificato soltanto software

23 test ARM64 PASS sul MFD, Windows 22 PASS/1 SKIP (privilegio symlink).
Test di fault/orchestrazione con mock e fixture: non equivalgono a rollback
reale o a installazione root del drop-in. Pin di candidato e precedente,
configurazione e runtime osservati realmente PASS, PID 1328 invariato.
Staging isolato `/home/pi/efis-mfd-activation-test-20261019-v1`.
SHA script `3d3e259e80af94fa6493023c4a7061392d7261f736657bde0cf3662baad23502`.

Il preflight reale --check-only termina correttamente exit 1 con
`qualified power approval missing`; nessuna mutazione, lock, stop o recovery.
Non viene fornito un comando di attivazione in questa fase.

## Sequenza candidata e ripristino

Pin archivio a0be9a5, binario, manifesti, profilo, unità e validatore globale
root; ricostruzione root/read-only prevista sotto
`/opt/efis-next-mfd-releases/mfd-a0be9a5-main-v1`. Puntatore attuale e vecchia
release non modificati. Recupero armato solo subito prima del primo stop.
Nessuna modifica rete, firmware, terreno/imagery o profili locali.

Differenza importante rispetto all'EMS: l'unità MFD corrente conserva
ConditionPathExists sul vecchio alias build-rpi5. L'attivatore candidato
aggiunge esclusivamente un blocco [Unit] che azzera quella condizione e
controlla il binario canonico corrente; lascia invariato il resto del drop-in
con SHA fissato. Scrittura atomica e daemon-reload soltanto DOPO il primo
stop. Se la transazione fallisce tenta il ripristino del drop-in originale
prima del ritorno alla vecchia release. Un errore di ripristino viene segnalato
anche se si riesce a riavviare la vecchia applicazione. Nessun alias artificiale
build-rpi5 nella nuova directory. Questa installazione root NON è stata provata.

Sono preservati HOME, whitelist environment, KMS, sorgente DDP 47 e percorsi
terrain/imagery esterni. Il futuro drill candidata→precedente→candidata
verifica PID, socket DDP, startup fresco, render/DDP LIVE e zero restart.
Non effettua reboot. Visuale, reboot e power-loss non dedotti dai test.

## Gate fisico: non aggirarlo

Richiede sia get_throttled=0x0 sia un record root non scrivibile da altri,
`/var/lib/efis-next/qualification/mfd-power-approval.json`, per questo boot.
Non viene creato automaticamente. Campi esatti: schema
`efis-next.qualified-power.v1`, node MFD, boot_id corrente, chain_identified
true, representative_load_pass true, approved_by non vuoto, evidence_sha256
del rapporto fisico approvato. Il record è attestazione dell'approvatore,
non una misura automatica: non compilarlo con valori fittizi per ottenere PASS.
Inventario PSU/cavo e prova a carico rappresentativo restano da concordare.
Un reboot che elimina lo storico non dimostra risoluzione della causa.

Prossimo passo: identificare PSU e percorso alimentazione di MFD e Blackbox;
qualificare separatamente i due nodi. Solo dopo revisione e finestra autorizzata
verranno fissati comando hash-checked e prove di attivazione del canary MFD.
