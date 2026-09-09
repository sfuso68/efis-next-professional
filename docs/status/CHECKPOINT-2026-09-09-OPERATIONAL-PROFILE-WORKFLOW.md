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

## Evidenza deployment PFD

- Il commit `b29fc03` è stato trasferito nella release separata
  `/home/efis/efis-next-avionics-b29fc03` sul PFD reale `.45`, compilato ARM64
  con tutti i 302 step e verificato con 17/17 CTest PASS.
- Il primo switch non ha eseguito il binario perché `logs/` non era presente
  nel pacchetto prodotto da `git archive`; il rollback automatico a `12b4697`
  è riuscito. `logs/.gitkeep` e il runbook ora rendono esplicito il requisito.
- Il secondo switch ha avviato il nuovo PFD con DDP `LIVE`, ma il log runtime
  ha rilevato due errori QML: `FieldBox` esponeva `value` mentre l'editor usava
  `text`. La release è stata nuovamente riportata a `12b4697`, attiva e LIVE.
- La correzione dell'alias e la compressione verticale dell'editor hanno
  superato rebuild, smoke offscreen senza errori QML e controllo visivo del
  layout software.
- La correzione è stata pubblicata come `74a85f9`, compilata da una nuova
  release ARM64 con 302/302 step e 17/17 CTest PASS e attivata sul PFD. Il
  runtime ha caricato il catalogo, imagery `OFFLINE_READY` e DDP `LIVE` senza
  errori QML. In 45 secondi gli snapshot accettati sono saliti da 1.817 a
  2.718 con zero reject, gap o errori socket; 24,2 fps finali, 52,7 °C e
  `throttled=0x0`.
- Una cattura Wayland reale 1024x600 conferma il renderer PFD completo della
  nuova release. Le quattro pagine non sono state aperte nella cattura: touch,
  tastiera e leggibilità del workflow restano quindi non verificati.

## Non verificato

- La correzione è installata e stabile sul PFD, ma non ancora provata via touch;
  nessuna installazione della stessa revisione è stata eseguita sul RPi4 EMS.
- Nessuna acceptance visiva a 1024x600 o 800x480.
- Nessuna validazione di documenti dell'aeromobile reale.
- Nessun workflow di firma, review a quattro occhi, rilascio o attivazione.

## Punto di ripresa

1. Verificare sul PFD Overview, scorrimento domini, tastiera e coda problemi a
   1024x600.
2. Solo dopo questo gate installare la stessa revisione sul display EMS RPi4,
   con particolare attenzione a 800x480.
3. Acquisire catture e tempi di risposta con bozza vuota e con 447 campi SIM.
4. Solo dopo l'acceptance progettare review, firma e release immutabile.

## Pausa operatore — 2026-09-09

Il proprietario ha chiesto di fissare qui il punto e verificherà il workflow al
proprio rientro. La release PFD `74a85f9` resta installata e stabile; la prova
touch delle quattro pagine resta **PENDING OPERATOR ACCEPTANCE**. Non si porta
il workflow sull'EMS RPi4 e non lo si dichiara accettato prima di tale verifica.
