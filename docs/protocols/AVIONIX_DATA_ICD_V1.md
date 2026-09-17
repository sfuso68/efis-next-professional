# AVIONIX NEXT — Data ICD V1

Revisione: 1.0.0

Freeze: 2026-09-17

Stato: **FROZEN-SW / DEVELOPMENT / NON-CERT**

## 1. Autorità e perimetro

Questo documento congela il contratto software fra Blackbox e display sopra i
protocolli già implementati DDP V2 ed EDP V1. Non cambia il wire format, non
autorizza deployment e non costituisce approvazione al volo.

La specifica machine-readable normativa è
`config/protocols/avionix-data-icd-v1.json`. Il documento prevale sulle note
storiche solo per il contratto G2; codice, ADR e prove hardware conservano il
loro livello di autorità definito dall'indice documentale.

`AVIONIX NEXT` è l'etichetta del programma usata da questo ICD. Nomi tecnici
`EFIS NEXT`, prefissi `efis-` e nomi di servizi restano alias di compatibilità:
il freeze non decide il naming G0 e non autorizza rinomina.

## 2. Matrice dei flussi

| Flusso | Publisher autorevole | Consumer | Porta | Direzione consumer | Comandi |
|---|---|---|---:|---|---|
| DDP V2 | Blackbox broker, nodo installativo 47 | PFD/MFD RPi5 | UDP/49002 | receive-only | vietati |
| EDP V1 | Blackbox broker, nodo installativo 47 | EMS RPi4 | UDP/49003 | receive-only | vietati |

Il trasporto è IPv4 UDP unicast. Gli endpoint sono default di installazione,
non costanti del codec. Multicast, broadcast, ACK e ritorno di comandi non
fanno parte di V1.

CRC32C e identità dichiarata rilevano corruzione e sorgenti inattese, ma **non
autenticano** il mittente. Questa baseline dipende da rete FLIGHT-DATA isolata,
filtraggio e allow-list. Non deve essere esposta a reti non fidate.

## 3. Regole wire comuni

- interi big-endian; `f64` IEEE-754 binary64 in ordine di byte di rete;
- CRC32C Castagnoli calcolato sul solo payload; vettore noto `123456789` =
  `e3069283`;
- `source_node` e `publisher_instance` zero sono invalidi;
- il publisher installativo inizia da sequenza 1 e la incrementa per sessione;
- un reboot deve cambiare `publisher_instance`; pacchetti di un'istanza ritirata
  vengono scartati;
- duplicati, riordino, schema/enum ignoti, lunghezza o CRC errati e payload
  malformati vengono rifiutati integralmente;
- il receiver non sostituisce l'ultimo snapshot valido con un datagramma
  rifiutato e rende visibile il degrado del link.

Le enumerazioni stabili di `SourceId`, `Quality`, `Validity` e `Unit`, con i
rispettivi valori numerici, sono congelate nel JSON normativo. I nuovi valori
si aggiungono in coda in una futura revisione; non si riusano né rinumerano.

## 4. Freshness e stato del link

Lo stato usa esclusivamente il clock monotono locale del consumer:

| Stato | Condizione |
|---|---|
| `WAITING` | nessun snapshot valido ricevuto |
| `LIVE` | età `<= 250 ms` |
| `STALE` | `250 ms <` età `<= 1000 ms` |
| `LOST` | età `> 1000 ms` |

Il timestamp monotono del publisher non viene confrontato con quello del
consumer. Le soglie sono la baseline software corrente, non un requisito
certificato di latenza o jitter. Le cadenze operative e l'endurance hardware
restano da qualificare.

## 5. DDP V2

Magic `EFN2`, schema 2, envelope 44 byte, payload massimo 65.536 byte. Il solo
payload decodificato dalla baseline è `snapshot` (tipo 1); i tipi 2–4 sono
riservati e non implicano supporto runtime.

### 5.1 Envelope

| Offset | Dimensione | Campo |
|---:|---:|---|
| 0 | 4 | magic |
| 4 | 2 | schema |
| 6 | 1 | profile |
| 7 | 1 | payload type |
| 8 | 4 | source node |
| 12 | 8 | publisher instance |
| 20 | 8 | sequence |
| 28 | 8 | monotonic microseconds |
| 36 | 4 | payload length |
| 40 | 4 | payload CRC32C |

Lo snapshot contiene un header health di 25 byte e **23** record da 20 byte,
per un payload canonico di 485 byte e un datagramma di 529 byte. Gli offset di
ogni campo, i profili, il health e l'ordine completo dei 23 segnali sono nel
JSON normativo. I due booleani `flight_director_active` e `to_flag` usano unità
`boolean` e valore esatto `0.0` o `1.0`.

## 6. EDP V1

Magic `EDP1`, schema 1, envelope 42 byte. Lo snapshot contiene un header di 4
byte e **18** record da 36 byte: payload 652 byte, datagramma 694 byte.

### 6.1 Envelope

| Offset | Dimensione | Campo |
|---:|---:|---|
| 0 | 4 | magic |
| 4 | 2 | schema |
| 6 | 4 | source node |
| 10 | 8 | publisher instance |
| 18 | 8 | sequence |
| 26 | 8 | monotonic microseconds |
| 34 | 4 | payload length |
| 38 | 4 | payload CRC32C |

Il record preserva valore ingegneristico e raw, provenienza nodo/canale,
revisione di calibrazione, validità, qualità, unità, età e flag SIM. L'ordine
canonico comprende RPM, MAP, olio, carburante, bus, alternatore, quattro CHT e
quattro EGT ed è congelato nel JSON normativo.

## 7. Vettori deterministici

`tests/data/protocols/avionix-data-icd-v1-vectors.json` definisce identità e
campioni canonici. Il validator ricostruisce i frame senza usare il codec C++:

| Protocollo | Payload CRC32C | SHA-256 datagramma | Byte |
|---|---|---|---:|
| DDP V2 | `48e6c31c` | `f15e1173a431a51bb7a943778dfa7f712a34e35b91c0363263321e2e4ebe4544` | 529 |
| EDP V1 | `376ee061` | `d92e0a6e39d15c68b5b00866aa4dfbb35b465c7aa6856d1457710fa0f31c373c` | 694 |

La verifica è fail-closed:

```text
python tools/protocol/validate_avionix_icd.py
python -m unittest tests.test_avionix_icd
```

## 8. Evidence bundle V1

Lo schema normativo è
`config/protocols/avionix-evidence-bundle-v1.schema.json`. Ogni bundle dichiara
requisiti, gate, livello di evidenza, ambiente, commit, artifact con SHA-256,
check, limitazioni ed esito. Proprietà ignote sono vietate e
`flight_approval` è sempre `false`.

Livelli ammessi:

- `VERIFIED_SW`: test ripetibile senza hardware target;
- `EVIDENCE_HW_HISTORICAL`: prova reale precedente, non riqualificata;
- `VERIFY_HW`: prova target ancora necessaria;
- `BLOCKED`: prova impedita da una dipendenza esplicita.

Un `PASS` software non può essere promosso a verifica hardware. Artifact senza
digest, ambiente o limitazioni non sono evidence bundle conformi.

## 9. Compatibilità e modifiche future

Il freeze è compatibile con l'implementazione corrente e verificato con parser
indipendente su workstation. Il replay dei vettori su ARM64 RPi4/RPi5 è aperto;
G2 resta quindi `IN_PROGRESS-SW/VERIFY-HW`.

Qualunque modifica di magic, offset, dimensione, enum, segnali obbligatori,
CRC, freshness o autorità richiede nuova revisione ICD, nuovi vettori, ADR e
strategia di compatibilità. Non è ammessa modifica silenziosa in-place.

## 10. Limiti espliciti

- nessuna autenticazione crittografica;
- nessuna garanzia di consegna UDP;
- nessuna cadenza/jitter/endurance certificata;
- nessuna verifica hardware introdotta da questo freeze;
- nessuna autorizzazione a comandi, controllo attuatori o uso in volo.
