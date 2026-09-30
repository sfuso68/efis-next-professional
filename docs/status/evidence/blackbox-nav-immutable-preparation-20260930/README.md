# Blackbox NAV immutable release — preparazione software

Data: 2026-09-30

Requisiti: `NAV-003`, `BBX-003`, `PROD-005`

Stato: **PREPARED-SW / NOT DEPLOYED / DEVELOPMENT / NON-CERT**

## Risultato

La policy fail-closed di `bbxnav`, già verificata sulla Blackbox il 2026-09-29,
è ora trattata come artefatto della release immutabile Blackbox anziché come
file esterno non coperto dal manifesto.

La candidata è fissata a:

- release `blackbox-46e56a9-nav-supervision-v1`;
- commit `46e56a955b6c3a8bdf647789930c3e7855051fa5`;
- archivio sorgente SHA-256
  `cb969e12e3f055f5be2aeff1c71f13abbcfaca967bb11f2d8223809ada065b1c`;
- rollback `blackbox-7655799-v1`.

Il manifesto deve coprire entrambi gli artefatti:

- `bin/efis-blackbox-broker`;
- `share/systemd/bbxnav-fail-closed.override.conf`.

## Proprietà fail-closed

- preparazione senza switch o riavvio e con `EXECUTION_AUTHORIZED=false`;
- identità nodo, architettura, release attiva e spazio minimo verificati;
- archivio e commit fissati;
- candidata non sovrascrivibile e artefatti root-owned/read-only;
- drop-in systemd collegato all'asset della release attiva;
- salvataggio e ripristino del precedente file o symlink;
- rollback reale del broker e della policy NAV;
- acceptance con ricevitori assenti: exit 2, `NRestarts=0` in due campioni;
- verifica separata dopo reboot;
- `throttled=0x0` obbligatorio prima e dopo l'attivazione.

## Verifica effettuata

Solo software: test statici dei contratti di rollout, validatore della policy,
test supervisione e controllo sintattico degli script. Nessuna Blackbox, unità
systemd, sorgente NAV, CAN o release attiva è stata modificata.

## Resta aperto

Occorrono una finestra hardware esplicita, trasferimento dell'archivio,
compilazione ARM64, attivazione, rollback, prova exit 2, reboot acceptance e
raccolta dell'evidence reale. Ricevitori, antenna, RF, frequenze, decoder e
annunci display restano non qualificati. `NAV-003` e `BBX-003` restano
`PARTIAL`; `flight_approval=false`.
