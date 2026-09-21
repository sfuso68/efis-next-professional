# ADR-018 — Topologia dei repository Git e replica NAS

- Stato: accettata per controllo progetto e disaster recovery
- Data: 2026-09-21
- Riferimenti: AVX-G0-02, AVX-G0-03, DEC-G0-002, PRJ-001

## Decisione

La topologia Git autorevole è la seguente:

1. GitHub `sfuso68/efis-next-professional` è la sorgente condivisa autorevole
   per codice, pull request e CI.
2. `serverha` (`192.168.1.6`) ospita il mirror locale primario
   `/home/sfuso/git/efis-next-professional.git` e la working copy dedicata a
   Claude Code `/home/sfuso/work/efis-next-professional`. Il collegamento
   operativo workstation→serverha negozia
   `mlkem768x25519-sha256`.
3. Il NAS DS214 conserva la replica archivio
   `/volume1/homes/sfuso/git/efis-next-professional.git`. `serverha` replica
   `main` e i tag ogni 15 minuti con una chiave dedicata limitata dal NAS al
   solo `git-receive-pack` di quel percorso.
4. La Blackbox conserva temporaneamente il bare ridondante
   `/home/pi/git/efis-next-professional.git`. Non è autorevole e nessun runtime
   di volo può dipendere dalla sua disponibilità.
5. Il precedente `/volume1/homes/sfuso/git-repos/efis.git` è classificato
   `LEGACY`: resta intatto e non autorevole. Merge, promozione o cancellazione
   richiedono un audit e un'autorizzazione separati.

I bare locali rifiutano cancellazioni di ref e aggiornamenti non fast-forward.
La replica non può modificare hardware, servizi avionici o release dei nodi.

## Evidenza di accettazione

Il 2026-09-21 i riferimenti `main` di GitHub, serverha, Blackbox e NAS sono
stati letti e confrontati. Tutti indicavano:

`5891f754ee06948349f711c57c90e8700137d09f`

Il timer utente `efis-git-nas-sync.timer` su serverha risultava attivo; il
relativo servizio oneshot aveva terminato con `status=0/SUCCESS` ed
`Everything up-to-date`. È stata inoltre provata la chiave NAS ristretta: una
richiesta shell arbitraria è stata rifiutata.

## Conseguenze e gate residui

- La presenza di più copie non crea più ambiguità: autorità e finalità sono
  esplicite.
- Il DS214 è limitato a DSM 7.1.1/OpenSSH 8.2; il tratto interno serverha→NAS
  resta quindi classico, confinato alla LAN e con chiave a comando forzato.
- La replica serverha→NAS non sostituisce una prova periodica di ripristino,
  né snapshot o backup fuori sito.
- Il mirror serverha non acquisisce automaticamente modifiche nate su GitHub:
  ogni integrazione deve aggiornare esplicitamente serverha, finché non verrà
  approvato un fetch autenticato e sorvegliato.
- La copia Blackbox potrà essere ritirata solo dopo un restore drill riuscito
  da NAS e autorizzazione esplicita; questa ADR non autorizza cancellazioni.
- Nessuna evidenza qui contenuta costituisce qualifica hardware avionica o
  approvazione al volo.
