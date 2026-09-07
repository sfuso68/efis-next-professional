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

Il receiver è stato poi compilato e provato sul PFD reale. La prova correlata ha
mostrato `WAITING → LIVE`, 138 snapshot accettati, zero rifiuti, gap ed errori
socket. Un arresto controllato del broker ha prodotto lo stato `LOST`; il log
non ha campionato separatamente lo stato intermedio `STALE`.

Il vecchio servizio PFD puntava a un binario già eliminato da `build/dev` ed è
risultato non riavviabile dopo l'arresto. Il renderer verificato è stato quindi
reso persistente tramite `/home/efis/efis-next-current` e il drop-in systemd
`efis-pfd-release.override.conf`. PFD e broker sono ora attivi e abilitati; il
broker resta read-only sugli ingressi e `can_a` conserva zero pacchetti TX.

## Sequenza di ripresa

1. audit in sola lettura di utenti, storage, servizi, porte e repository;
2. identificazione documentata di `can_a` e `can_b` senza trasmettere frame;
3. build ARM del commit approvato in una directory separata;
4. installazione binario/unità senza sostituire `bbxlog`;
5. avvio manuale temporaneo, poi verifica recorder e DDP;
6. attivazione persistente solo dopo installazione e prova del receiver PFD —
   completata sulla WLAN di banco;
7. aggiornamento di `EFIS_NEXT_STATE.md` con evidenza distinta software/HW —
   completato per lo staging Blackbox.

`BBX-003` resta `PARTIAL`: mancano Ethernet cablata, autenticazione, MAVLink
reale, riordino, reboot, latenza/jitter e carico prolungato.
