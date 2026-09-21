# CHECKPOINT — Synthetic Vision session manager C10

Data: 2026-09-21  
Branch: `test/synthetic-vision-session-manager-v1`  
ID: `AVX-G3-01`, `SV-001`, `PFD-001`, `MFD-001`  
Classificazione: implementazione e verifica software; hardware non eseguito

## Implementato

- inizializzazione non sovrascrivibile di una sessione C9;
- congelamento di contratto, SHA-256 e commit sorgente completo;
- template `NOT_RUN` distinti per PFD e MFD;
- radici artefatti separate e confinate per ruolo;
- caricamento fail-closed di sessione, contratto e risultati;
- finalizzazione solo con entrambi i run C9 validi;
- conferma testuale esplicita e identità operatore coincidente;
- manifesto finale non sovrascrivibile con digest dei due risultati;
- comando `verify` che ricalcola digest e riesegue C9 sui due ruoli;
- mantenimento obbligatorio di `flight_approval=false`;
- test registrato in CTest/CI.

## Verificato software

- creazione sessione completa con due ruoli;
- rifiuto di commit malformato e directory preesistente;
- rifiuto dei template non eseguiti;
- rifiuto di conferma errata e operatore divergente;
- rifiuto di commit ruolo/sessione divergente;
- rifiuto di traversal e contratto alterato;
- finalizzazione sintetica PFD+MFD e divieto di sovrascrittura;
- rifiuto di alterazioni successive alla finalizzazione;
- regressione con il validatore C9.

## Non verificato hardware

- nessun nodo PFD/MFD contattato;
- nessun renderer o scenario avviato;
- nessuna cache modificata e nessun fault iniettato;
- nessuna metrica o immagine reale acquisita;
- nessuna firma operatore reale raccolta.

## Stato

C10 è `VERIFIED_SW_NOT_RUN`. Il tooling elimina l'ambiguità operativa fra il
contratto C9 e la futura acquisizione, ma non produce evidenza hardware.
`AVX-G3-01` resta `OPEN`; `SV-001`, `PFD-001` e `MFD-001` restano `PARTIAL`.
Il prossimo passo autorizzabile è creare una sessione dal commit realmente
installato e acquisire la baseline corrente sui due RPi5 senza cambiare soglie.
