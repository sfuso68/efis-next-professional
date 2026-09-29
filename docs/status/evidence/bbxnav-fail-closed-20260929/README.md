# Blackbox bbxnav fail-closed acceptance — 2026-09-29

Stato: **VERIFIED-SW + VERIFIED-HW / NAV RECEIVERS STILL ABSENT**

Nodo: `efisblackbox` (`192.168.1.47`, ARM64).

## Condizione iniziale

- unità legacy `/etc/systemd/system/bbxnav.service`;
- comando invariato: `/usr/local/bin/bbxnav --interface vcan0 --source ricevitori`;
- ricevitore NAV1 seriale `46817136` non rilevato;
- NAV2 senza frequenza operativa;
- il processo terminava con exit code 2;
- `Restart=always` e `StartLimitIntervalSec=0` avevano prodotto 157.422
  restart al campione immediatamente precedente la correzione.

## Correzione

È stato installato soltanto il drop-in versionato:

`/etc/systemd/system/bbxnav.service.d/20-fail-closed.conf`

SHA-256:
`4a2e33ce3127b2baa41246037376bf6821465d583cdd3f5df9b43c502e4eb116`

La policy applica `Restart=on-failure`, impedisce il restart su exit status 2
e ripristina un limite di tre tentativi in 60 secondi per altri errori. Non
modifica comando, sorgente, interfaccia, utente o capability.

## Evidenza

- validator Windows: PASS;
- 4/4 test unitari positivi/negativi: PASS;
- `systemd-analyze verify` sul file unito ARM64: PASS;
- dopo start senza ricevitori: `ActiveState=failed`, `Result=exit-code`,
  `ExecMainStatus=2`;
- `NRestarts=0` nel primo campione e ancora `0` dopo 12 secondi;
- broker Blackbox prima/dopo: `active`, `NRestarts=0`;
- `bbxlog` prima/dopo: `active`, `NRestarts=0`;
- destinazione resta `vcan0`; nessun bus CAN reale o dato simulato abilitato.

Decisione: **RESTART STORM CLOSED**. Lo stato `failed` di `bbxnav` è il
comportamento fail-closed corretto finché i ricevitori non sono presenti e
validati. Restano aperti inventario SDR, frequenze, antenna, qualità RF,
decoder, annunci PFD/MFD e integrazione nel broker professionale.
