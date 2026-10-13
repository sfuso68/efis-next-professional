# Policy backup — proposta, NON attiva

PRJ-001. Le copie correnti non vengono cancellate da questa proposta.

1. Main/tag: mantenere pipeline fast-forward e restore registrato; tag con
   contenuto diverso sono conflitti, non aggiornamenti.
2. Nuovi branch: proporre namespace separato `refs/archive/github/…`, senza
   sovrascrivere branch NAS esistenti. Branch spariti su GitHub conservati
   come storico, non prune automatico. Non copiare refs pull/merge effimere.
3. Retention: prima inventariare bundle, dimensioni, restore verificati e
   versione minima recuperabile. Proposta 30 snapshot giornalieri + 12 mensili;
   conservare comunque baseline pre-migrazione e ogni release approvata.
   Necessari quota concordata e dry-run revisionato prima di qualsiasi delete.
4. Off-site: seconda destinazione distinta, cifrata, con chiave di recupero
   fuori serverha. Destinazione/costi/credenziali richiedono scelta proprietario.
5. Prove: restore mensile main e release; trimestrale disaster-recovery da
   off-site; registrare data della prova separatamente dal controllo replica.
6. Avvisi: backup FAIL, disco sotto riserva, replica stale e restore oltre
   intervallo. Un alert non autorizza riavvii, force push o potatura.

Limiti: Git non conserva dati non tracciati, database, registrazioni, immagini
terrain o credenziali. Queste categorie richiedono backup dedicati con
inventario e verifica; non dichiararle protette dal solo bundle Git.
