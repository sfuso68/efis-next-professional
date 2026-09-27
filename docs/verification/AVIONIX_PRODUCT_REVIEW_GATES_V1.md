# AVIONIX NEXT — Product Review Gates V1

**Stato:** `PREPARED_SW_NOT_RUN`

**Data:** 2026-09-27

**Perimetro:** `EMS-004`, `PROD-001`, `FPV-002`, `VOI-003`, `VER-002`

## Obiettivo

Separare la raccolta delle evidenze dalla decisione tecnica. I cinque intake
esistenti descrivono cosa deve essere acquisito; questi gate definiscono chi
deve revisionarlo, quali finding devono essere chiusi e quali capacità restano
vietate fino a una decisione successiva, esplicita e tracciabile.

## Cinque gate

1. **Engine source review:** seriale/applicabilità, revisione manuale, pagine
   dei limiti, separazione UL/ULS e disposizione dei limiti legacy.
2. **SKU/release review:** allowlist capability, allocazione nodi, unica
   codebase, rifiuto feature ignote e blocco del cambio profilo in volo.
3. **FPV benchmark review:** quattro feed, latenza/drop, margine termico,
   recovery, isolamento VIDEO/FLIGHT-DATA e sincronismo sidecar.
4. **Voice/cabin review:** autorità delle fonti, integrità clip, provenienza
   TTS offline, intelligibilità, isolamento PTT e priorità CAS.
5. **HIL execution review:** fixture isolata, digest input, replay
   deterministico, contenimento fault, esiti attesi e artefatti completi.

Ogni gate è legato tramite SHA-256 all'intake corrispondente. Un cambio
dell'intake invalida il gate e richiede una revisione esplicita del digest.

## Verifica software

- validatore dei cinque gate: `PASS`;
- 9 test di mutazione: `PASS`;
- path traversal, digest drift, reviewer/evidence prematuri: rifiutati;
- attivazione limiti, deployment SKU, bridge VIDEO, trasmissione voce e uso
  HIL su produzione/attuatori: rifiutati.

## Limiti dell'evidenza

Questa verifica prova soltanto la coerenza fail-closed dei file. Nessun
documento motore, profilo SKU, feed video, dispositivo audio o scenario HIL è
stato acquisito o eseguito. Tutti i gate conservano:

- `PREPARED_SW_NOT_RUN`;
- `evidence_bundle_sha256=UNRESOLVED`;
- lista reviewer vuota;
- `hardware_verified=false`;
- `flight_approval=false`;
- decisione `NO_GO`.

Il passaggio a una decisione operativa richiede un bundle immutabile, firme
dei ruoli previsti e una modifica separata revisionata. Questi template non
autorizzano test hardware, deployment o uso in volo.
