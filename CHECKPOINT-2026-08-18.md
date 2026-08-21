# EFIS NEXT — Checkpoint operativo

Data: 18 agosto 2026

## Stato raggiunto

Il lavoro prosegue esclusivamente nel nuovo progetto `efis-next`. I progetti EFIS precedenti restano invariati e utilizzabili.

Completati e verificati sul Raspberry Pi 5:

- Unified Setup Center richiamabile con pressione prolungata del logo EFIS NEXT.
- Catalogo normalizzato di aeromobili e motori.
- Selezione e staging del profilo aeromobile.
- Revisione di pesi, carburante, motore associato e V-speed.
- Verifica obbligatoria tramite riferimento POH/AFM.
- Attivazione atomica del profilo con backup, audit e rollback.
- Selezione e staging del profilo motore.
- Inserimento e validazione dei limiti RPM, olio, CHT, EGT e carburante.
- Mappatura dei sensori EMS per sorgente, nodo e canale.
- Controllo di canali mancanti e duplicati.
- Banco prova del singolo canale con valore grezzo, convertito, frequenza e sample age.
- Calibrazione temporanea gain/offset.
- Iniezione e cancellazione del timeout.
- Modalità `BENCH_SIM_ACTIVE` separata dai provider hardware reali.

## Verifica piattaforma

- Target: Raspberry Pi 5, Debian 13 aarch64, Qt 6.8.2.
- Risoluzione verificata: 1024 × 600 touchscreen.
- Compilazione ARM completata.
- Test automatici superati al 100%.
- Servizio `efis-pfd.service` attivo al termine delle prove.
- Profilo aeromobile operativo non alterato durante le anteprime.

## Ultime schermate di riferimento

- `output/rpi5-captures/setup-aircraft-verification-v1.png`
- `output/rpi5-captures/setup-engine-limits-v1.png`
- `output/rpi5-captures/setup-engine-sensor-map-v1.png`
- `output/rpi5-captures/setup-engine-channel-test-v1.png`

## Punto esatto di ripartenza

Implementare il provider hardware normalizzato tra ESP32-S3, CAN e Raspberry Pi.

Ordine previsto:

1. Definire il messaggio sensore comune: signal ID, node ID, channel, raw value, engineering value, unit, monotonic timestamp, sequence, validity e quality.
2. Definire heartbeat e stato dei nodi.
3. Implementare timeout, stale data e perdita nodo.
4. Creare adapter ESP32-S3 seriale/CAN e adapter CAN Raspberry.
5. Collegare i provider reali al banco prova già creato.
6. Implementare priorità e failover tra sensore primario e secondario.
7. Registrare telemetria, cambi sorgente e guasti nella black box.

## Vincoli da mantenere

- Nessun valore indicativo può diventare attivo senza verifica POH/AFM o manuale motore.
- Nessuna assegnazione hardware automatica.
- Dati simulati sempre identificati come simulazione.
- Scritture di configurazione atomiche, con backup e audit.
- Setup disponibile solo a terra.
- Una sola codebase EFIS NEXT con profili Standard e Professional.
- Conservare FPV, telemetria, black box e integrazione futura Pixhawk/ArduPilot.

## Stato repository

Non sono stati creati commit o pull request e non è stato modificato `main`.

## Aggiornamento Copernicus — 19 agosto 2026

- Download e audit completati per 12/12 celle Veneto del dataset `COP-DEM_GLO-30-DGED/2024_1`.
- Pacchetto attivo sul Raspberry Pi: `cop-dem-glo30-veneto-2024_1`, EGM2008, `DEVELOPMENT` / `NON-CERT`.
- Checksum, 17 bordi condivisi e quota campione LIPH verificati; differenza massima ai bordi 0 m.
- Corretto il fallback del renderer per impedire la sostituzione silenziosa con tile Mapzen quando il pacchetto configurato è incompleto.
- Cache fredda, tile indisponibile e prestazioni Wayland verificate; regime osservato circa 32–33 fps.
- Servizio utente `efis-pfd.service` attivo e test automatici superati.
- Prossimo gate terreno: attraversamento dinamico reale tra celle, punti quota indipendenti aggiuntivi e prova termica prolungata.
