# Checkpoint workflow profilo operativo — 2026-09-09

## Scopo

Rendere più semplice il caricamento e la compilazione del profilo aeromobile
senza indebolire il confine tra simulazione, bozza e dati approvati.

Branch: `work/avionics-ethernet-v1`

ID di controllo: `CFG-001`, `PFD-001`, `EMS-002`.

## Implementato

- Quattro pagine condivise da PFD ed EMS: Overview, Guided Entry,
  Load/Prefill, Check & Review.
- Stato di avanzamento per tutti i 28 domini e coda problemi ordinata per
  criticità.
- Navigazione diretta al primo problema e salvataggio del singolo campo con
  avanzamento guidato.
- Importazione fail-closed di bozze V2 compatibili, con limite di 4 MiB e
  verifica di schema, profilo, revisione, campi, tipi, unità e metadati; un
  profilo radice non può importare una disposizione `INHERIT` priva di padre.
- Prefill deliberato dei 447 valori analoghi: ogni valore resta marcato SIM e
  lo stato rimane `BLOCKED_SIMULATION`.
- Verifica di completezza e provenienza separata da qualunque attivazione.

## Verificato solo software

- Build MSVC/Qt 6.11 di PFD, EMS e test modello completata.
- Compilazione QML AOT della pagina condivisa completata nei due target.
- Test modello avviato direttamente: PASS. Copre 28 domini/447 campi, coda
  problemi, prefill SIM completo, importazione valida e rigetto di
  `activation_allowed=true` e di `INHERIT` su un profilo radice.
- Suite catalogo Python: 12/12 PASS.
- Smoke/cattura offscreen delle pagine `Load / Prefill` e `Check & Review` a
  1024x600: caricamento e geometria senza sovrapposizioni evidenti. Il backend
  headless Windows non ha caricato il font e ha mostrato glifi sostitutivi;
  pertanto la prova non costituisce acceptance di leggibilità.
- Il launcher CTest Windows resta in timeout sullo stesso eseguibile; questa
  anomalia non viene confusa con il PASS dell'esecuzione diretta.

## Non verificato

- Nessuna installazione o prova touch su RPi5/PFD o RPi4/EMS.
- Nessuna acceptance visiva a 1024x600 o 800x480.
- Nessuna validazione di documenti dell'aeromobile reale.
- Nessun workflow di firma, review a quattro occhi, rilascio o attivazione.

## Punto di ripresa

1. Installare una release separata e reversibile prima sul PFD.
2. Verificare Overview, scorrimento domini, tastiera e coda problemi a
   1024x600.
3. Ripetere sul display EMS RPi4, con particolare attenzione a 800x480.
4. Acquisire catture e tempi di risposta con bozza vuota e con 447 campi SIM.
5. Solo dopo l'acceptance progettare review, firma e release immutabile.
