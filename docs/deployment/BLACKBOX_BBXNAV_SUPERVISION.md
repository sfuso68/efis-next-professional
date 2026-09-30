# Blackbox bbxnav — supervisione fail-closed

Stato: **VERIFIED-SW + VERIFIED-HW / DEPLOYED 2026-09-29**

## Problema osservato

Il 2026-09-29 la Blackbox reale ha eseguito `bbxnav` con
`--interface vcan0 --source ricevitori`. Nessun ricevitore configurato era
avviabile; il processo ha terminato correttamente con exit code 2, ma l'unità
legacy usava `Restart=always` e `StartLimitIntervalSec=0`. Il risultato è stato
un ciclo ogni cinque secondi, fino a 157.393 restart osservati.

L'assenza dei ricevitori deve produrre NAV non disponibile, non dati simulati e
non un restart storm.

## Politica

`deploy/systemd/bbxnav-fail-closed.override.conf` modifica esclusivamente la
supervisione:

- `Restart=on-failure`;
- `RestartPreventExitStatus=2` per configurazione/ricevitori non validi;
- massimo tre avvii in 60 secondi per gli altri guasti;
- nessuna modifica a `ExecStart`, sorgente, interfaccia CAN, utente o
  capability.

Il servizio resta visibilmente `failed` dopo exit 2 e non viene rilanciato.
Questo è intenzionale. Dopo aver collegato e verificato i ricevitori, un
operatore deve eseguire un nuovo start esplicito. Non è autorizzata la sorgente
simulata nel servizio operativo.

## Gate software

```bash
python3 tools/navigation/validate_bbxnav_supervision.py
python3 -m unittest tests.test_bbxnav_supervision -v
```

## Deployment separato e reversibile

Questi comandi sono autorizzabili soltanto in una finestra hardware dedicata:

```bash
sudo install -d -m 0755 /etc/systemd/system/bbxnav.service.d
sudo install -m 0644 deploy/systemd/bbxnav-fail-closed.override.conf \
  /etc/systemd/system/bbxnav.service.d/20-fail-closed.conf
sudo systemctl daemon-reload
sudo systemctl reset-failed bbxnav.service
sudo systemctl start bbxnav.service
```

Acceptance con ricevitori assenti:

```bash
systemctl show bbxnav.service \
  -p ActiveState -p SubState -p Result -p ExecMainStatus -p NRestarts
sleep 15
systemctl show bbxnav.service -p NRestarts
```

Esito atteso: `ExecMainStatus=2`, servizio non attivo e `NRestarts` invariato
nel secondo campione. Broker, recorder, DDP/EDP e CAN di produzione devono
restare invariati.

Rollback:

```bash
sudo rm /etc/systemd/system/bbxnav.service.d/20-fail-closed.conf
sudo systemctl daemon-reload
```

Il rollback rimuove soltanto il drop-in; non avvia il servizio e non autorizza
il precedente restart loop.

## Esito hardware 2026-09-29

Il drop-in è stato installato sulla Blackbox reale. Con ricevitori assenti il
processo termina con exit 2, l'unità resta `failed` e `NRestarts` è rimasto 0
nei due campioni. Broker e `bbxlog` sono rimasti `active` con zero restart.
L'evidenza è in
`docs/status/evidence/bbxnav-fail-closed-20260929/README.md`.

## Integrazione nella release immutabile candidata

Il rollout successivo non copia più il drop-in come file non tracciato. La
release candidata `blackbox-46e56a9-nav-supervision-v1` dichiara nel proprio
manifesto sia il broker sia
`share/systemd/bbxnav-fail-closed.override.conf`. Il collegamento systemd punta
all'asset della release attiva; cambio release e cambio policy restano quindi
coerenti e verificabili tramite SHA-256.

La procedura è divisa in tre fasi:

1. `prepare_blackbox_nav_immutable_release.sh` verifica archivio/commit,
   compila il broker su ARM64, esegue i test e crea il manifesto senza mutare
   il runtime;
2. `activate_blackbox_nav_immutable_release.sh`, eseguibile solo in una futura
   finestra autorizzata, conserva il drop-in precedente, attiva la candidata,
   prova l'exit 2 senza restart, esegue un rollback reale e torna alla
   candidata;
3. `verify_blackbox_nav_reboot.sh` verifica dopo reboot manifesto, binario,
   symlink del drop-in, broker, exit 2 e `NRestarts=0`.

La preparazione termina esplicitamente con `EXECUTION_AUTHORIZED=false`. Gli
script non abilitano sorgenti simulate NAV e non modificano il comando legacy,
la sorgente `ricevitori`, `vcan0`, utente o capability. La release corrente
resta `blackbox-7655799-primary-v1` finché le fasi hardware non sono state
autorizzate ed eseguite.

Preparazione dell'archivio da una copia Git fidata:

```bash
git cat-file -e 46e56a955b6c3a8bdf647789930c3e7855051fa5^{commit}
git archive --format=tar \
  --output=/tmp/efis-next-46e56a955b6c3a8bdf647789930c3e7855051fa5.tar \
  46e56a955b6c3a8bdf647789930c3e7855051fa5
echo 'cb969e12e3f055f5be2aeff1c71f13abbcfaca967bb11f2d8223809ada065b1c  /tmp/efis-next-46e56a955b6c3a8bdf647789930c3e7855051fa5.tar' \
  | sha256sum --check --strict
```

La futura sessione sul nodo deve creare
`/home/pi/efis-release-blackbox-nav-stage`, trasferirvi l'archivio con il nome
atteso e poi eseguire, nell'ordine:

```bash
bash deploy/scripts/prepare_blackbox_nav_immutable_release.sh
sudo bash deploy/scripts/activate_blackbox_nav_immutable_release.sh
# reboot autorizzato e successivo controllo read-only
sudo bash deploy/scripts/verify_blackbox_nav_reboot.sh
```

Il secondo e il terzo comando non sono autorizzati dalla sola presenza di
questo documento o degli script.
