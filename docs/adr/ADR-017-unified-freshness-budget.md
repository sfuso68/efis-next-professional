# ADR-017 — Budget unificato di freshness

- Stato: accettata per sviluppo e prove hardware
- Data: 2026-09-19
- Riferimenti: ADR-003, ADR-004, ADR-008, ADR-009, ADR-011

## Decisione

Tutte le soglie runtime del percorso dati distribuito derivano da
`efis::FreshnessBudget`, il solo punto C++ autorevole. Il tick base è 250 ms:

| Livello | Moltiplicatore | Soglia |
|---|---:|---:|
| margine età rete DDP | 1× | 250 ms |
| link STALE | 1× | 250 ms |
| campione STALE | 2× | 500 ms |
| link LOST | 4× | 1000 ms |
| nodo STALE | 5× | 1250 ms |
| banco STALE | 8× | 2000 ms |
| nodo LOST | 12× | 3000 ms |

Le soglie non sono sinonimi. Il link descrive il flusso dei datagrammi, il
campione la misura, il nodo il heartbeat e il banco una vista diagnostica lenta.
La sequenza temporale è intenzionale e verificata da `static_assert` e test.

DDP trasporta l'età dichiarata dal publisher ma i clock monotoni di host diversi
non sono confrontabili. Il consumer somma quindi un margine conservativo di un
tick all'età dichiarata. Una sincronizzazione futura misurata potrà sostituire
questo margine, ma non potrà assumere latenza zero.

## Conseguenze e gate

- nuove soglie non devono essere introdotte come letterali locali;
- configurazioni/ICD devono restare coerenti col contratto e sono validate
  separatamente;
- la variazione del nodo STALE da 1000 a 1250 ms evita che `link LOST` e
  `node STALE` cambino nello stesso istante;
- servono fault injection e misura jitter sui nodi reali prima di qualificare i
  valori. Questa ADR non costituisce approvazione al volo.
