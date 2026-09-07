# Checkpoint Blackbox DDP — 2026-09-07

## Punto fissato

- PR #5 integrata in `work/codex-multisource-p1` al commit
  `1f679b6649678a2a695437f135f2e8b70d34617d`.
- `efis-blackbox-broker` collega la pipeline normalizzata a recorder e DDP.
- Build completa e 15 test su 15 superati prima del merge.
- Nessun binario del broker è stato installato sulla Blackbox reale.
- Nessun servizio, bus CAN o parametro Pixhawk è stato modificato.

## Motivo della sospensione hardware

L'accesso SSH diretto a `pi@192.168.1.47` e quello indiretto tramite il PFD sono
stati rifiutati perché la workstation corrente non possiede una chiave già
autorizzata. È stata generata una chiave dedicata fuori dal repository; la
chiave pubblica deve ancora essere aggiunta a `~pi/.ssh/authorized_keys`.

Il lavoro hardware riprende solo quando l'operatore scrive **fatto blackbox**.
Quella frase significa esclusivamente che l'accesso SSH è stato autorizzato;
non costituisce evidenza di installazione o collaudo.

## Sequenza di ripresa

1. audit in sola lettura di utenti, storage, servizi, porte e repository;
2. identificazione documentata di `can_a` e `can_b` senza trasmettere frame;
3. build ARM del commit approvato in una directory separata;
4. installazione binario/unità senza sostituire `bbxlog`;
5. avvio manuale temporaneo, poi verifica recorder e DDP;
6. attivazione persistente solo dopo esito positivo;
7. aggiornamento di `EFIS_NEXT_STATE.md` con evidenza distinta software/HW.

`BBX-003` resta `PARTIAL` fino alle prove Ethernet e ai fault test reali.
