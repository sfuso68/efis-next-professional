# Dieci passi autonomi — R1-05, meteo e controllo CI

Data: 2026-10-21. Stato: candidato software, non distribuito e non approvato al
volo. ID: WX-001, WX-002, VER-001. Branch meteo
`fix/weather-observation-freshness-v1`, base PR #55
`14b93268218ee60f21067ecdd70ad746abfb62fb`. PR #56 è uno stack separato su #53:
questo documento non implica merge o integrazione di quei sorgenti.

| Passo | Risultato concreto | Evidenza / limite |
|---|---|---|
| 1. Audit CI | PR #55 Core e Project Control PASS; rilevato FAIL Project Control #56 | Run 39667558632/39667558505 e 39670643622 |
| 2. Correzione gate #56 | Tolto marker JS obsoleto per autonomia, mantenuta prova runtime; follow-up 306d145 | Project Control 39671281723 PASS; Core 39671281524: build, test Qt, core e sanitizer PASS ma golden FAIL, non aggirato |
| 3. Contratto provider | Verificati campi temporali AWC e cambiamento API DPC 2026 | Fonti ufficiali sotto; documentazione, non qualifica live |
| 4. Età METAR | `obsTime` governa freschezza, mai `reportTime` o download | Confini 3600/7200 s e tolleranza futura 5 s, precisione ms |
| 5. Validità TAF | Età da `issueTime` più intervallo `[validTimeFrom, validTimeTo)` | Futuro, intervallo mancante/invertito ed expiry non diventano CURRENT |
| 6. Merge e limiti | METAR e TAF indipendenti per ICAO; no sostituzione con osservazione precedente | Solo LIPH/LIPZ già richiesti, max 16 record/512 KiB, testo 8192 caratteri |
| 7. Perdita/fallimenti | Errore, JSON malformato o risposta vuota non ringiovaniscono il rapporto | Aging a lettura/notify timer; report storico conservato distinto dal testo utilizzabile |
| 8. HMI e selezione | Card con tipo/stato/età, raw scaduto non operativo, SIM esplicito; ICAO selezionato deve coincidere | 14 esiti Qt offscreen, non acceptance sul pannello |
| 9. Radar N10 | Età cache separata; `UNKNOWN TIME`, età osservazione -1 e nessun pixel reale senza identità osservazione | Legacy WMS non qualificato: non è ripristino del radar live |
| 10. Verifiche e controllo | Test nativi, due mutazioni, CTest QML, regressioni Python, build e stato/matrice/cruscotto | Evidenza versionata, CI remota del nuovo candidato ancora da verificare; nessun deploy |

## Contratto temporale implementato

Soglie **DEVELOPMENT di presentazione**, non limiti regolamentari: METAR
CURRENT fino a 60 min, STALE fino a 120 min, poi EXPIRED. Per TAF: CURRENT
fino a 6 h dall'emissione, STALE fino a 12 h, subordinatamente alla finestra
di validità; fuori finestra non è utilizzabile. Mancanza del timestamp dà
UNKNOWN TIME; oltre 5 s nel futuro CLOCK SKEW. Timestamp ISO senza fuso e
millisecondi passati per secondi vengono rifiutati.

Il modello conserva raw, osservazione/emissione, ricezione, finestra TAF e stato
separatamente. Solo CURRENT/STALE reali o SIMULATED esplicito producono testo
operativo nel nuovo riquadro; STALE non eredita il verde VFR. Nessuna autorità
su pilotaggio, navigazione di sicurezza o fraseologia operativa è attribuita.
Le richieste asincrone usano generazione modalità e numero richiesta distinto
per prodotto. Training→reale svuota il deposito dei rapporti.

## Conseguenza intenzionale per il radar

Il vecchio WMS restituisce qui un PNG senza identità di osservazione verificata.
Il codice prima trasformava ricezione/mtime in CURRENT. Il candidato non lo
fa più: conserva la cache e ne espone l'età **come cache**, ma non pubblica quei
pixel come radar qualificato. Non interpreta Last-Modified come osservazione e
non associa a un'immagine anonima il timestamp di un altro prodotto.

È aperto **R1-05b / provider radar**: selezionare l'osservazione DPC, vincolare
tile e manifest allo stesso prodotto/timestamp, validare formato WebP/GeoTIFF,
georeferenziazione, scala cromatica, copertura e metadati persistenti. Servono
fixture con provenance e poi qualifica provider/ARM64. Nessuna API live è stata
qualificata in questo blocco; WX-001/WX-002 rimangono PARTIAL.

## Evidenze e prossime azioni

Risultati puntuali in
`../status/evidence/r1-05-weather-20261021/verification.json`.
Non conteggiare 14 esiti Qt come 14 requisiti chiusi. Nessuna prova su sensori,
pannelli, CAN, Pixhawk o alimentazione; nessun riavvio o cambio servizio.

1. PR #56: revisionare le immagini Linux STALE/LOST/FAIL e approvare o correggere
   la nuova baseline con evidenza visiva. Non ampliare le tolleranze per passare
   il gate. Artefatto: run 39671281524, `ems-responsive-states-linux-failed-rerender`.
2. Candidato meteo: CI completa/review, poi integrazione con #54/#55 senza
   presumerle già unite. La presentazione radar #54 resta indipendente.
3. Provider R1-05b e successivamente R1-06 (inventario strumenti read-only,
   gate controllo progetto ed evidenze); verticale EDP reale R1-07 ancora aperto.
4. Attivazioni e prove banco rimangono subordinate ai gate power e acceptance.

## Fonti primarie consultate

- AWC: https://aviationweather.gov/data/api/ e
  https://aviationweather.gov/data/schema/openapi.yaml — METAR `obsTime` epoch
  secondi; TAF `issueTime` stringa UTC, intervallo epoch secondi.
- DPC: https://dpc-radar.readthedocs.io/it/latest/ e
  https://dpc-radar.readthedocs.io/it/latest/api.html — aggiornamento 12 gennaio
  2026, API prodotto/timestamp e file sorgente, avvertenze uso informativo.
  Il cambio di documentazione non prova da solo la disponibilità operativa di
  ciascun endpoint o la correttezza geografica di una tile.
