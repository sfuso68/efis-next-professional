# EMS a0be9a5 — attivazione operatore al banco

DEVELOPMENT/NON-CERT, SIM, nessuna approvazione al volo. Non usare in volo.
Il comando è preparato per il solo `pi@efis`, RPi4 `192.168.1.36`.
Non richiede modifiche sudoers; il sudo è quello della sessione locale.

## Preparazione verificata, non attivazione già eseguita

Script `/home/pi/activate_ems_a0be9a5.py`, SHA-256:
`7848ef0ee53add17aa8b04445ea6d588f823c3a4ae708e577a901bc6683564ce`.
17 unit test PASS su ARM64; Windows 16 PASS/1 SKIP symlink. Sono simulazioni
del flusso/fault, non una prova di switch reale. Preflight `--check-only` e
launcher con hash verificato provati sul vero EMS senza arrestare il servizio.
Current rimane `ems-62ba90b-hmi-cas-v1`, PID 949/NRestarts 0.

Candidate e rollback hanno lo stesso profilo aeromobile (hash fissato) e il
codice del runtime store non è cambiato tra i due commit. L'attivatore preserva
unità, drop-in, environment, calibrazioni QSettings, file ore, carburante e log.
I file runtime continuano le normali scritture del programma; non vengono
azzerati né ripristinati a valori precedenti durante il drill.

## Comando esatto nel terminale EMS

Lasciare la Blackbox accesa e il collegamento Ethernet operativo. Prepararsi a
tre brevi interruzioni del pannello (candidata → precedente → candidata).
La sigillatura iniziale può richiedere alcuni minuti e precede gli stop.

```sh
sudo /usr/bin/python3 -I -c '
import hashlib, pathlib
p = pathlib.Path("/home/pi/activate_ems_a0be9a5.py")
b = p.read_bytes()
if hashlib.sha256(b).hexdigest() != "7848ef0ee53add17aa8b04445ea6d588f823c3a4ae708e577a901bc6683564ce":
    raise SystemExit("HASH_MISMATCH: non eseguire")
exec(compile(b, str(p), "exec"), {"__name__": "__main__", "__file__": str(p)})
' --activate --bench-window
```

Il launcher legge/verifica una sola copia dei byte e la esegue in memoria:
nessun import dalla cartella utente, nessuna race check/riapertura dello script.
Non eseguire invece direttamente gli attivatori EMS storici con vecchi pin.

Esito atteso:

```text
EMS_A0BE9A5_ACTIVATION=PASS
ROLLBACK_DRILL=PASS
NEXT_REQUIRED=REBOOT_AND_VISUAL_ACCEPTANCE
EVIDENCE=/var/lib/efis-next/activation-evidence/ems-a0be9a5-...
```

Con FAIL/HASH_MISMATCH: non ripetere alla cieca, non riavviare e non cancellare
directory. Inviare le ultime righe, inclusi eventuali RECOVERY=PASS o
ACTIVATION_FAILED_AND_RECOVERY_FAILED. Un errore prima del primo comando di
stop non provoca recovery/restart. Dopo la prima mutazione tenta di tornare
alla release precedente; un ripristino fallito è segnalato esplicitamente.

## Cosa fa e cosa non fa

- Pin di host, modello, architettura, manifesti/binari/profilo, archivio sorgente,
  validatore root e unità attuale; 0x0, PID, environment, EDP e conflitti.
- Lock; ricostruzione da archivio git con SHA fissato e binario verificato in
  `/opt/efis-next-ems-releases/ems-a0be9a5-main-v1`, di proprietà root e read-only.
  Non copia/esegue helper arbitrari dalla candidata scrivibile.
- Nessuna modifica a unità, validatore installato, rete, firmware o sorgenti.
- Snapshot del piccolo file ore in evidenza privata root prima dei cambi;
  nessun rollback dei dati runtime. Vecchia release e staging restano intatti.
- Stop/switch atomico/start, controllo PID/hash, zero restart automatici,
  startup, link EDP LIVE con payload SIM, heartbeat di rendering e assenza di
  errori runtime; prova precedente e ritorno alla candidata.
- Recovery armato solo quando viene tentato il primo stop. Directory parziali
  e evidenze conservate in caso di errore; non vengono cancellati dati.

La continuità del rendering non è una misura di FPS qualificata, e il drill non
prova power-loss/SIGKILL, EMC, sensori, sonde o uso in volo. Nessun reboot
automatico. Dopo PASS servono verifica visiva e reboot acceptance separati.
Non ripetere il comando dopo PASS: il guard del current rifiuta il nuovo stato.
