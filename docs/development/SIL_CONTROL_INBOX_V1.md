# R1-30 — inbox SIL e risultati conservati

Componente di sviluppo, non trasporto autenticato o console live.
`ControlInbox` riceve soltanto bytes immutabili e principal già fidato.
16 messaggi/256 KiB, massimo 64 KiB per messaggio e quattro ammissioni per
servizio owner; lock FIFO acquisito senza attesa. FULL/BUSY sono diagnostiche
locali, non estensioni agli enum ICD congelati. I produttori non accedono
alla fisica o alla coda di esecuzione owner.

Una latch di revoca separata dalla FIFO viene controllata prima del servizio
e prima della mutazione FDM. Non è un interrupt atomico o hard real-time.
`service` chiama `maintain` anche senza avanzare tick, ma **il chiamante deve
servirlo durante la pausa**: SessionEngine non è ancora collegato all'inbox.
Non esiste watchdog autonomo dell'inbox e il numero di messaggi non prova
un limite temporale di validazione o la deadline di un intero ciclo.

`CommandQueue` conta pending+dispatched nel limite 16; la consegna una tantum
non è APPLIED. Dopo autorizzazione immediatamente prima della mutazione,
readback e audit, l'executor fidato può conservare un ACK terminale. La
fixture locale rilegge la lease dopo INTENT e conserva APPLIED dopo l'audit.
Non confondere append accettata da writer async con fsync confermato.
Se esecuzione/readback/audit diventano incerti deve fallire la sessione.

La storia resta limitata a 256 identità/512 KiB, senza eviction. Retry
identici restituiscono l'ACK conservato, non rieseguono. Dopo chiusura
dell'ingresso, `lookup_outcome` owner-only consente la sola consultazione
limitata con principal fidato, senza ammissione né allocazione. Il futuro
trasporto deve offrire questo percorso anche dopo revoca; oggi non esiste
un endpoint. Nessuna mutazione degli ACK terminali APPLIED alla scadenza.

La review indipendente ha identificato e fatto correggere lookup dopo
chiusura e tick monotono dopo un outcome, oltre al recheck dopo INTENT.
Test: contesa, revoca con FIFO piena, pausa inattiva, retry, dispatch scaduto,
monotonia, journal lento e assenza di scrittura FDM dopo lease scaduta.

Follow-up R1-31: `SIL_OWNER_SERVICE_V1.md` collega il servizio al pump owner
RUNNING/PAUSED, con revoca permanente alle transizioni e nessun executor.
La descrizione sopra fotografa il componente R1-30 isolato.

Restano aperti: autenticazione trasporto,
policy lifecycle/resume e dedup tra epoch, audit interattivo, executor live,
durabilità confermata degli esiti e fanout multi-panel. Non allargare lease
o endpoint esistenti e non collegare questo ingresso ad attuatori reali.
