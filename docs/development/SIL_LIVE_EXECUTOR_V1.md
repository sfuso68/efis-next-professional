# R1-32 — executor locale SIL e audit degli ingressi

2026-11-01. DEVELOPMENT / NON-CERT / NO-ENDPOINTS / NO-ACTUATORS.
Opt-in di libreria, NON console di pilotaggio pronta o nuova release sui nodi.
La catena reale Pixhawk rimane read-only (ADR-003/012); questo codice opera
soltanto sul backend FDM iniettato dal banco di test fidato.

## API e autorità

Prima del bind in CREATED, l'owner può chiamare
`SessionAdmission.enable_execution(backend, audit_path)`. Il successivo bind
verifica identità del backend e assenza di una fixture pilota concorrente.
Restano una sola lease e la revoca permanente alle transizioni R1-31.
Default admission-only invariato. Nessun socket, chiave, endpoint o controllo
hardware è aggiunto. Gli oggetti iniettati sono fidati: non è una sandbox
contro un backend Python malevolo. Il futuro trasporto deve autenticare il
principal e limitare i file; oggi non c'è trasporto live esposto.

Sono supportati solo SET_CONTROL aileron/elevator/throttle e le tre proprietà
FDM già provate della fixture C172P. Si verifica il catalogo prima delle
scritture. Altre azioni/controlli o proprietà mancanti ricevono UNSUPPORTED.
L'intero batch viene classificato prima di mutare ingressi: al massimo un
comando per controllo per step, primo in ordine di dispatch accettato e
conflitti successivi INVALID. Nessun nuovo enum viene aggiunto all'ICD.

## Ordine dello step

1. Servizio owner, watchdog e deadline, dispatch dei comandi dovuti.
2. Classificazione del batch; audit dei rifiuti prima delle mutazioni.
3. Per ciascun comando accettato: INTENT scritto e fsync, controllo deadline/
   writer, rilettura lease/latch, poi scrittura della sola mappa override SIL.
4. Dopo l'ultimo guard writer/deadline: controllo finale di lease/latch anche
   per override già mantenuti. Un rilascio può fare I/O: dopo il rilascio si
   ricontrollano writer e deadline, con override già rimossi. Prima dello step
   resta un controllo lag senza I/O. Nessuna garanzia di interrupt atomico.
5. Si marca che la fisica può iniziare; si esegue un solo step backend.
6. Rilettura di tutte le proprietà: devono corrispondere ai valori richiesti.
   Per ciascun esito: APPLIED scritto e fsync, poi conservazione ACK terminale.
7. Solo dopo gli esiti noti si gestiscono scadenza/revoca giunte durante lo step.
   Una scadenza durante il fsync del risultato non riscrive APPLIED in REJECTED.

Il ritardo di fsync conta nel limite fisico 100 ms, non allargato. Una scrittura
sincrona non è temporalmente limitata dal solo limite di byte: un blocco kernel
resta un rischio; watchdog/endurance e isolamento processo sono lavori aperti.

## Errori e risultato incerto

Errore prima della fisica: niente esecuzione dello step; revoca e rimozione
override. Se la fisica può essere iniziata ma readback/audit non completano,
gli ID ancora irrisolti diventano internamente incerti. Lookup/retry identici
sollevano errore esplicito: non vengono emessi falsi ACK REJECTED o APPLIED
e non si riesegue. Le identità incerte condividono la history bounded senza
eviction. Gli APPLIED già confermati rimangono tali anche se fallisce un esito
successivo dello stesso batch. Il run fallisce e non pubblica completion.

Rilascio: prima rimozione della mappa override, poi audit e chiusura sidecar,
anche su errore clear. Se clear fallisce, sessione terminale e nessun RELEASED
asserito. RELEASED significa soltanto OVERRIDE_MAP_ONLY: non neutralizzazione,
non ripristino immediato di proprietà FDM durante pausa e non sicurezza in volo.
Il pilota sintetico di banco riprende sugli step successivi.

## Sidecar e replay: confine esplicito

`SyncControlAudit` crea un file esclusivo senza overwrite, header con manifesto,
authority/lease e record BOUND/INTENT/APPLIED/REJECTED/RELEASED. Massimo 1024
record, 1 MiB complessivo, 64 KiB per record. Ogni append controlla short write
e fsync; cap/errori/close falliscono il run. Il rilascio segnala ID incerti.
Il risultato riuscito include nome/hash/conteggi del sidecar chiuso.
Questo prova integrità/ordine e readback ingressi, non autenticità crittografica,
correttezza aerodinamica indipendente o resistenza a power-loss/directory fsync.

Non è ancora un contratto completo di audit ammissione né replay esecutivo.
Il risultato include `control_execution`: il verificatore trace legacy e quindi
il pack/restore R1-28 lo rifiutano esplicitamente, per non esportare una run
omettendo il sidecar. Le vecchie run senza questo campo restano supportate.
Schema replay live, config congelata, correlazione completa, firma e restore
del nuovo sidecar sono gate ancora aperti; non chiamarli PASS per induzione.

## Evidenze e residui

Test locali coprono mutation/readback, retry, conflitti, capability mancanti,
revoca dopo setter, expiry nell'ultimo watchdog, lease durante fsync esito,
release lento, errori short-write/fsync/close/clear e stato incerto. Test nativo
usa JSBSim reale con comando offerto dopo l'avvio; clock/journal principale
sono fixture, sidecar scritto su file. Non è prova timing live o pannello.

Restano console/trasporto/lifecycle autenticati, riarmo epoch con dedup run-wide,
replay/restore sidecar, fanout multi-panel, modelli/impianti completi, endurance,
power e acceptance hardware. Nessuna calibrazione o fonte motore approvata.
