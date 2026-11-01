# R1-36 — contratto candidato e preflight multi-lease SIL

2026-11-01. DEVELOPMENT / NON-CERT. **PLAN ONLY / NOT ARMED.**
Questo componente non implementa riarmo, rinnovo o attivazione runtime.
È il prerequisito verificabile prima di introdurre un nuovo audit multi-lease.
Non modifica SessionAdmission, CommandQueue, executor, console o wire attuali.
Nessun servizio/porta/chiave, Pixhawk, attuatore o deployment autorizzato.

## Perché un contratto separato

Il controllo attuale è legato a una lease e all'epoch iniziale. Pause/stop/errori
lo revocano in modo permanente; il sidecar conserva un solo BOUND/RELEASED.
Sostituire la coda dopo resume perderebbe dedup e renderebbe incompleto l'audit.
La modalità nuova richiederà una policy diversa e un archivio multi-lease,
non un fallback permissivo nella V1 congelata. Le run precedenti restano tali.

`tools.simulation.rearm_preflight` è una funzione pura e CLI offline: non
importa engine/FDM per istanziarli, non legge clock o device, non crea lease,
file, socket, reservation o storia di dedup. Una validazione positiva resta
`PASS_PLAN_ONLY`, `execution=NOT_ARMED`. Chiamarla due volte non consuma un
piano e non costituisce un doppio riarmo: nessun riarmo viene eseguito.

## Tre oggetti bounded

- POLICY: sessione/principal fidato previsto, source commit/model hash e
  catalogo di 2..8 lease uniche. Epoch contigue da0, wall limit1..5e9 ns e
  tick limit1..500 per lease. Modalità FROZEN_LEASE_CATALOGUE_PLAN_ONLY.
- SNAPSHOT: stato PAUSED, epoch/revisione, lease corrente e prefisso delle
  lease consumate, vecchia autorità revocata e audit chiuso, hash audit,
  pending/dispatched/uncertain, elenco globale ID/byte e hash della storia,
  identità sessione/principal/config/modello/source e confine SIM esplicito.
- PLAN_REARM: ID del piano e della ripresa distinti/nuovi, sessione ed epoch/
  revisione attese, vecchia/prossima lease, hash policy/config/storia attesi e
  conferma booleana esplicita. Nessun principal fornito dalla richiesta.

Lo snapshot è **un'asserzione del chiamante**, non una cattura autenticata o
un'attestazione. Hash audit/storia sono opachi qui: non si pretende di aver
verificato i relativi byte. Anche la conferma è un dato di test e non dimostra
un gesto umano. Integrazione futura dovrà acquisire/validare sul thread owner
lo snapshot reale, senza accettarlo da un client, e correlare tutto all'audit.
L'esito non autorizza un processo che si limiti a presentare questi JSON.

## Gate e piano prodotto

Rifiuto senza piano con stato diverso da PAUSED, autorità/audit ancora aperti,
comandi pendenti/dispatchati/incerti, sessione/principal/source/modello/config
discordanti, revisione/storia/policy/epoch cambiate, catena lease non contigua,
catalogo esaurito, lease già usata o saltata, ID vecchi/duplicati e capacità
globale insufficiente (256 ID/512 KiB). Tipi/campi/schema non validi sono errori.
Non esiste recupero silenzioso, clamp, eviction o rinnovo della lease precedente.

Il candidato include snapshot/policy/request digest, audit chiuso, prossima
lease catalogata, richiesta RESUME identificata e due ID/byte aggiuntivi da
riservare. La sua epoch è quella successiva; il piano non cambia l'epoch.
Il riferimento del piano è SHA256 canonico, non una firma o credenziale.

Ogni consumer futuro deve ricontrollare sul thread owner revisione, epoch,
config, storia, audit, revoca e stato immediatamente prima di riservare e
attivare. Un piano può diventare obsoleto appena dopo il preflight. Oggi non
c'è alcun consumer/consumo atomico né garanzia contro TOCTOU/crash: volutamente
non viene offerta un'API `activate`. La lease wall non parte durante il piano.

## Uso per sviluppo, non per riarmare la console

```sh
python3 -B -m tools.simulation.rearm_preflight \
  --policy policy.json --snapshot snapshot.json --request request.json
```

Tre file JSON regolari, non symlink, massimo64 KiB ciascuno. Parser strict,
no duplicati/nonfinite/UTF-8 invalido. Nessun file di output creato.
Exit0: PASS_PLAN_ONLY; exit3: BLOCKED con motivi; exit2: input invalido/I/O.
Vettore di riferimento puramente inventato e non attivabile in
tests/data/protocols/sil-rearm-preflight-v1-vectors.json.

## Prossima integrazione necessaria

1. Nuovo contratto di audit che rappresenti ogni lease/epoch, ammissioni,
   timestamp wall, dedup globale e link al precedente rilascio.
2. Verificatore e archivio esplicitamente versionati, incluse coppie mancanti,
   riordino/replay, lease sovrapposte, stop/errori e corruzione con hash rifatti.
3. Preparazione/commit owner atomici con reservation degli ID e ricontrolli,
   poi attivazione esplicita dopo RESUME identificato; mai auto-rinnovo.
4. Prove real-clock JSBSim, revoca durante I/O/step e recovery che non riesegua
   esiti incerti. Solo dopo esporre il comando nella console locale.

Il riarmo runtime resta disabilitato, P03/M2 PARTIAL. Test di pianificazione
non chiudono sicurezza, hardware, timing, modello, fanout o acceptance pilota.

## Verifica del candidato

16 nuovi metodi,280 test locali e16 suite CTest Python PASS. Serverha296
test PASS inclusi16 JSBSim nativi: il nuovo caso usa FDM reale con clock e
journal fixture e verifica assenza di mutazioni del modello,della epoch,
della coda revocata e dell'audit chiuso. Non è prova di riarmo o timing.
Sourcebc8f968; evidenza completa e hash log canonici Git LF in
docs/status/evidence/r1-36-rearm-preflight-20261101/verification.json.
Container senza rete e con source/runtime read-only; nessun deploy o nuovo
test hardware avionico. I gate Windows/power e WinError4551 restano aperti.
