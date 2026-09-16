# Emergency storage fault — Blackbox 2026-09-14

- Target: `efisblackbox`, ARM64, management IP `192.168.1.47`.
- Harness: `tools/blackbox/test_emergency_storage_fault_hw.sh`.
- Isolamento: ext4 loopback 32 MiB sotto `/var/tmp`, bind
  `127.0.0.1:28094`, token e database temporanei.
- Report remoto:
  `/var/tmp/efis-emergency-storage-fault-report-20260914T085410Z.json`.
- Esito: PASS per filesystem pieno, recovery e riavvio brusco del solo processo
  hub isolato.
- Produzione: `efis-emergency-event-hub.service` ed
  `efis-blackbox-broker.service` active con `NRestarts=0`; il test non li ha
  fermati né riavviati.
- Cleanup: nessun listener su 28094 e nessun mount con prefisso di test dopo
  l'uscita.
- Limite: `power_loss_verified=false`; nessuna prova sul volume production.

Il listener production era presente su `10.77.10.2:18094`. Una richiesta
locale verso quel bind è bloccata dalla policy nftables che rifiuta destinazioni
10.77.10.0/24 non instradate su `eth0`; serve una successiva prova health dal
PFD attraverso il collegamento FLIGHT-DATA.
