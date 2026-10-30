# R1-31 — servizio di ammissione nel ciclo owner SIL

Stato: PARTIAL / DEVELOPMENT / NO-ENDPOINTS / NO-ACTUATORS.
Estende R1-30 senza introdurre un executor live. Un comando QUEUED non è
applicato: nessun `take_due` o input FDM è invocato da questo componente.

## Collegamento e confini

`SessionAdmission` possiede CommandQueue e ControlInbox e si collega mediante
`SessionEngine.attach_admission` soltanto in CREATED, una volta, sullo stesso
thread owner e con manifesto identico alla copia congelata. Niente callback
dei consumer, socket, credenziali o chiavi. I producer possono solo offrire
bytes immutabili con principal già fidato o segnalare la latch di revoca.
L'autenticazione del principal è responsabilità del futuro trasporto.

Ogni `pump` in RUNNING o PAUSED controlla il watchdog del journal, serve al
massimo quattro richieste, ricontrolla il watchdog e legge il clock. Il lavoro
di ammissione conta nel ritardo della successiva fisica: oltre 100 ms il motore
fallisce prima dello step, senza recupero in burst. In PAUSED non avanza la
fisica e non pubblica frame. La verifica writer continua anche in pausa.
Il chiamante deve continuare a chiamare `pump`: non è un watchdog indipendente
e non si promette hard real-time né un tempo massimo del parsing/OS.

`session_jsbsim.run` usa ora questo stesso pump nella propria pausa esplicita
di banco. Nessuna console o canale di rete viene aggiunto. `SessionEngine.run`
resta il loop RUNNING: per una pausa gestita esternamente il relativo owner
deve continuare a pompare; non esiste una RESUME accettata da thread esterni.

## Policy lifecycle ed epoch deliberatamente restrittiva

Pause, resume, stop, errore journal/backend/watchdog revocano la lease collegata.
La revoca precede scritture lifecycle e finalizzazione. Pump su COMPLETE o
FAILED resta no-op, senza richiamare un writer già chiuso. Non cambia la
semantica delle transizioni esistenti o il formato della trace fisica.

Alla ripresa la fisica conserva total_ticks e il frame riparte da tick relativo
zero con una nuova epoch. La vecchia coda resta chiusa e non viene reinizializzata:
niente rinnovo implicito, riuso ID o esecuzione ritardata di comandi vecchi.
La history bounded rimane interrogabile dall'owner con principal ed esatto
payload originale. Una nuova lease dopo resume non è ancora implementata;
serve definire audit e dedup run-wide prima di permettere il riarmo.

Le risposte dell'ultimo servizio sono uno snapshot bytes sovrascritto e limitato,
non una coda di output né un registro durevole. Le risposte possono essere
perse; l'owner può consultare la history. Il futuro trasporto dovrà conservare
quel percorso anche a ingresso chiuso. Le risposte di ammissione non sono
eventi di esecuzione e non vengono aggiunte alla trace come falso audit live.

## Prove e limiti

Test software: ingress da thread diverso, attach tardivo/doppio/owner errato,
manifesto mutato, lease a tick fermo, revoca prima del journal, no fisica in
pausa, reset epoch senza inversione tick della coda, ritardo ammissione,
watchdog dopo servizio, guasti arm/start/frame/backend/finalizzazione,
no-op terminale e zero dispatch anche oltre la scadenza dei comandi.
Il test nativo JSBSim usa clock/journal fixture per verificare la composizione
con FDM reale: non è misura di scheduling o durabilità in tempo reale.

Restano aperti executor live/readback/capability, lifecycle autenticato,
riarmo lease/dedup tra epoch, audit interattivo e durabilità confermata,
console, fanout e consumer simultanei. Nessun pannello, Pixhawk, CAN, I2C,
recorder operativo o policy Windows è modificato da questo lavoro.
