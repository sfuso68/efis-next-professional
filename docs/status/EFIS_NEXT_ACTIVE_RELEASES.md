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
| `efis-pfd-01` | PFD RPi5 | `192.168.1.45` | `60b3526` | 2026-09-12 | DA RICONFERMARE | archivio storico, sezione Emergency Assist `60b3526` | leggere target di `/home/efis/efis-next-current`, `ExecStart`, PID e commit senza effettuare switch |
| `pi5` | MFD RPi5 | `192.168.1.49` | `3432402` | 2026-09-12 | DA RICONFERMARE | archivio storico, sezione MFD `3432402` | leggere symlink, unità e processo senza modifiche |
| `efis` | EMS RPi4 | `192.168.1.36` | non determinata da commit | 2026-09-12 | DA RICONFERMARE | EDP V1 e runtime osservati, ma nessun hash release univoco | registrare symlink/processo/commit e versione EDP |
| `efisblackbox` | Blackbox RPi5 | `192.168.1.47` | broker `e916548` soltanto | 2026-09-08 | DA RICONFERMARE | broker installato e provato; non identifica l'intera release nodo | registrare release completa, broker, recorder e unità attive |
| `ems-sensor` | ASSO5 sensor RPi4 | `192.168.1.89` | nessuna release distribuita | 2026-09-18 | NON DISTRIBUITA | identità nodo verificata; adapter reale non distribuito | nessun deployment prima di inventario e approvazione G1 |
| `PORTATILE` | Ground station | `192.168.1.39` | non applicabile/untracked | 2026-09-15 | DA INVENTARIARE | QGC e firewall osservati separatamente | registrare package/versioni/config hash in sola lettura |
| `serverha` | Server mappe | `192.168.1.6` | non applicabile/untracked | 2026-09-14 | DA INVENTARIARE | container osservati Up; nessun release manifest canonico | acquisire image digest e compose revision |

## Conflitti chiusi documentalmente

I valori PFD `1d25e62`, `93bbe44`, `e902973` e `60b3526` descrivono una
sequenza storica, non quattro stati correnti simultanei. `60b3526` è l'ultima
release PFD documentata come osservata; fino alla nuova lettura hardware lo
stato rimane `DA RICONFERMARE`. Per lo stesso motivo `f27508a`, `1d02b84` e
`3432402` sono la sequenza storica MFD, con `3432402` ultima osservata.

Nessuna riga di questo registro autorizza deployment, rollback o uso in volo.
