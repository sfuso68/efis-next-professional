# AVIONIX NEXT — G2 ICD freeze readiness

Data verifica: 2026-09-17  
Baseline ispezionata: `e0275c8d26f3ebbc38604f82f0d635533c4e5913`  
Stato: READINESS ONLY / FREEZE NON AUTORIZZATO

## Scopo

Questa verifica prepara `AVX-G2-01` senza congelare l'ICD e senza modificare
wire protocol, runtime, configurazioni, servizi o nodi. Le fonti confrontate
sono implementazione C++, protocolli DDP V2/EDP V1, ADR-008…011, strumenti di
cattura/assessment/replay, test e documentazione di deployment.

## Baseline verificata solo software

- `efis_ddp_v2_tests`: PASS;
- `efis_ddp_udp_tests`: PASS;
- `efis_edp_v1_tests`: PASS;
- suite Python cattura/assessment/replay EDP: 26/26 PASS;
- CI della baseline documentale: EFIS Project Control #86 PASS ed EFIS Core
  CI #70 PASS.

I test dimostrano coerenza software locale e CI. Non qualificano Ethernet,
RPi4/RPi5, Blackbox, Pixhawk, sensori o cablaggi reali.

## Contratti già coerenti

- DDP V2: magic `EFN2`, schema 2, envelope 44 byte, payload massimo 64 KiB,
  CRC32C, identità publisher, sequenza e policy consumer 250/1000 ms.
- EDP V1: magic `EDP1`, schema 1, envelope 42 byte, 18 campioni obbligatori,
  CRC32C, identità publisher, sequenza e policy consumer 250/1000 ms.
- Entrambi i percorsi sono UDP unicast, read-only lato display e fail-closed
  su identità, struttura, CRC, duplicati e riordino.
- Provenienza, simulazione, validità, qualità, età e stato del collegamento
  sono preservati; nessun esito di test concede approvazione al volo.

## Disallineamenti da chiudere nel freeze

1. `DISPLAY_DATA_PROTOCOL_V2.md` conserva frasi storiche secondo cui adapter,
   processi reali e installazione sarebbero ancora assenti; codice, deployment
   ed evidenze successive dimostrano che parte di questi elementi esiste.
2. Le evidenze installative pregresse non equivalgono a una nuova acceptance
   hardware della baseline corrente: il testo finale dovrà distinguere
   `VERIFIED_SW`, `EVIDENCE_HW_HISTORICAL` e `VERIFY_HW`.
3. Manca un unico ICD sovraordinato che assegni formalmente domini, endpoint,
   cadenze, autorità, freshness, transizioni e responsabilità DDP/EDP.
4. EDP documenta i campi ma non una tabella canonica completa con offset,
   dimensioni, enumerazioni ammesse e vettori binari versionati.
5. DDP descrive l'envelope e lo snapshot, ma non pubblica un catalogo canonico
   di offset/payload e un digest di test vector indipendente dall'ABI.
6. Manca lo schema normativo dell'evidence bundle: manifest, digest, origine,
   ambiente, limiti, esito, livello SW/HW e divieto di `flight_approval`.
7. Cadenze operative, limiti di jitter/perdita e criteri di endurance hardware
   non sono ancora congelati; non devono essere inventati nel freeze.
8. Autenticazione del mittente non è implementata. Il freeze deve mantenere
   esplicito che CRC e `source_node` non sono autenticazione e che il prototipo
   dipende da isolamento e filtraggio della rete.
9. Il naming prodotto `AVIONIX NEXT`/`EFIS NEXT` resta una decisione G0 aperta;
   l'ICD dovrà usare una regola di alias provvisoria o attendere la decisione.

## Contenuto minimo proposto per il freeze

- matrice di autorità e flussi consentiti/proibiti;
- identificatori, tipi, endian, offset, range e compatibilità di versione;
- endpoint di default separati dalla configurazione installativa;
- cadenza nominale e limiti configurabili, senza promuovere valori non
  qualificati a requisiti aeronautici;
- macchina `WAITING/LIVE/STALE/LOST`, recovery e reboot publisher;
- regole provenance/SIM/degraded e comportamento del renderer;
- test vector DDP/EDP con SHA-256 e decoder indipendente;
- schema evidence bundle e livelli `VERIFIED_SW`, `VERIFY_HW`, `BLOCKED`;
- matrice di compatibilità x86-64/ARM64 e rollback documentale.

## Gate di ingresso e uscita

Ingresso al freeze: conferma esplicita dell'utente sul solo scope documentale.

Uscita proposta: documento ICD revisionato, vettori riproducibili, validator e
link dalla Master Matrix; tutti i test software verdi. Il gate resta
`IN_PROGRESS-SW/VERIFY-HW` finché non esistono prove reali su Ethernet e nodi
installati. Il verticale reale `AVX-G2-02` resta separato e dipendente da G1.

