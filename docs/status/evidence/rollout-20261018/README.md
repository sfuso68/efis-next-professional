# Preparazione autonoma ARM64 — 2026-10-18

Fonte congelata: main `a0be9a52494ea7ccb5a3890e94d03b671162383b`,
riconfermata su GitHub via ls-remote durante la sessione. Nessun sorgente
avionico modificato: script conservati qui come evidenza delle operazioni.

## Completato e verificato

- MFD: 15/15 test CTest PASS sul Raspberry Pi 5, con core, safety,
  freshness, DDP, EDP, motore/runtime/store, link EMS, radar e contratti HMI.
  Un ulteriore test recorder storage-failure PASS; test software/fixture,
  non fault injection sull'archivio operativo o sulla rete di bordo.
- Compilato `efis-recorder` dallo stesso sorgente del renderer ARM64.
- Blackbox: broker compilato nativamente su MFD RPi5, pacchetto trasferito in
  `/home/pi/efis-release-blackbox-a0be9a5-stage/` sulla Blackbox. Checksum,
  manifesti candidata/rollback e dipendenze PASS sul nodo destinatario.
  Broker operativo PID 1301/NRestarts 0 invariato; binario candidato NON
  eseguito, nessun accesso CAN/MAVLink o modifica di bbxnav/meteo/hub.
  Archivio `85bf9aad2124d5e75504fe772f5fae69e319b0d6ce5515fc7e6a611638b87512`;
  manifesto `37c96d54c4e884161fd25c71350af71f6a69aca73e3f5af1217bdbf1ede71cc4`.
- PFD: pacchetto dual-role `pfdrec-a0be9a5-main-v1` presente in
  `/home/efis/efis-release-pfdrec-a0be9a5-stage/`, non nel puntatore current.
  Build host `pi5` aarch64, verifica dipendenze e dei due manifesti (candidata
  e rollback attivo) PASS sul vero PFD. Non è una build eseguita sul PFD.
- SHA-256 archivio PFD
  `3f02772bc623dc63b1bbf3e66b138ba6fa4ad5015a845dc2ccc996857d78f5c7`;
  manifesto `8e3d3a9a929f27261eb70805bf437d4a0588890c16b3ca142ba3b8cbfb64ef26`.
  Il recorder ha lo stesso hash della versione attiva; non è stato riavviato.
- PFD attivo: PID 1495, NRestarts 1 storico; recorder PID 1419, NRestarts 0.
  Nessun riavvio causato da questa sessione; get_throttled 0x0, root 71%/17G
  liberi dopo il trasferimento. Retention e configurazione non cambiate.
- Serverha: viewer e timer refresh/replica attivi; endpoint backup PASS,
  checked_utc 20261018T201511Z, main a0be9a5, restore PASS, no_change=true,
  preexisting_nas_refs_preserved=true. È una riconferma del report esistente,
  NON un nuovo restore completo eseguito oggi.

## EMS completato

Build nativa sul RPi4 `.36`, GCC 14.2/Qt 6.8.2, CMake Unix Makefiles perché
Ninja non è installato. Nessun pacchetto installato. CTest 7/7 PASS: motore,
runtime, store, EDP, presentazione link, presentazione stati e matrice responsive.
Quest'ultima è un test software di 60,45 s, non prova del pannello fisico.

Candidata `/home/pi/efis-next-releases/ems-a0be9a5-main-v1`, manifesto
`9bd369facf5a348ec83d9dd6f9eaa388f7dc90f9b839861a62cd67528095e625`,
binario `8256045bf8707a7c269562799dafdb79f6ab4bee21341946aeafcf0dcfce33f6`.
Manifesti candidata/rollback e dipendenze PASS. Il primo helper si ferma
all'import Python, dopo tutti i test PASS; log preservato. Completion con
PYTHONPATH esplicito PASS, riconfermato da una seconda validazione read-only.
Un test finale del wrapper SSH era mal quotato in PowerShell e dava exit 1:
non era parte della completion, non è stato usato per attestare il PASS.

Smoke separato PASS: QML caricato, catalogo 3 profili/447 campi, EDP non
configurato e SIM esplicito; uscita 0 con cattura 1024x600. XDG/settings,
runtime, cache e file di prova isolati sotto staging, nessun accesso al DRM
master o all'EDP operativo. Software backend NON prova gli indicatori nativi
o la rasterizzazione GPU. PNG SHA-256
`786a2a8eb2e2493d844532b1d7615af91da3ab8687a24e204d8cee50498cf004`.
PID operativo 949/NRestarts 0 invariati; nessuna attivazione, sudo o limite
motore reale approvato. Ultimo controllo alimentazione 0x0; massima temperatura
osservata nei controlli della sessione 68,1 °C, non una misura continua del picco.

## Gate per l'attivazione

MFD e Blackbox 0x50000 (storico), EMS sudo richiede password; `.89` SSH timeout
e inventario G1 aperto. Nessun power-cycle per azzerare i bit. Il piano
impone canary MFD prima del PFD, poi acceptance visuale/reboot/rollback per
ogni nuova release. Stage non equivale ad attivazione o package sigillato.

Errore helper PFD conservato in `pfd-package-initial.log`: chiamata diretta
Python senza import root `tools`. `complete-package.sh` fornisce PYTHONPATH
esplicito, completa manifest e archivio, senza toccare file della release
precedente. Il fallimento non era un errore del renderer.

## Ripresa interattiva, nell'ordine

1. Qualificare alimentazione/cavo MFD e Blackbox con prova sotto carico;
   solo dopo riconfermare il gate, senza azzerarlo artificialmente.
2. Preparare l'attivazione MFD fissata al nuovo commit e rollback osservato;
   sigillare root-owned, provare switch/rollback e reboot con acceptance.
3. Attivare PFD+recorder solo dopo canary PASS, preservando runtime e retention.
4. Finestra sudo EMS: attivazione transazionale della candidata e acceptance
   sul pannello reale; smoke offscreen non prova le geometrie degli indicatori.
5. Inventario G1, seriali/manuali 912 UL/ULS, mapping e calibrazioni approvati;
   niente adapter sensori operativo o limiti di volo dedotti dai dati SIM.

Nessun reboot, restart servizio, modifica current, rete, Pixhawk, bus o
potatura. DEVELOPMENT/NON-CERT; ogni manifesto candidato mantiene SIM e
flight_approval=false. Le prove non qualificano il sistema al volo.
