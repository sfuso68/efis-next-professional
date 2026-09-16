# Checkpoint — AVIONIX NEXT offline gates — 2026-09-16

## Perimetro autorizzato

Il proprietario ha congelato la ricognizione hardware `AVX-G1-01/02` fino alla
disponibilità del terminale locale ASSO5 e ha autorizzato l'avanzamento delle
attività che non richiedono interazione con terminali remoti.

## Congelato, non verificato

- `192.168.1.89` risponde alla rete, ma gli utenti `avionix`, `pi` ed `efis`
  rifiutano le chiavi SSH disponibili.
- Nessun login è stato ottenuto e nessun comando è stato eseguito sul nodo.
- Identità OS/storage/power e inventario 4/4 Sequent rimangono aperti.
- Non sono stati modificati cablaggi, DIP, EEPROM, watchdog, servizi o uscite.

## Implementato offline

- manifesto macchina `avionix-next.data-contract.v1` per DDP V2 ed EDP V1;
- specifica unificata con confini, freshness e regole fail-closed;
- validatore contro header C++, piano di rete e documenti wire;
- cinque test automatici, inclusi porta errata, schema divergente, freshness
  incoerente e capacità di comando non ammessa.

## Evidenza software

```text
data contract: PASS — 2 contratti, 3 flussi, 10 costanti sorgente
network plan:  PASS — 2 segmenti, 11 nodi, 4 flussi, 2 switch
unit tests:    PASS — 5/5
python syntax: PASS
```

Queste prove non costituiscono acceptance hardware o autorizzazione al volo.

## Ripresa G1

Quando il terminale ASSO5 sarà disponibile, installare esclusivamente la
chiave pubblica già comunicata e riprendere i comandi read-only previsti dal
Master Brief. G2 può proseguire offline con vettori wire e replay, ma la
verticale reale G2-02 resta dipendente dall'inventario G1.
