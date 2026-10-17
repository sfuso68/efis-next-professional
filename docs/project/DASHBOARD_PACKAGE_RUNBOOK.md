# Pacchetto monitor/backup

Pacchetto senza segreti: nessuna chiave privata, configurazione personale,
repository o registrazione. Richiede Python standard library e Git; Bash e
systemd user solo per l'attivazione dei servizi. Destinazione operativa
attuale: utente sfuso su serverha. Non è un installer per i nodi avionici.

Dal root del repository:

```sh
python -B -m tools.project_status.package_dashboard build output/dashboard-package-v1
python -B -m tools.project_status.package_dashboard verify output/dashboard-package-v1
python -B -m tools.project_status.package_dashboard install output/dashboard-package-v1 --releases /home/sfuso/.local/lib/avionix-dashboard-releases
```

Build rifiuta una destinazione già presente; per confrontare due build usare
due cartelle. Da dentro il pacchetto usare sempre `python -B` per non creare
`__pycache__` che il verificatore rifiuta come contenuto extra. Il primo staging
senza -B è stato rifiutato correttamente, senza installare file.
Per confrontare due build usare
due cartelle. Le build normalizzano LF e producono manifest deterministico.
Installazione versionata verificata prima della rename; reinstallazione
identica idempotente, corruzione o contenuto extra rifiutati. SHA-256 prova
integrità, non autenticità: confrontare digest su canale fidato.

Il comando install NON cambia la release attiva, le unit systemd, i timer o
le chiavi. Le versioni precedenti rimangono disponibili. L'attivazione del
layout fisso `/home/sfuso/.local/lib/avionix-dashboard` resta separata: copiare
solo dopo verifica, mantenere la versione precedente e riavviare il solo
monitor. Il legacy `install_dashboard.sh` configura i tre servizi monitor
nel layout fisso; NON configura chiavi, ingest Git o wrapper backup.

## Attivazione transazionale (aggiunta 2026-10-16)

La versione V1 del runbook separava staging e attivazione. Ora il pacchetto
include `activate_dashboard.py`: configura drop-in delle tre unit monitor,
refresh e backup; ferma temporaneamente timer e job oneshot, effettua reload,
refresh/backup, restart monitor e health HTTP con identità release attesa.
Le singole scritture drop-in sono atomiche; la suite è transazionale, non
un cambio simultaneo di tutti i processi. Nessuna sostituzione del vecchio
layout fisso o di chiavi private. Le release restano conservate.

```sh
python3 -B -m tools.project_status.activate_dashboard /home/sfuso/.local/lib/avionix-dashboard-releases/PACKAGE_ID --rollback-drill
python3 -B -m tools.project_status.activate_dashboard /home/sfuso/.local/lib/avionix-dashboard-releases/PACKAGE_ID
```

Eseguire da una copia verificata del pacchetto. Prerequisiti: utente sfuso,
servizi/timer già installati, chiavi già autorizzate, rete funzionante.
Manifest verificato PRIMA di cambiare le unit. Stato precedente salvato in
`.local/state/avionix-dashboard/activations`; lock esclusivo impedisce due
attivazioni contemporanee. Errori di avvio/health attivano ripristino dei
drop-in precedenti, riavvio, controllo servizi e health release precedente.
Il drill ritorna successo SOLO se raggiunge il guasto intenzionale e il
rollback è verificato; altri errori non sono mascherati come drill PASS.

Non protegge da SIGKILL/power-loss durante la transazione; reboot acceptance
e disaster recovery dei servizi sono gate separati ancora aperti.

Verifica meteo offline:

```sh
python -m unittest discover -s server/edge-node -p test_edge_weather.py -v
```

Nessuna rete/provider richiesti. Qt radar è un gate distinto: necessita build
del target `efis_weather_service_tests` e ambiente Qt funzionante. Un vecchio
binario non certifica il codice corrente.
