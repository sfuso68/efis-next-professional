# Audit baseline e replica — 2026-10-10

Controllo read-only alle 09:13 UTC. PRJ-001 resta PARTIAL.

| Fonte | main osservato | Esito |
|---|---|---|
| GitHub e copia dedicata cruscotto | `1cc0a78682741872e19082ae84dae936370e3d7e` | allineati |
| serverha `/home/sfuso/git/efis-next-professional.git` | `7aa45b66fc71824eb0dd4b1e6ffe898ecfcdb897` | arretrato rispetto GitHub |
| NAS main via `nas-replica` | `7aa45b66fc71824eb0dd4b1e6ffe898ecfcdb897` | allineato al serverha arretrato |

Timer NAS active, ultima esecuzione Result=success/ExecMainStatus=0.
Questo prova replica della sorgente locale, NON aggiornamento da GitHub.
Cruscotto GITHUB_SYNC_OK. Disco serverha: 114 GB disponibili, 46% uso.
Connectivity fsck della copia cruscotto termina senza errori oggetti, ma
avvisa che HEAD è ancora master non nato: main resta leggibile e validato.
Restore, reboot e off-site backup non eseguiti in questo audit.

Problema aperto: introdurre lettura GitHub nella pipeline backup esistente
con fast-forward verificato, confronto ancestry, protezione dei tag e
snapshot recuperabile prima dell'aggiornamento. Non fare push --mirror,
force, reset o sostituire il repository NAS. Prima inventariare tutte le
refs su serverha/NAS e verificare divergenze; poi un aggiornamento normale
main e prova di restore. Nessuna modifica delle copie backup in questo lavoro.

Cruscotto/planning salvati sul branch `work/project-dashboard-planning-v1`
nel commit `c8a50c1`, push riuscito. Il cruscotto pubblico via tunnel segue
solo main e quindi NON mostra ancora questi cambiamenti non integrati.
