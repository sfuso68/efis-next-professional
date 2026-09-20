# EFIS NEXT — Registro canonico delle release sui nodi

Aggiornamento documentale: 2026-09-20
Stato: DEVELOPMENT / NON-CERT

Questo file è l'unica fonte documentale autorizzata per rispondere alla domanda
«quale release è attiva su un nodo?». I checkpoint restano evidenze storiche e
non possono sovrascrivere questo registro.

## Regola di lettura

- `ULTIMA OSSERVATA` significa che la release è stata letta sul nodo nella data
  indicata; non dimostra che sia ancora attiva oggi.
- `ATTIVA CONFERMATA` richiede una nuova lettura read-only del symlink o del
  processo in esecuzione, più commit completo e timestamp UTC.
- `DA RICONFERMARE` è il solo stato corretto quando esistono evidenze storiche
  successive ma manca una lettura corrente.
- Una build, un test ARM64, un archivio trasferito o una release disponibile per
  rollback non valgono come prova di attivazione.

## Registro corrente

| Nodo | Ruolo | Indirizzo noto | Ultima release osservata | Data evidenza | Stato canonico corrente | Evidenza | Prossima verifica |
|---|---|---:|---|---|---|---|---|
| `efis-pfd-01` | PFD RPi5 | `192.168.1.45` | `60b3526` | 2026-09-20 03:48 UTC | ATTIVA CONFERMATA | symlink immutabile, unità, PID, processo e SHA-256 acquisiti read-only | ricondurre il recorder legacy `build/dev` alla gestione release senza modificarlo durante l'audit |
| `pi5` | MFD RPi5 | `192.168.1.49` | `3432402` | 2026-09-20 03:48 UTC | ATTIVA CONFERMATA / SUPERVISORE APERTO | symlink, processo e SHA-256 coincidono; `efis-mfd.service` assente | identificare il meccanismo reale di avvio e documentarne restart/boot policy |
| `efis` | EMS RPi4 | `192.168.1.36` | non identificabile | 2026-09-20 03:48 UTC | ATTIVA NON IMMUTABILE / NON CONFORME | processo reale da `/home/pi/efis-next/build/dev/efis-ems-qt`; unità nominativa e manifest assenti | creare profilo release immutabile solo dopo review, senza dedurre un commit dal checkout mutabile |
| `efisblackbox` | Blackbox RPi5 | `192.168.1.47` | broker attivo non identificato | 2026-09-20 03:48 UTC | ATTIVA NON IDENTIFICATA | servizio attivo; hash corrente diverso dalla copia `.e916548`; nessun manifest nodo | produrre manifest completo e rimuovere l'ambiguità di versione; feed motore resta SIM |
| `ems-sensor` | ASSO5 sensor RPi4 | `192.168.1.89` | nessuna release distribuita | 2026-09-18 | NON DISTRIBUITA | identità nodo verificata; adapter reale non distribuito | nessun deployment prima di inventario e approvazione G1 |
| `PORTATILE` | Ground station | `192.168.1.39` | non applicabile/untracked | 2026-09-15 | DA INVENTARIARE | QGC e firewall osservati separatamente | registrare package/versioni/config hash in sola lettura |
| `serverha` | Server mappe | `192.168.1.6` | non applicabile/untracked | 2026-09-14 | DA INVENTARIARE | container osservati Up; nessun release manifest canonico | acquisire image digest e compose revision |

## Conflitti chiusi documentalmente

I valori PFD `1d25e62`, `93bbe44`, `e902973` e `60b3526` descrivono una
sequenza storica, non quattro stati correnti simultanei. La lettura hardware
del 2026-09-20 conferma `60b3526`. Per lo stesso motivo `f27508a`, `1d02b84` e
`3432402` sono la sequenza storica MFD; la lettura corrente conferma il processo
e il binario `3432402`, ma non il suo supervisore.

L'acquisizione corrente è documentata in
[`evidence/release-inventory-20260920/README.md`](evidence/release-inventory-20260920/README.md).
Nessuna riga di questo registro autorizza deployment, rollback o uso in volo.
