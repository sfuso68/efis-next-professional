# AVIONIX NEXT — sessione di acceptance HMI hardware

Stato: harness software verificato; prove PFD/MFD/EMS non eseguite da questo
documento. Attività: `AVX-G3-03`, `PRJ-004`.

## Scopo

`manage_hmi_hardware_session.py` chiude il vuoto operativo fra il piano HMI
A20 e il packager P20. Prepara una directory nuova e fail-closed per i tre
ruoli, poi consente la finalizzazione soltanto quando un operatore ha compilato
tutte le misure e depositato tutti gli artefatti reali richiesti.

Il tool non controlla display, touch, rete o fault; non esegue reboot; non
genera screenshot; non attribuisce esiti hardware. Ogni sessione nuova resta
`PREPARED_SW_NOT_RUN` e contiene un `results.json` esplicitamente `NOT_RUN`.

## 1. Preparazione

Usare il commit completo realmente installato sui nodi e una directory nuova:

```text
python3 tools/ui/manage_hmi_hardware_session.py init \
  --source-commit <40-hex> \
  --output output/hmi-acceptance/<UTC-session-id>
```

La sessione contiene:

- copia immutata del piano e relativo SHA-256;
- `session.json` con `flight_approval=false`;
- `pfd/run.json`, `mfd/run.json`, `ems/run.json` vuoti;
- una directory `artifacts/` per ciascun ruolo;
- `results.json` `NOT_RUN`, che non può essere passato al packager.

Il comando rifiuta directory esistenti, commit abbreviati e overwrite.

## 2. Evidenze richieste per ciascun ruolo

Ogni `run.json` deve conservare esattamente le proprietà `role`, `node`,
`release`, `measurements` e `artifacts`. `release` è l'object ID completo della
release effettivamente provata. Gli otto artefatti obbligatori sono:

1. `SCREENSHOT` — stati richiesti e DAY/NIGHT sul pannello reale;
2. `FRAME_TIMING` — minimo FPS e p95 frame-time sull'intera prova;
3. `THERMAL` — temperatura massima e storico `get_throttled`;
4. `TOUCH_LOG` — target provati e risultato operatore;
5. `BOOT_LOG` — cold boot, reboot, servizio, release e restart;
6. `TRANSPORT_REPLAY` — DDP per PFD/MFD, EDP per EMS;
7. `FAULT_RECOVERY` — perdita Ethernet, stato fail-closed e recovery;
8. `OPERATOR_SIGNOFF` — leggibilità DAY/NIGHT e touch, firmati.

I percorsi nella mappa `artifacts` sono relativi alla radice della sessione.
File vuoti, symlink, path assoluti, escape `..`, ruoli mancanti, soglie non
raggiunte e release abbreviate fanno fallire la finalizzazione.

## 3. Soglie bloccanti

- durata per ruolo almeno 60 minuti;
- minimo 40 FPS;
- p95 frame-time non superiore a 25 ms;
- `throttled` esattamente `0x0`;
- target touch minimo 44 px;
- tutti gli stati `LIVE`, `STALE`, `DEGRADED`, `LOST`, `FAIL`, `SIM`;
- entrambi i modi `DAY`, `NIGHT`;
- cold boot, reboot, replay e fault/recovery positivi.

Una temperatura numerica viene registrata, ma non viene inventato qui un
limite ambientale di prodotto.

## 4. Finalizzazione

La finalizzazione è autorizzata soltanto dopo osservazione reale e firma
dell'operatore:

```text
python3 tools/ui/manage_hmi_hardware_session.py finalize \
  --bundle output/hmi-acceptance/<UTC-session-id> \
  --operator '<nome operatore>' \
  --confirmation I_CONFIRM_REAL_HARDWARE_OBSERVATION
```

Il comando:

1. ricontrolla schema, digest del piano, corrispondenza con il piano canonico
   del repository e percorsi confinati;
2. valida le tre run e le soglie con il validatore P20 esistente;
3. crea atomicamente `results-ready.json`;
4. assembla `evidence.json` con SHA-256;
5. rilegge il manifesto con il validatore A20 indipendente.

Né risultati né evidence vengono sovrascritti. Se il controllo downstream
fallisce, gli output intermedi creati dalla finalizzazione vengono rimossi.

## Limiti

Un bundle PASS dimostra l'esecuzione del piano documentato sul banco; non è una
certificazione e conserva sempre `flight_approval=false`. Finché i tre
`run.json` non sono compilati da prove reali, `AVX-G3-03` resta `OPEN` e
`PRJ-004` resta `VERIFY-HW`.
