# EFIS NEXT — Registro canonico delle release sui nodi

Aggiornamento documentale: 2026-09-21
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
| `efis-pfd-01` | PFD + recorder RPi5 | `192.168.1.45` | `pfdrec-d52e377-primary-v1` (`d52e377d495dd8d33f1e7e40c3b67a663168a749`) | 2026-09-21 03:22 UTC | ATTIVA IMMUTABILE CONFERMATA | manifest dual-role PASS, processi canonici, rollback drill e reboot PASS; boot `be165369-0f3a-4477-a095-b45ccfab6f32`, DDP LIVE e recorder status crescente | acceptance visuale/touch, terrain, fault dati e soak della release corrente |
| `pi5` | MFD RPi5 | `192.168.1.49` | `mfd-3432402-primary-v1` (`3432402ae35c168a14a14fe408a2b635fe750c92`) | 2026-09-20 | ATTIVA IMMUTABILE CONFERMATA | manifest PASS, unità `efis-next-mfd-direct.service`, eseguibile canonico, rollback drill e reboot PASS | acceptance touch/terrain e soak della release corrente |
| `efis` | EMS RPi4 | `192.168.1.36` | `ems-7655799-primary-v1` (`765579959f7970a17eb212ca1ef13628aaa4d71f`) | 2026-09-20 17:20 UTC | ATTIVA IMMUTABILE CONFERMATA | manifest PASS, rollback e reboot PASS; EDP Blackbox 47 `LIVE • SIM`; stato runtime persistente esterno alla release | sorgenti motore reali, calibrazioni, touch e soak restano aperti |
| `efisblackbox` | Blackbox RPi5 | `192.168.1.47` | `blackbox-7655799-primary-v1` (`765579959f7970a17eb212ca1ef13628aaa4d71f`) | 2026-09-20 | ATTIVA IMMUTABILE CONFERMATA | manifest PASS, binario canonico, rollback drill, reboot e broker health PASS | engine resta esplicitamente SIM; CAN reale, endurance e power qualification aperti |
| `ems-sensor` | ASSO5 sensor RPi4 | `192.168.1.89` | nessuna release distribuita | 2026-09-18 | NON DISTRIBUITA | identità nodo verificata; adapter reale non distribuito | nessun deployment prima di inventario e approvazione G1 |
| `PORTATILE` | Ground station | `192.168.1.39` | non applicabile/untracked | 2026-09-15 | DA INVENTARIARE | QGC e firewall osservati separatamente | registrare package/versioni/config hash in sola lettura |
| `serverha` | Server mappe | `192.168.1.6` | non applicabile/untracked | 2026-09-14 | DA INVENTARIARE | container osservati Up; nessun release manifest canonico | acquisire image digest e compose revision |

## Conflitti chiusi documentalmente

I valori PFD `1d25e62`, `93bbe44`, `e902973` e `60b3526` descrivono una
sequenza storica, non quattro stati correnti simultanei. La lettura hardware
del 2026-09-20 confermava `60b3526`; il rollout del 2026-09-21 ha poi attivato
la release dual-role `pfdrec-d52e377-primary-v1`. Per lo stesso motivo
`f27508a`, `1d02b84` e
`3432402` sono la sequenza storica MFD; la lettura corrente conferma il processo
e il binario `3432402`, ma non il suo supervisore.

L'inventario iniziale è documentato in
[`evidence/release-inventory-20260920/README.md`](evidence/release-inventory-20260920/README.md).
La migrazione autorizzata MFD/EMS/Blackbox e le prove di rollback/reboot sono in
[`evidence/release-immutability-20260920/README.md`](evidence/release-immutability-20260920/README.md).
Nessuna riga di questo registro autorizza deployment, rollback o uso in volo.
