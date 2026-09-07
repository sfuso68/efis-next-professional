# Checkpoint Blackbox DDP — 2026-09-07

## Punto fissato

- PR #5 integrata in `work/codex-multisource-p1` al commit
  `1f679b6649678a2a695437f135f2e8b70d34617d`.
- `efis-blackbox-broker` collega la pipeline normalizzata a recorder e DDP.
- Build completa e 15 test su 15 superati prima del merge.
- Nessun binario del broker è stato installato sulla Blackbox reale.
- Nessun servizio, bus CAN o parametro Pixhawk è stato modificato.

## Ripresa hardware del 2026-09-07

L'operatore ha autorizzato una chiave SSH V2 dedicata e l'accesso diretto a
`pi@192.168.1.47` è stato verificato. La chiave privata resta fuori dal
repository.

La sequenza prevista è stata eseguita fino al collaudo temporaneo dell'unità:

- audit di sistema, storage, servizi e porte completato;
- `can_a` identificato come bus sensori corrente a 500 kbit/s in listen-only;
  `can_b` era attivo ma silente;
- build aarch64 del commit `35dff11` completata e 15 test su 15 superati;
- binario e unità installati senza sostituire o arrestare `bbxlog`;
- prova reale come utente `blackbox` completata con recorder e DDP attivi,
  zero errori di invio e zero pacchetti CAN TX.

Il servizio è stato arrestato ed è rimasto disabilitato. Il receiver DDP non è
ancora presente nel deployment PFD reale; l'attivazione persistente resta quindi
sospesa e non è evidenza end-to-end.

## Sequenza di ripresa

1. audit in sola lettura di utenti, storage, servizi, porte e repository;
2. identificazione documentata di `can_a` e `can_b` senza trasmettere frame;
3. build ARM del commit approvato in una directory separata;
4. installazione binario/unità senza sostituire `bbxlog`;
5. avvio manuale temporaneo, poi verifica recorder e DDP;
6. attivazione persistente solo dopo installazione e prova del receiver PFD;
7. aggiornamento di `EFIS_NEXT_STATE.md` con evidenza distinta software/HW —
   completato per lo staging Blackbox.

`BBX-003` resta `PARTIAL` fino alle prove Ethernet e ai fault test reali.
