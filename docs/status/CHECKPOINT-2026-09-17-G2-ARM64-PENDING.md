# Checkpoint — G2 ARM64 autorizzato, evidenza pendente — 2026-09-17

## Punto congelato

Il proprietario ha autorizzato la verifica read-only del freeze G2 sui tre
nodi ARM64 PFD, EMS e MFD. Questa autorizzazione non equivale a esito positivo.

## Tentativo eseguito

Dal workstation Windows è stato tentato esclusivamente l'accesso SSH in
lettura ai nodi noti `.45`, `.36` e `.49`. Tutti e tre hanno rifiutato
l'autenticazione. Il servizio `ssh-agent` locale risultava fermo/disabilitato e
non era disponibile una chiave utilizzabile. Nessun nodo e nessun servizio è
stato modificato.

## Procedura predisposta

È stato consegnato un blocco di comandi da eseguire sul PFD. La procedura:

1. recupera l'HEAD congelato `c331a9d3846d1f59addcea76f9db530174c8ec11`;
2. estrae in `/tmp` soltanto i sei file G2 necessari;
3. esegue validator e unit test sul PFD;
4. ripete la stessa verifica read-only su EMS e MFD;
5. salva il riepilogo in `/tmp/avionix-g2-arm64-results.txt`.

## Stato evidenza

- PFD ARM64: PENDING.
- EMS ARM64: PENDING.
- MFD ARM64: PENDING.
- Hardware reale verificato da questa attività: nessuno.

`PRJ-003` resta `VERIFY-HW` e `AVX-G2-01` resta `IN_PROGRESS`. La chiusura
richiede il report integrale con identità macchina, architettura, digest ed
esito dei test; un messaggio generico “eseguito” non è evidenza sufficiente.
