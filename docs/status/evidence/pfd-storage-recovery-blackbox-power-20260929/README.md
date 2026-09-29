# PFD storage recovery and Blackbox power diagnosis — 2026-09-29

Stato: **PFD RECOVERY PASS / BLACKBOX POWER VERIFY-HW**

Finestra locale: 2026-09-29 20:58–21:17 CEST.

Questa evidenza distingue il recupero effettivamente eseguito sul PFD dalla
diagnosi read-only della Blackbox. Non costituisce approvazione al volo.

## PFD `efis-pfd-01` (`192.168.1.45`)

La sorgente legacy è stata validata prima di ogni rimozione:

- percorso risolto `/home/efis/efis-next/records`;
- directory reale, non symlink, distinta dal percorso recorder attivo
  `/home/efis/.local/state/efis-next/records`;
- recorder fermato e timer imagery fermato durante la copia;
- 49.597 file regolari, 11.525.831.016 byte.

La copia è stata acquisita da `serverha` con una chiave temporanea limitata per
IP e senza forwarding/PTY in:

`/home/sfuso/archives/pfd-legacy-records-20260929/records`

Il primo `rsync` ha terminato con exit code 0. Un secondo `rsync -aHnc
--delete --itemize-changes` ha riletto e confrontato i contenuti:

- destinazione: 49.597 file, 11.525.831.016 byte;
- differenze checksum: 0 righe, 0 byte;
- esito: `CHECKSUM_VERIFY=PASS`.

Solo dopo tale esito è stato svuotato esclusivamente il contenuto della
directory legacy verificata. Il filesystem root è passato da 100%, 0 byte
disponibili, a 81%, 11 GiB disponibili. La copia resta recuperabile su
`serverha`; la chiave temporanea è stata rimossa da `authorized_keys` del PFD.

Acceptance reale successiva:

- `efis-recorder.service`: `active/running`, PID `969557`, `NRestarts=0`
  stabile nel doppio campione;
- scrittura osservata nel percorso release attivo con segmento `.partial`
  recente;
- directory legacy ancora vuota;
- `efis-imagery-sync.timer`: `active`;
- run manuale imagery: `Result=success`, `ExecMainStatus=0`, 1.326 tile
  richieste/riusate, 0 download, 0 failure;
- package `treviso-belluno`: `OFFLINE_READY`, 1.326 tile verificate;
- resta aperto il gap di copertura già noto di 27 tile.

Decisione PFD: **STORAGE/RECORDER/IMAGERY RECOVERY PASS**. Restano aperte
acceptance visuale/touch, prestazioni terrain, retention automatica e soak.

## Blackbox `efisblackbox` (`192.168.1.47`)

Diagnosi read-only:

- Raspberry Pi 5 Rev 1.1, root su NVMe Crucial P310 500 GB;
- `get_throttled=0x50000`, temperatura 48,3–48,8 °C;
- cinque coppie undervoltage/normalizzazione il 28–29 settembre, durata
  osservata circa due secondi;
- nessun errore I/O/ext4, reset NVMe o disconnessione USB correlato nei log
  esaminati;
- broker Blackbox `active/running`, PID `1301`, `NRestarts=0`;
- Pixhawk presente su USB CDC ACM;
- tensione corrente non qualificata: `vcgencmd measure_volts EXT5V` non è
  supportato da questo firmware, quindi il solo storico kernel resta evidenza.

È stato inoltre rilevato un difetto separato: `bbxnav.service`, configurato con
`--source ricevitori` su `vcan0`, non trova i ricevitori previsti e rimane in
`auto-restart`; il contatore osservato ha raggiunto 157.031. Il servizio non è
stato modificato o fermato in questa finestra. Il broker e la registrazione
Blackbox non hanno riavvii correlati.

Decisione Blackbox: **POWER VERIFY-HW / NAV SERVICE ACTION REQUIRED**. La
causa dei transienti non è provata; PSU, cavo, connettori e prova sotto carico
restano da qualificare. Il loop NAV va corretto con una unità fail-closed che
non riparta senza ricevitori/configurazione valida.
