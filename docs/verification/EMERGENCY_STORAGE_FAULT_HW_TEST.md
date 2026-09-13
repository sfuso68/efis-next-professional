# Emergency storage fault — prova Blackbox isolata

Stato: **HARNESS IMPLEMENTATO / ESECUZIONE SU HARDWARE APERTA / NON-CERT**

## Scopo

Verificare sul nodo Blackbox che l'hub Emergency:

- non confermi un record quando il filesystem è pieno;
- esponga health degradata;
- accetti esattamente lo stesso record dopo il ripristino dello spazio;
- conservi i record dopo un arresto brusco del processo;
- superi il controllo SQLite all'avvio successivo.

La prova non simula un'interruzione elettrica dell'NVMe e non chiude il gate
power-loss.

## Isolamento

`tools/blackbox/test_emergency_storage_fault_hw.sh` crea una nuova immagine
ext4 da 32 MiB sotto `/var/tmp`, la monta come loopback con `nodev`, `nosuid` e
`noexec`, avvia una seconda istanza dell'hub su `127.0.0.1:28094` e usa un token
temporaneo. Non indirizza:

- `/var/lib/blackbox/emergency`;
- la porta operativa 18094;
- `efis-emergency-event-hub.service`;
- il database reale.

Il flag esplicito impedisce l'avvio accidentale. Il cleanup rimuove soltanto i
file nominati nella directory temporanea con prefisso controllato; non usa una
cancellazione ricorsiva.

## Esecuzione

Dal checkout della stessa revisione da verificare sulla Blackbox:

```sh
cd /percorso/efis-next-professional
sudo tools/blackbox/test_emergency_storage_fault_hw.sh --confirm-isolated-loopback
```

Risultato atteso:

```text
PASS: isolated filesystem full, recovery and abrupt process restart
REPORT=/var/tmp/efis-emergency-storage-fault-report-....json
```

Il rapporto non contiene token. Conservarlo come evidenza insieme a:

```sh
systemctl is-active efis-emergency-event-hub.service
systemctl show efis-emergency-event-hub.service -p NRestarts -p ExecMainStatus
curl -sS http://10.77.10.2:18094/health
```

L'ultimo comando verifica che il servizio operativo sia rimasto disponibile;
non deve essere usato per dedurre che la prova isolata abbia esercitato il
database reale.

## Criterio di esito

Il rapporto è `PASS` soltanto con sequenza HTTP `201 → 507/503 → 201/200`, due
record presenti prima di `SIGKILL` e gli stessi due record dopo il riavvio.
`power_loss_verified` resta obbligatoriamente `false`.
