# SIL P03 — ammissione comandi di test

2026-10-27, R1-26. DEVELOPMENT / NON-CERT, componente software isolato.

`tools/simulation/command_queue.py` implementa la sola ammissione e consegna
una tantum. Non è un server autenticato, un esecutore FDM o un collegamento
al Pixhawk. Nessun ingresso di rete, file di credenziali o endpoint aggiunto.
Il chiamante fidato deve autenticare il principal PRIMA della chiamata:
`authenticated_principal` non è una password né un token verificato qui.

## Confini implementati

- Manifesto copiato e congelato per istanza/sessione/epoch; JSON strict P01.
- Solo thread owner; nessun callback nel producer o lock bloccante di rete.
- Principal e lease fissati all'avvio; lease monotona massima 5 secondi e
  scadenza tick indipendenti. Nessun rinnovo implicito; anche a fisica ferma
  la successiva chiamata osserva la scadenza monotona e svuota i pending.
  Non esiste un timer autonomo: l'integrazione dovrà chiamare il controllo
  durante pausa. Nessun comando può uscire dalla coda senza questo controllo.
- Orizzonte e range dal contratto P01 invariato; massimo 16 pending,
  256 identità e 512 KiB di payload canonici complessivi nella storia.
- Storia non espulsa: esaurimento di identità/byte revoca l'intera coda,
  cancella i pending e fallisce. Un pieno pending respinge il nuovo comando
  con INVALID, motivo già presente nell'ICD congelato; nessuna nuova enum.
- Sequenza crescente per nuove ammissioni; stesso ID/contenuto restituisce
  l'ultimo ACK memorizzato senza riaccodare. Contenuto diverso con stesso ID
  restituisce DUPLICATE_CONFLICT. Anche i rifiuti validi occupano la storia.
- `take_due` ordina per apply_tick/sequence e restituisce bytes immutabili
  una sola volta, entro expires_tick e lease. Expiry/revoca aggiornano l'ACK
  dei comandi ancora pending. Clock/tick all'indietro revocano i pending.
- Revoca indipendente dalla capacità della coda; nessun auto-restart.

QUEUED è un ACK storico di ammissione, NON applicazione o durabilità. Dopo
dispatch resta QUEUED: questa classe non emette mai APPLIED. Il retry non
costituisce una nuova esecuzione. Stato e dedup sono solo in memoria: niente
recovery dopo crash, nuova epoch o riavvio. Non ricreare la coda nella stessa
run come scorciatoia per azzerare la storia.

## Integrazione ancora aperta

P03 resta PARTIAL. Servono ingressi autenticati con backpressure verso
l'owner (questa classe NON è una inbox multi-thread), capability del modello,
esecutore tick/lifecycle, controlli effettivi JSBSim, audit intent/result e
ACK APPLIED correlato alla fisica. START/PAUSE/RESUME hanno semantica di clock
da definire prima del collegamento: i comandi schedulati non possono far
avanzare da soli un clock fermo. Kill/restore di sicurezza resta indipendente.
L'audit dei comandi richiederà anche l'estensione esplicita del trace verifier;
nessun evento di comando è oggi accettato dal runner R1-25.

Nessuna nuova prova di display o avionica reale. Il recorder operativo,
wire DDP/EDP, chiavi R1-20 e release dei nodi rimangono invariati.
