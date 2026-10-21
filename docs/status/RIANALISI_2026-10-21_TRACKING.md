# Rianalisi indipendente: trattamento dei rilievi

Fonte: RIANALISI-EFIS-2026-10-21.md fornito dal proprietario. I 63/63 CTest e
sanitizer sono esiti dichiarati dal revisore, non rieseguiti in questo lotto.
La baseline indicata dal revisore è a0be9a5; main remoto osservato all'ingresso
del fix è invece 1cc0a78682741872e19082ae84dae936370e3d7e. N1 è presente e
identico anche nella precedente baseline 4aea1e4. Nessun reset/merge/rebase o
riallineamento forzato dei riferimenti è stato eseguito.

## N1 — correzione software candidata, non distribuita

ID PIX-002. set_mavlink_parameter.py è una tombstone di quarantena: non importa
pymavlink, non crea connessioni, non effettua query o trasmissioni e non contiene
un generatore PARAM_SET. Ogni CLI legacy, inclusi --commit e --allow-protected,
termina exit 2 con refused_not_authorized; --help resta disponibile. L'API
write_parameter rifiuta prima di accedere al link passato. Non ci sono override.
Non viene dichiarato un audit persistito: il rifiuto è JSON su stdout e non
scrive il percorso eventualmente passato con --audit-dir né stampa device o
credenziali. Senza connessione non è possibile neppure la vecchia dry-run attiva.

Questo ritira intenzionalmente il tool, invece di fingere che un controllo di
disarmo da solo soddisfi ADR-015. L'ADR non cambia: interlock fisico, disarmo,
identità computer/device, audit, timeout, esclusione operativa e rollback di
un futuro canale manutentivo richiedono progetto/approvazione separati.
Non usare copie storiche del writer per aggirare la quarantena. Le vecchie
versioni restano recuperabili in Git ma non costituiscono capacità autorizzata.

Sette test comportamentali Windows PASS e sette Linux isolati PASS, senza
Pixhawk, socket o seriali. Coprono CLI con device seriale/rete e override,
chiamata diretta con oggetto tripwire, socket/file tripwire, import senza
pymavlink, help e nessuna scrittura audit. Il vecchio harness manuale esegue
ora questi test anziché pretendere scritture riuscite. Le fixture backup
storiche rimangono separate e NON sono state eseguite o qualificate qui.
CI Project Control include la nuova suite; esito remoto da leggere a parte.

Nessuna distribuzione sui nodi, rimozione delle copie remote o prova hardware.
Il ritiro nel sorgente candidato non dimostra che tutte le copie installate
siano state ritirate. Tutte le attivazioni rimangono bloccate fino a PSU review.

## Coda correttiva prima del nuovo sviluppo funzionale

| Rilievo | Stato nel presente lotto | Prossima azione |
|---|---|---|
| N1 | PARTIAL | Review/integrazione del ritiro; inventario copie remote separato |
| N4 + N5 | TODO | Contratto engineDataState comune e prova di comportamento prima del binding |
| N2 + N6 | TODO | Visibilità radar fail-closed e codifica primaria STALE distinta |
| N3 | TODO | Posizione validata, rimozione coordinate operative di default |
| N7 + N8 | TODO | Inizializzazione fuel e valori EMS subordinati a validità per canale |
| N9 + N10 | TODO | Freshness osservativa/provider distinta dal fetch, gate consumer |
| N11 | TODO | Riclassificare o ritirare i tool query che emettono COMMAND_LONG |
| N12 + N13 | TODO | Gate controllo progetto su sé stesso e prove non circolari |

TODO significa aperto nel piano, non prova che ogni dettaglio sia stato ancora
rieseguito indipendentemente. Non vengono importati esiti hardware dall'audit
né dichiarati chiusi i vecchi rilievi senza ripetere la relativa verifica.
