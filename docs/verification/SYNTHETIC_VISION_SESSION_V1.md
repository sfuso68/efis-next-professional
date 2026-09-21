# AVIONIX NEXT — Synthetic Vision session manager V1

Stato: `PREPARED_SW_NOT_RUN`  
Attività: `AVX-G3-01`, `SV-001`, `PFD-001`, `MFD-001`  
Prodotto: DEVELOPMENT / NON-CERT / `flight_approval=false`

## Scopo

Il manager C10 trasforma il contratto C9 in una sessione ripetibile per i due
RPi5. Non avvia il renderer, non svuota cache, non sposta tile e non inietta
fault. Prepara invece una directory chiusa, separata per PFD e MFD, che può
essere compilata durante una finestra hardware autorizzata.

Il comando è `tools/terrain/manage_synthetic_vision_session.py`.

## Preparazione

Usare il commit completo realmente installato sui nodi:

```text
python3 tools/terrain/manage_synthetic_vision_session.py init \
  --output /percorso/evidence/sv-session-UTC \
  --source-commit <40-caratteri-esadecimali>
```

Il comando:

- rifiuta directory già esistenti;
- valida il contratto C9 canonico;
- congela copia e SHA-256 del contratto;
- crea `pfd/results.json` e `mfd/results.json` in stato `NOT_RUN`;
- crea due radici artefatti separate;
- mantiene `flight_approval=false`.

La preparazione non è evidenza hardware e non va interpretata come PASS.

## Acquisizione autorizzata

Per ciascun ruolo l'operatore deve compilare il relativo `results.json` con:

- release realmente osservata e stesso commit della sessione;
- almeno 60 minuti, DAY e NIGHT;
- metriche FPS/frame-time, termica, throttling e bordo tile;
- tutti i cinque checkpoint;
- fault missing-tile e fallback esplicito;
- sei famiglie di artefatti reali con digest SHA-256;
- stesso nome operatore usato nella finalizzazione.

Gli artefatti di un ruolo devono restare nella sua directory `artifacts/`.
Percorsi assoluti, traversal, symlink, file vuoti e digest falsi sono respinti.

## Finalizzazione

Solo dopo avere completato entrambi i ruoli:

```text
python3 tools/terrain/manage_synthetic_vision_session.py finalize \
  --bundle /percorso/evidence/sv-session-UTC \
  --operator "Nome Cognome" \
  --confirmation I_CONFIRM_REAL_SYNTHETIC_VISION_OBSERVATION
```

La finalizzazione richiama il validatore C9 per PFD e MFD, richiede identità
operatore coincidente e produce `evidence.json` una sola volta. Il manifesto
lega contratto, commit, release e file risultati mediante SHA-256. Non concede
approvazione al volo.

Il bundle può essere ricontrollato indipendentemente in qualsiasi momento:

```text
python3 tools/terrain/manage_synthetic_vision_session.py verify \
  --bundle /percorso/evidence/sv-session-UTC
```

La verifica ricalcola i digest e riesegue C9 sui due risultati; una modifica
successiva alla finalizzazione invalida il bundle.

## Condizioni fail-closed

La sessione non può finalizzarsi se manca anche una sola osservazione, se una
soglia C9 non è rispettata, se i due ruoli non sono entrambi presenti, se
commit/contratto/operatore divergono o se un artefatto non è confinato e
integro. Una sessione fallita resta ispezionabile e non viene convertita in
PASS.

## Limite

I test automatici completano sessioni sintetiche soltanto per provare il
manager. Nessun RPi5 o pannello è stato contattato in C10. `AVX-G3-01` resta
`OPEN`; `SV-001`, `PFD-001` e `MFD-001` restano `PARTIAL`.
