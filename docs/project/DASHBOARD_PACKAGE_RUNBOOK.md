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

Limite esplicito: packaging e installazione versionata ripetibili sono
realizzati; attivazione atomica con rollback automatico di tutta la suite
NON è ancora implementata. Non chiamare questo una release completa end-to-end.

Verifica meteo offline:

```sh
python -m unittest discover -s server/edge-node -p test_edge_weather.py -v
```

Nessuna rete/provider richiesti. Qt radar è un gate distinto: necessita build
del target `efis_weather_service_tests` e ambiente Qt funzionante. Un vecchio
binario non certifica il codice corrente.
