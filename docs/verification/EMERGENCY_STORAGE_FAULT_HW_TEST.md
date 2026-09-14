# Emergency storage fault — prova Blackbox isolata

Stato: **PASS SU FILESYSTEM ISOLATO BLACKBOX / POWER-LOSS APERTO / NON-CERT**

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

## Esecuzione registrata — 2026-09-14

Nodo: `efisblackbox` (`192.168.1.47`, ARM64).

Durante i primi due tentativi l'harness si è arrestato prima della fault
injection e ha evidenziato due difetti nello script:

- la directory `mktemp` 0700/root impediva all'hub avviato come `blackbox` di
  raggiungere token e database isolati;
- il testo dell'evento superava il limite protocollo di 4096 caratteri e veniva
  correttamente respinto con HTTP 400.

Lo script ora assegna al gruppo `blackbox` il solo attraversamento della
directory radice (`root:blackbox`, 0710) e mantiene il payload sotto il limite.
Dopo entrambe le interruzioni il cleanup non ha lasciato mount o listener.

Il terzo tentativo ha prodotto `PASS` con:

- scrittura iniziale HTTP 201;
- filesystem pieno HTTP 507 e `persisted=false`;
- health degradata HTTP 503;
- nuova scrittura dopo recupero HTTP 201;
- 2 record prima e dopo `SIGKILL`;
- `quick_check` superato al riavvio;
- servizio production `active` prima e dopo, senza restart.

Evidenza: `docs/status/evidence/emergency-storage-fault-blackbox-20260914/`.
Il listener production resta vincolato a `10.77.10.2:18094`; il firewall del
nodo rifiuta intenzionalmente traffico verso `10.77.10.0/24` che non esce da
`eth0`, quindi una health locale instradata su `lo` non è una prova valida. La
health end-to-end deve essere verificata dal PFD sul collegamento FLIGHT-DATA.
