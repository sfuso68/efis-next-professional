# PFD recorder retention — evidence index

Data: 2026-09-30

Classificazione: **SOFTWARE PASS / HARDWARE VERIFY-HW / SOAK OPEN**

## Archivio pre-potatura

- sorgente stabile dopo arresto controllato del recorder: 9.208 file,
  33.777.901.735 byte;
- destinazione serverha:
  `/home/sfuso/archives/pfd-active-records-pre-retention-20260930/records`;
- secondo `rsync -aHnc --delete --itemize-changes`: zero righe e zero byte di
  differenza;
- [`serverha-archive/checksum-diff.txt`](serverha-archive/checksum-diff.txt) è
  intenzionalmente vuoto; SHA-256:
  `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.

Un rsync iniziale è rimasto orfano dopo l'interruzione della sessione; sono
stati terminati soltanto i PID identificati. Il delta finale non ha trasferito
alcun file e il controllo completo successivo ha confermato zero differenze.
La chiave temporanea dedicata è stata rimossa da `authorized_keys` sul PFD, da
serverha e dalla workspace; le tre righe duplicate trovate sul PFD sono state
eliminate e l'assenza è stata ricontrollata. L'archivio verificato è rimasto
intatto con gli stessi 9.208 file e 33.777.901.735 byte.

## Build e release

- source commit: `7051a47839389f739f2d1e0ad22b385173299f61`;
- source archive SHA-256:
  `6b3ec3e7e05bd352e43852b8ec4d84a6c6a320d9f7a4d714bb74718f03fc59b5`;
- build nativa ARM64 GCC 14.2 e Qt 6.8.2: PASS;
- test ARM64 mirati core/storage/release: PASS;
- release: `pfdrec-7051a47-retention-v1`, `flight_approval=false`;
- manifest PFD e RECORDER: PASS;
- hash PFD: `a64edcf2d0fcf5f64f30d771926a60d4e0c2854763a2f082375c7630df828854`;
- hash recorder:
  `4cb136ec14a326e1cdf8c091cfc9324eb670e53c3b52bf84c292e77197e87ca5`.

L'archivio sorgente privato, l'albero di build e gli script temporanei sono
stati rimossi da `/tmp` dopo l'attivazione; la release immutabile non è stata
rimossa né modificata.

## Attivazione reale

- prima potatura: 2.222 segmenti e 8.008.825.406 byte;
- dopo potatura: 6.986 segmenti completati, 25.769.076.329 byte e
  17.422.503.936 byte disponibili;
- rollback reale a `pfdrec-d52e377-primary-v1`: PASS;
- ritorno alla candidata: PASS;
- reboot acceptance: PASS; nuovo boot ID
  `b52439a1-a261-464d-9005-6025682d1182`;
- post-reboot: PFD e recorder attivi dai binari canonici, status V3 crescente,
  DDP LIVE ~20 Hz, root 71%/17,4 GB disponibili, throttled `0x0`.
- campione successivo: seconda potatura autonoma di 12 segmenti/43.890.672
  byte, recorder `NRestarts=0`, 17.421.615.104 byte disponibili.

Gli artefatti non sensibili acquisiti dal nodo sono in
[`pfd-activation/`](pfd-activation/). Il rapporto post-reboot è in
[`post-reboot-acceptance.md`](post-reboot-acceptance.md).

## Limiti dell'evidenza

Il soak di almeno 24 ore non è ancora maturato. Uno startup retry PFD e uno
stall `weather_simulation` di 10,1 s sono aperti. Non vengono dichiarati DONE,
non autorizzano l'uso in volo e non cambiano la classificazione
DEVELOPMENT/NON-CERT.
