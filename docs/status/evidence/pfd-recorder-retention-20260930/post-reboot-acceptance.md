# PFD retention — post-reboot acceptance

Data: 2026-09-30

- boot ID precedente: `be165369-0f3a-4477-a095-b45ccfab6f32`;
- boot ID successivo: `b52439a1-a261-464d-9005-6025682d1182`;
- release corrente:
  `/home/efis/efis-next-releases/pfdrec-7051a47-retention-v1`;
- `efis-pfd.service`: active/running, PID 1495, eseguibile canonico;
- `efis-recorder.service`: active/running, PID 1419, eseguibile canonico;
- recorder status: `EFIS_RECORDER_STATUS_V3`, `state=RECORDING`, due campioni
  crescenti da 1.050 a 1.200 record;
- retention attiva, 6.988 segmenti completati, 25.768.818.239 byte;
- spazio disponibile riportato dallo status: 17.422.659.584 byte;
- filesystem root: 71%, 17.421.307.904 byte disponibili;
- DDP UDP 49002 attivo; log PFD LIVE ~20 Hz, zero rejected/gap/socket error;
- hash binari uguali al manifest;
- release e binari root-owned e non scrivibili;
- `vcgencmd get_throttled=0x0`; temperatura 52,7 °C.

Campione successivo della stessa sessione:

- recorder `NRestarts=0`, status V3 `state=RECORDING` e 39.700 record nel
  segmento corrente;
- seconda potatura post-reboot: 12 segmenti e 43.890.672 byte;
- 6.989 segmenti completati, 25.768.985.137 byte;
- 17.422.450.688 byte disponibili nello status e 17.421.615.104 byte nel
  filesystem;
- PFD ancora al solo retry iniziale, recorder stabile, throttled `0x0`.

Osservazioni aperte:

- PFD `NRestarts=1` durante l'avvio; il processo corrente è poi rimasto stabile
  e systemd riporta `Result=success`. Il log del primo processo non è presente;
- `weather_simulation` ha bloccato il callback per 10.132 ms e l'event loop per
  10.212 ms; non è un PASS prestazionale;
- soak >=24 h resta obbligatorio; la rotazione/potatura successiva è stata
  osservata ma non sostituisce il requisito temporale.
