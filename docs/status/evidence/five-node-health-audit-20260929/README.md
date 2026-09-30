# Five-node read-only health audit — 2026-09-29

Stato: **MIXED / ACTION REQUIRED**

Finestra UTC: 2026-09-29 18:48–18:55

Modalità: SSH read-only; nessun servizio, file runtime, rete o release modificati.

## PFD `192.168.1.45`

- host `efis-pfd-01`, `aarch64`, boot
  `be165369-0f3a-4477-a095-b45ccfab6f32`;
- release risolta:
  `/home/efis/efis-next-releases/pfdrec-d52e377-primary-v1`;
- `efis-pfd.service` è un'unità **utente**, `active/running`, PID `1495`,
  eseguibile nella release immutabile;
- `efis-recorder.service` è `activating/auto-restart`, `MainPID=0`,
  `NRestarts=101015`;
- ultimo `recorder.status`: 2026-09-27, `state=RECORDING`, segmento `.partial`;
- filesystem root 57 GiB: 55 GiB usati, 0 disponibili, 100%; inode 8%;
- principali occupazioni osservate: 31,8 GB in
  `~/.local/state/efis-next/records`, 11,6 GB in `~/efis-next/records`, 2,65 GB
  in `~/efis-next/output`;
- `efis-imagery-sync.service` fallisce ripetutamente con `ENOSPC`;
- `get_throttled=0x0`, temperatura 53,8 °C.

Decisione: **DEGRADED / STORAGE RECOVERY REQUIRED**. Non sono stati eliminati
o spostati record; non sono stati fermati servizi o timer.

## MFD `192.168.1.49`

- host `pi5`, `aarch64`, boot `519d1c00-ed5d-422a-a584-3b695bb91d62`;
- release `mfd-3432402-primary-v1`; servizio
  `efis-next-mfd-direct.service` `active/running`, PID `1395`, `NRestarts=0`;
- root 37% usata, temperatura 56,5 °C;
- `get_throttled=0x50000`; kernel: undervoltage il 2026-09-23 alle 18:37 e
  23:41, poi tensione normalizzata.

Decisione: **RUNNING / POWER VERIFY-HW**.

## EMS `192.168.1.36`

- host `efis`, `aarch64`, boot `ec0eed02-052f-426b-804e-53be70646081`;
- release reale `ems-62ba90b-hmi-cas-v1`, commit
  `62ba90bcc53799303af2fa1de5b8ec3b1b5fb35f`, manifesto valido e
  `simulation_mode=true`;
- `efis-ems.service` `active/running`, PID `949`, `NRestarts=0`, eseguibile
  nella release corrente;
- root 19% usata, `get_throttled=0x0`, temperatura 58,4 °C.

Decisione: **RUNNING / SIMULATION / REGISTRY CORRECTED**.

## Blackbox `192.168.1.47`

- host `efisblackbox`, `aarch64`, boot
  `40c08258-1f93-4a3a-ac8a-3df7da075c0e`;
- release `blackbox-7655799-primary-v1`, commit
  `765579959f7970a17eb212ca1ef13628aaa4d71f`, `simulation_mode=true`;
- `efis-blackbox-broker.service` `active/running`, PID `1301`, `NRestarts=0`;
- broker avviato esplicitamente con `--engine-simulation`; archivio flight
  osservato 1,1 MB;
- root NVMe 12% usata, temperatura 50,5 °C;
- `get_throttled=0x50000`; cinque eventi kernel di undervoltage osservati fra
  2026-09-28 21:43 e 2026-09-29 13:12, tutti seguiti da normalizzazione.

Decisione: **RUNNING / SIMULATION / POWER VERIFY-HW URGENT**.

## serverha e NAS

- `serverha` `x86_64`, boot `078d8253-a220-4e4e-994d-ed1d7d5b526c`;
- 12 container osservati `Up`, cinque marcati `healthy` dal proprio healthcheck;
- bare serverha e NAS espongono `main`
  `7aa45b66fc71824eb0dd4b1e6ffe898ecfcdb897`;
- `git fsck --full --strict` serverha PASS;
- timer NAS attivo e ultima replica `Result=success`, `ExecMainStatus=0`;
- restore NAS→serverha separato già verificato PASS nel bundle I-10.

Decisione: **HEALTHY FOR CURRENT DOCUMENTED SCOPE**. Le immagini container
restano non congelate da manifesti applicativi e la protezione off-site resta
aperta.

## Azioni richieste

1. autorizzare una procedura conservativa di recupero spazio PFD, con copia e
   hash prima di qualunque eliminazione;
2. arrestare il loop recorder e il timer imagery soltanto nella finestra
   approvata;
3. qualificare PSU/cavi MFD e Blackbox: lo storico `0x50000` è reale e recente;
4. dopo il recupero PFD, riavviare recorder e imagery e acquisire una nuova
   evidenza di avanzamento/assenza restart;
5. mantenere EMS esplicitamente SIM finché sensori, mapping e limiti non sono
   verificati.
