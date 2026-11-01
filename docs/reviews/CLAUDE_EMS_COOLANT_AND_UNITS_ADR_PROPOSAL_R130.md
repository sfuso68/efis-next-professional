# Proposta di ADR — canale temperatura liquido e vocabolario unità EMS

- **Stato: proposta, non approvata e non eseguibile.** Segue la convenzione di
  `docs/adr/ADR-016-sensor-node-upstream-proposal.md`.
- **Data:** 2026-11-01.
- **Autore:** Claude Code, filone B. Non è una decisione architetturale: il
  piano di esecuzione parallela riserva all'integratore i registri canonici e
  le approvazioni, e questo documento si ferma alla raccomandazione.
- **Origine:** finding **E-01** ed **E-04** di
  `docs/reviews/CLAUDE_EMS_GAP_REVIEW_R130.md`, i due di priorità A che la
  sezione 7 di quella review classifica come «non lavoro EMS — contratto dati».
- **Riferimenti:** ADR-004, ADR-006, ADR-011, ADR-017; `DEC-G2-001` (freeze ICD
  DDP/EDP, APPROVED 2026-09-17); `DEC-G3-001` (profili distinti 912 UL e
  912 ULS, APPROVED 2026-09-22).
- **Baseline:** `main` = `4aea1e4edf8678c180e04171813d842ca6ff58a0`. Lo stack
  EMS aperto (#52/#53/#56/#59, testa `e5ed684`) non tocca nessuno dei file qui
  discussi.
- **Non decide alcun limite motore.** Nessuna soglia di temperatura liquido è
  proposta: il registro motore dichiara le fonti `UNRESOLVED` e `DEC-G3-001`
  non autorizza limiti non documentati.

---

## 1. Contesto

### 1.1 Il canale che non esiste

`config/active/aircraft-profile.json:62` dichiara `COOLANT_TEMPERATURE` come
sensore **`"required": true`**. Entrambi i motori dell'incarico hanno teste
raffreddate a liquido, quindi la dichiarazione è coerente con l'installazione.

Il canale però non esiste in nessuno strato:

| Strato | Stato |
|---|---|
| `include/efis/core/types.hpp:128-147` | il blocco motore di `SignalId` non ha un identificativo per il liquido |
| `include/efis/engine/engine_snapshot.hpp:15-32` | `EngineSnapshot` non ha il campo |
| `src/protocol/edp_v1.cpp:155-186` | `expected_unit` non conosce il segnale, quindi EDP V1 non lo trasporta |
| `config/active/aircraft-profile.json:30-40` | i limiti non hanno una voce per il liquido, benché il sensore sia obbligatorio |

E dove il canale viene richiesto, viene **soddisfatto con un'altra grandezza**:
`src/nodes/ems/ems_controller.cpp:546` e
`src/renderers/qt/pfd_controller.cpp:1069` fanno leggere a
`COOLANT_TEMPERATURE` il campione `oil_temperature_c`. È il caso che ADR-004
descrive come il peggiore: il numero è reale, fresco, con validità piena, e
descrive un'altra cosa.

### 1.2 Il vocabolario che non copre l'installazione

`include/efis/core/types.hpp:151-179` — `Unit` contiene `gallon`,
`gallon_per_hour`, `pound_per_square_inch`, `inch_mercury`, `hectopascal`,
`pascal`. **Non** contiene litro, litro/ora né bar.

Il punto non è la presentazione: `qml/AdaptiveEmsView.qml:153-162` converte già
in metrico, e tenere la misura in un'unità canonica e convertire solo in
presentazione è una scelta difendibile. Il punto è la **provenienza**: un
sensore che misura nativamente in bar, o una sonda di livello tarata in litri,
non può dichiarare la propria unità in `SampleOrigin.unit`. Il campione
arriverebbe marcato `pound_per_square_inch` senza che nessuno abbia convertito
nulla, oppure `unknown`. `src/protocol/edp_v1.cpp:323` rifiuta un campione la
cui unità non coincide con quella attesa per il segnale, quindi oggi la
seconda strada chiude il canale e la prima mente sull'origine.

Il profilo attivo, per inciso, dichiara `units.fuel: "L"` e le capacità in
litri (`:84`, `:15`), mentre l'intera catena EMS è in galloni.

---

## 2. Il costo misurato, non stimato

Questa sezione esiste perché la decisione dipende dal costo, e il costo è
verificabile. Tutti i numeri sono letti o eseguiti sulla baseline.

### 2.1 Il contratto è congelato e il gate funziona

`config/protocols/avionix-data-icd-v1.json` dichiara `EDP_V1` con
`status: FROZEN-SW`, `frozen_utc: 2026-09-17T00:00:00Z` — il freeze approvato
da `DEC-G2-001`. Il blocco dichiara `sample_count: 18`,
`sample_size: 36`, `payload_header_size: 4`, `payload_size: 652`,
`envelope_size: 42`.

`tests/data/protocols/avionix-data-icd-v1-vectors.json` congela il vettore
canonico EDP: `wire_size` **694**, `payload_crc32c` **`376ee061`**,
`wire_sha256` **`d92e0a6e39d15c68b5b00866aa4dfbb35b465c7aa6856d1457710fa0f31c373c`**.

Eseguiti sulla baseline, in sola lettura:

```bash
python3 -m tools.protocol.validate_avionix_icd
```

→ `AVIONIX ICD V1: PASS`, exit `0`.

```bash
python3 -m tools.protocol.validate_data_contract
```

→ `{"contracts": 2, "flows_checked": 3, "source_constants_checked": 10, "status": "PASS"}`, exit `0`.

I due gate sono quindi **attivi e verdi**, e `validate_data_contract` confronta
le costanti delle sorgenti con l'ICD: una modifica del formato che non passi
dalla revisione dell'ICD viene respinta. È il comportamento corretto, e è la
ragione per cui questo documento è una proposta e non una patch.

### 2.2 Che cosa costerebbe un diciannovesimo campione

Il payload è `header_size + sample_count × sample_size`:

| | campioni | payload | datagramma |
|---|---:|---:|---:|
| oggi | 18 | 652 B | 694 B |
| con il liquido | 19 | 688 B | 730 B |

Un canale aggiuntivo costa **36 byte**. Cambierebbero, tutti insieme:
`sample_count` e `payload_size` nell'ICD; `wire_size`, `payload_crc32c` e
`wire_sha256` del vettore congelato; l'esito di
`tests/avionix_icd_vector_tests.cpp`, che ricostruisce i frame canonici e
confronta dimensione, CRC32C e SHA-256; `engine_sample_count` in
`include/efis/protocol/edp_v1.hpp`; `signal_index` e `expected_unit` in
`src/protocol/edp_v1.cpp`.

**Publisher e consumer non sono separabili sotto V1.**
`src/protocol/edp_v1.cpp:284` rifiuta il datagramma se
`count != engine_sample_count`. Un publisher aggiornato e un consumer no — o
viceversa — producono un rifiuto totale, non una degradazione. Poiché il
publisher vive sulla Blackbox e il consumer sull'RPi4 EMS, e `PROD-005`
richiede per ogni release manifesto e rollback propri, un rollout simultaneo
dei due nodi è un vincolo operativo reale, non una formalità.

### 2.3 Che cosa costerebbe estendere il vocabolario unità

Sul filo, **nulla**: l'ICD dichiara il campo `unit` a offset 14, lungo 1 byte,
tipo `u8`. L'enumerazione ha quindi spazio fino a 255 valori senza che la
disposizione del campione cambi, e `types.hpp:168-169` documenta già la regola
giusta — le aggiunte vanno in coda perché gli identificativi già scritti nelle
registrazioni non possono spostarsi.

Il costo non è sul filo ma sul gate:
`tools/protocol/validate_avionix_icd.py:103-104` asserisce che le enumerazioni
della specifica coincidano con un insieme congelato, e l'enumerazione `unit`
congelata ha esattamente venti valori, da `unknown` = 0 a `pascal` = 19.
**Anche una semplice aggiunta in coda fallisce il gate** finché l'ICD non viene
revisionato.

È una distinzione che vale la pena isolare: le due metà di E-04 hanno costi
molto diversi. Aggiungere `litre`, `bar` e `litre_per_hour` costa una revisione
documentale dell'ICD e zero byte. Trasportare una grandezza in quelle unità
costa invece un intervento su `expected_unit`, cioè sulla stessa decisione di
schema del punto 2.2.

---

## 3. Opzioni

### Opzione A — diciannovesimo campione in EDP V1

Si porta `engine_sample_count` da 18 a 19 e si revisiona l'ICD.

*A favore:* riusa tutta la macchina esistente; nessun percorso di decodifica
doppio; un solo formato da verificare.

*Contro:* invalida il vettore congelato e l'evidence bundle G2 approvato da
`DEC-G2-001`; impone il rollout simultaneo di Blackbox e RPi4; ogni futuro
canale ripete lo stesso costo. Il freeze del 17 settembre perde significato
retroattivo, perché il formato «congelato» cambia senza che la versione dica
che è cambiato.

### Opzione B — EDP V2 con conteggio dichiarato (raccomandata)

Si porta `schema_version` da 1 a 2. Il consumer accetta entrambe le versioni.
Il conteggio dei campioni, che l'envelope **già trasporta**
(`src/protocol/edp_v1.cpp:200` lo scrive), diventa autorevole in V2 invece di
dover coincidere con una costante: il consumer accetta i campioni che riconosce
e ignora gli identificativi che non conosce, mantenendo fail-closed il
controllo di unità per ogni segnale noto.

*A favore:* il vettore V1 congelato resta valido e l'evidence G2 non va
rigenerato; Blackbox e RPi4 si aggiornano su calendari separati, che è il modo
in cui i nodi vengono realmente rilasciati; ogni canale futuro non ripete il
costo; la versione dice la verità su che cosa è cambiato.

*Contro:* due percorsi di decodifica da mantenere e verificare; due serie di
vettori; va deciso in modo esplicito che un campione con identificativo
sconosciuto si **ignora** invece di far fallire il datagramma, e quella
tolleranza va verificata con un test di frame ostile, perché è precisamente il
punto in cui un decoder permissivo diventa un rischio.

### Opzione C — riusare uno slot esistente

Per esempio destinare al liquido lo slot `manifold_pressure`, dato che il
profilo dichiara il sensore MAP con stato `OWNER_DECLARED_INDICATIVE`.

**Da scartare.** È la sostituzione plausibile-ma-falsa che ADR-004 vieta, ed è
anche la stessa forma del difetto che E-01 segnala: un numero corretto sotto il
nome di un'altra grandezza. Inoltre la provenienza del sensore MAP su questo
esemplare è irrisolta, quindi la premessa non è nemmeno accertata.

### Opzione D — nessun intervento sul protocollo

Si smette soltanto di soddisfare `COOLANT_TEMPERATURE` con la temperatura
olio: la richiesta viene rifiutata, e il profilo dichiara il canale non
trasportabile finché il contratto non lo prevede.

*A favore:* rimuove oggi la lettura falsa, che è il danno immediato; non tocca
alcun contratto; non richiede questa ADR.

*Contro:* non consegna il canale, e lascia obbligatorio nel profilo un sensore
che il sistema non può leggere.

**Nota di sequenza.** D non è alternativa a B: è il primo passo, e va fatto in
ogni caso. Non è però eseguibile adesso, perché l'alias vive in
`src/nodes/ems/ems_controller.cpp` e `src/renderers/qt/pfd_controller.cpp`,
entrambi modificati dallo stack aperto: appartiene al lotto L1 della sezione 7
della review e attende il merge.

---

## 4. Raccomandazione

**Opzione D subito, come parte di L1, poi Opzione B.**

1. **D, con L1:** rimuovere l'alias e rifiutare la calibrazione di un canale che
   non esiste. Chiude la violazione di ADR-004 senza toccare il contratto.
2. **B, con una ADR accettata:** `schema_version` 2, conteggio dichiarato
   autorevole, consumer che accetta V1 e V2, campioni sconosciuti ignorati con
   un test di frame ostile che lo dimostri.
3. **Vocabolario unità:** aggiungere `litre`, `bar` e `litre_per_hour` **in
   coda** a `Unit` e all'enumerazione congelata dell'ICD, nella stessa
   revisione di B. Serve a far dichiarare a un sensore la propria unità nativa,
   non a cambiare l'unità di trasporto, che resta canonica.
4. **Limite del liquido:** resta fuori. Va acquisito con l'intake motore come
   qualunque altro limite, con documento, revisione e pagina. Finché
   `config/product/engine-source-registry-v1.json` dichiara le fonti
   `UNRESOLVED`, un limite per il liquido non può essere presentato come
   normale, e questo documento non ne propone alcuno.

Il motivo per cui B è preferibile ad A non è il costo in byte, che è
trascurabile: è che A fa cambiare in silenzio un formato dichiarato congelato,
mentre B lo dice nella versione. Dato che il freeze è stato approvato
esplicitamente con `DEC-G2-001`, modificarlo senza cambiare versione
svuoterebbe l'approvazione invece di sostituirla.

---

## 5. Righe proposte per il registro decisioni

Da applicare dall'integratore in `docs/project/AVIONIX_NEXT_DECISION_REGISTER.md`,
se condivise. I testi sono una proposta, non una decisione.

| ID | Gate | Decisione | Raccomandazione | Stato | Effetto dopo approvazione |
|---|---|---|---|---|---|
| DEC-G3-00x | G3/WS6 | Canale temperatura liquido per 912 UL/ULS | introdurre un `SignalId` proprio, il campo in `EngineSnapshot` e lo slot EDP tramite **EDP V2** con conteggio dichiarato; vietato riusare uno slot esistente o la temperatura olio | PROPOSED | abilita schema, vettori V2 e test; non autorizza alcun limite per il liquido né il deployment |
| DEC-G3-00y | G3/WS6 | Vocabolario unità per installazioni metriche | aggiungere `litre`, `bar`, `litre_per_hour` in coda a `Unit` e all'enumerazione ICD congelata, nella stessa revisione di EDP V2 | PROPOSED | un sensore può dichiarare la propria unità nativa; l'unità di trasporto resta canonica |
| DEC-G2-00z | G2 | Revisione del freeze ICD | il freeze `DEC-G2-001` resta valido per V1; V2 nasce come revisione dichiarata con vettori propri, senza rigenerare quelli di V1 | PROPOSED | conserva l'evidence bundle G2 approvato; richiede una seconda serie di vettori |

---

## 6. Verifica richiesta se la proposta viene accettata

Nessuna di queste prove è stata eseguita: la proposta non modifica codice.

**Software.** Vettori canonici V2 con dimensione, CRC32C e SHA-256 propri, e
vettori V1 invariati a dimostrare la compatibilità; `validate_avionix_icd` e
`validate_data_contract` verdi su entrambe le versioni; test di frame ostile
che dimostri che un identificativo sconosciuto viene ignorato e non apre un
percorso permissivo; test di unità che dimostri che un campione con unità non
attesa per il proprio segnale è ancora rifiutato; replay incrociato
publisher V1 → consumer V2 e V2 → V2.

**Hardware, gate separati.** Un canale di temperatura liquido reale non è
qualificato da nessuna prova software: serve una sonda installata, la sua
taratura, e il confronto con uno strumento di riferimento, nella finestra
C-BENCH e con il motore documentato. Nessun limite può essere attivato prima
della review indipendente delle fonti motore, che resta
`NO_GO_PENDING_ENGINE_SOURCE_REVIEW`.

---

## 7. Che cosa questa proposta non fa

Non modifica alcun file. Non introduce, corregge o approva alcun limite
motore. Non dichiara validato alcuno dei due motori. Non autorizza merge,
deployment o modifiche hardware. Non assegna un numero ADR: la numerazione in
`docs/adr/` è una risorsa condivisa, e in esecuzione parallela due filoni
potrebbero rivendicare lo stesso numero. L'integratore promuova questo testo a
`docs/adr/ADR-0NN-...` con il numero che gli compete.
