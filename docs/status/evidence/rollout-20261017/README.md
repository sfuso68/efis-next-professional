# Rollout main a0be9a5 — 2026-10-17

Autorizzazione: «Distribuiscile sui nodi disponibili». Sorgente integrato
`a0be9a52494ea7ccb5a3890e94d03b671162383b`, PR #48.

## Perimetro ed evidenze

`preflight.json` registra letture SSH reali, spazio, alimentazione, processi
e gate. I quattro `*-active-manifest.json` sono copie dei manifesti letti dai
puntatori correnti, non manifesti della nuova candidata. Il preparatore
`stage-mfd-a0be9a5.sh` è identico a quello eseguito sul MFD: SHA-256
`ffdd8aabffaeea37cf523c81716c13da57b8dfc8b7fa944d9e774825abd291e6`;
`bash -n` sul MFD PASS. La prova Bash locale Windows era bloccata dal sandbox
MSYS, non usata come esito di validazione.

Archivio privato da `git archive --prefix=efis-next/` sul commit completo:
SHA-256 `0ab89831f0887b1729aea5837feb98d329edb252bfe27e3dcad0b913b8334c15`.
Percorso remoto `/home/pi/efis-release-mfd-a0be9a5-stage/`.
Compilazione nativa GCC 14.2/aarch64, Qt Debian, Release/Ninja con due processi;
nessuna installazione di dipendenze di sistema. Log integrale conservato sul
nodo in `staging.log`, compresi warning QML plugin e GCC DDP, senza soppressione.

## Esito staging (05:31 UTC)

Build ARM64 dei tre target PASS. CTest **5/5 PASS**: weather service, profilo
aeromobile PFD, golden tools, contratto HMI, SkyView responsive. Sono test
software eseguiti sul Raspberry reale con Qt offscreen, NON acceptance del
pannello, golden raster GPU, provider radar live o georeferenziazione.

Il primo preparatore termina exit 1 dopo il PASS dei test e del manifesto
candidato: la vecchia release 3432402 non contiene il proprio validatore.
Il log originale `staging.log` conserva il fallimento. Il completamento
`complete-mfd-staging.sh` usa il validatore del sorgente corrente verificato
per controllare **entrambi** i manifesti e i rispettivi file: PASS. Nessuna
modifica alla release precedente. `ldd` non segnala librerie mancanti.

Candidata `/home/pi/efis-next-release-mfd-a0be9a5-v1`, release id
`mfd-a0be9a5-main-v1`. SHA-256 binario
`065ba0bda94cc30b03c4c79868be82546c5d02ddb58f89befb5f938dd606fad8`,
manifesto `252c6f374da0a1cf5feba3477fc7419d65000ba8480ffff0cce149775c385eaa`.
`STAGING=PASS`, `ACTIVATION=NOT_RUN_POWER_GATE`; il pacchetto in staging
non è ancora sigillato root-owned né installato come corrente.
MFD invariato: PID 1395, NRestarts 0, temperatura finale 60,4 °C,
get_throttled invariato 0x50000. Test e completion log conservati qui.

## Condizioni per la ripresa

- MFD: storico `0x50000`; qualificare alimentatore/cavo e ripetere prova
  sotto carico. Non riavviare soltanto per azzerare il registro.
- Blackbox: stesso storico; `bbxnav` exit 2, zero riavvii. Non forzato.
- PFD: `0x0`, ma piano canary impone MFD prima di PFD. Conservare il dual-role
  PFD+recorder e la retention quando si preparerà la sua nuova release.
- EMS: sano al controllo, ma `sudo -n` richiede password. Nessuna modifica
  a sudoers o ai meccanismi di accesso; attivazione richiede finestra root.
- Sensore `.89`: SSH timeout; G1 inventario/mapping/calibrazione ancora
  obbligatorio prima di distribuire adapter o interrogare il bus.

Nessuna release attivata, nessun riavvio, nessun cambio current, nessuna
potatura, scrittura bus, parametro Pixhawk o modifica firewall/rete.
Prima di attivare: riconfermare sorgente, gate, release precedente e supervisore;
adattare il piano di attivazione al nuovo commit senza eseguire script storici
che puntano a 76afead; manifest/rollback reali, reboot e acceptance visuale
rimangono obbligatori. DEVELOPMENT/NON-CERT, flight_approval=false.
