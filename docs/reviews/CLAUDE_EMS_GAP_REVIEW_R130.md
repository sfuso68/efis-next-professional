# Audit indipendente EMS — Rotax 912 UL e 912 ULS — R1-30

- **Autore:** Claude Code, primo incarico parallelo EMS.
- **Tipo:** review indipendente di sola lettura. Nessuna modifica a codice di
  runtime, schemi, protocolli, `tools/simulation/`, CMake, CI, registri
  canonici od output generati.
- **Data:** 2026-10-30; sezione 8 aggiunta il 2026-10-31.
- **Repository:** `sfuso68/efis-next-professional`.
- **Branch di consegna:** `claude/keen-curie-azwny9`.
- **HEAD esatto all'inizio del lavoro:** `4aea1e4edf8678c180e04171813d842ca6ff58a0`
  (merge della PR #49), identico a `main` e a `origin/main` al momento del fetch.
- **Stato del prodotto:** DEVELOPMENT / NON-CERT. Questo documento **non**
  dichiara validato né il prodotto né alcuno dei due motori.

---

## 0. Conflitti di baseline e di contratto — da risolvere prima dell'integrazione

Segnalati e non corretti, come richiesto dall'incarico.

> **Erratum R1-30-A, stesso giorno, commit successivo.** La prima stesura di
> questa sezione dichiarava inesistenti la baseline `9a0171ea` e il piano di
> esecuzione parallela. **Era un errore mio**: a quel punto il checkout aveva
> ricevuto solo `main`, e i due oggetti vivono sullo stack EMS non integrato.
> Dopo il fetch dei branch dello stack entrambi si risolvono. C-1, C-2 e C-5
> sono quindi ritirati come difetti del repository e sostituiti da C-0, che
> descrive il fatto reale. I finding della sezione 4 e gli esiti della sezione 5
> non dipendevano da quell'errore e restano invariati.

| # | Conflitto | Riscontro | Effetto sulla review |
|---|---|---|---|
| C-0 | Il contesto che governa questo incarico è raggiungibile **solo dallo stack non integrato** | `9a0171ea51b5e02602179a15923fbd72e56d2e58` esiste, è antenato di `22de6c3` con **2 commit** di distanza, e **non** è antenato di `main` (`git merge-base --is-ancestor … origin/main` → exit 1). `docs/project/PARALLEL_EXECUTION_PLAN_2026-10-30.md` è aggiunto da `892b393` sullo stack; `docs/project/CLAUDE_EMS_HANDOFF_2026-10-30.md` è lì accanto e corrisponde verbatim al testo dell'incarico | L'handoff prescrive «non assumere che main includa #59», ma il piano e l'handoff stesso **non sono in `main`**: da un checkout del solo `main` le due istruzioni — auditare `main` e leggere il piano — non sono soddisfacibili insieme. Risolto fetchando lo stack in sola lettura: piano e handoff letti integralmente, audit mantenuto su `main` = `4aea1e4`. Riproducibile con la classe `OpenStackRelationship` di `test_ems_limit_authority_r130.py` |
| C-1 | ~~La baseline `9a0171ea` non esiste~~ | **Ritirato.** Vedi C-0: esiste, sullo stack | Nessuno. L'audit resta su `4aea1e4`, dichiarato in testa al documento |
| C-2 | ~~Il piano di esecuzione parallela non esiste~~ | **Ritirato.** Vedi C-0: esiste, sullo stack, e il suo filone B corrisponde a questo incarico | Lettura obbligatoria eseguita |
| C-3 | Il piano cita `EFIS_NEXT_STATE.md` e `EFIS_NEXT_MASTER_MATRIX.md` nella radice | I file sono in `docs/status/`. La radice non li contiene | Risolto senza ambiguità: letti i file in `docs/status/` |
| C-4 | Il piano propone il branch `work/ems-gap-review-r130` | Il contratto di sessione assegna `claude/keen-curie-azwny9` e vieta di spingere altrove | **Conflitto aperto.** Lavorato e consegnato su `claude/keen-curie-azwny9`. La differenza va sanata dall'integratore |
| C-5 | «Il commit che aggiunge questo documento è il checkpoint R1-30» | Corretto: il commit esiste, **sullo stack**, non in `main` | Nessuno, una volta fetchato lo stack |
| C-6 | «PR #59 draft impilata» | Confermato. #59 è draft, head `22de6c37e4a3da517b8903b8abd47e556895373e`, base `fix/engine-channel-validity-v1`. Lo stack EMS aperto è #53 → #56 → #59, oltre a #52. **Nessuno è in `main`** | Rispettata l'istruzione «non assumere che main includa #59». Ogni finding dichiara se è aperto su `main`, sullo stack, o su entrambi |

**C-6 è il conflitto che cambia il significato della review.** Lo stack aperto
chiude per conto proprio una parte dei difetti che un audit del solo `main`
avrebbe elencato come aperti. Per non consegnare finding già risolti, ogni voce
della sezione 4 è stata verificata **due volte**: su `4aea1e4` e su `22de6c3`.

Resta aperto un solo conflitto di contratto, **C-4**, sul nome del branch.

---

## 1. Documenti letti

Letti integralmente: `docs/status/README.md`, `docs/status/EFIS_NEXT_STATE.md`,
`docs/status/EFIS_NEXT_MASTER_MATRIX.md`, `CLAUDE_CODE_HANDOFF_2026-08-21.md`,
`CHECKPOINT-2026-08-21.md`.

ADR: 004 (dato plausibile ma falso), 006 (sezione motore come nodo), 011 (EDP
V1), 012 (confine comandi Emergency Assist), 013 (allocazione RPi4/RPi5), 017
(budget unificato di freshness). Lette inoltre 002, 003, 005, 007, 008 per i
confini citati dalle precedenti.

Specifiche e registri: `docs/design/EMS_PROFESSIONAL_RPI4_V1.md`,
`docs/design/AVIONIX_SKYVIEW_EMS_HMI_V1.md`,
`docs/protocols/EMS_SIMULATION_V1.md`, `docs/deployment/RPI4_EMS.md`,
`config/active/aircraft-profile.json`,
`config/product/engine-source-registry-v1.json`,
`config/product/engine-source-intake-v1.json`,
`config/product/engine-source-review-gate-v1.json`,
`config/ui/avionix-ems-responsive-state-matrix-v1.json`.

Letti dallo stack non integrato, in sola lettura, perché non presenti in `main`
(vedi C-0): `docs/project/PARALLEL_EXECUTION_PLAN_2026-10-30.md` e
`docs/project/CLAUDE_EMS_HANDOFF_2026-10-30.md`. Il piano assegna a questo
lavoro il filone B — «EMS/config/HMI, Claude Code, da avviare dall'utente,
primo incarico ristretto nel handoff collegato, checkout separato, patch/branch
e report, senza deploy/merge» — e il macroblocco M3 con la campagna «parametri
validi/invalidi, cambio unità, stale/loss, allarmi e layout», che coincide con
l'ambito verificato nella sezione 4. Il piano riserva all'integratore
`EFIS_NEXT_STATE.md`, la Master Matrix, il backlog e gli output generati, e
prescrive che gli altri filoni consegnino proposte: è la ragione della forma
della sezione 6. Confrontati inoltre, sullo stack, le righe `EMS-002` ed
`EMS-003` della Master Matrix e i file EMS modificati dalle PR #52/#53/#56/#59.

Gli snapshot storici in `docs/status/archive/` non sono stati usati per dedurre
quale release sia attiva oggi, come prescrive `docs/status/README.md`.

---

## 2. Confini rispettati

Non modificati in nessuna fase: schemi, protocolli, `tools/simulation/`,
`CMakeLists.txt`, workflow CI, registri canonici (`EFIS_NEXT_STATE.md`,
`EFIS_NEXT_MASTER_MATRIX.md`, `config/product/*`, `config/active/*`), output
generati, golden PNG, asset. Nessun accesso a Pixhawk, CAN, I2C, credenziali,
hardware, servizi, release o alimentazione. Nessun deploy, merge, reset,
rebase, force push o pulizia di archivi. Nessun asset proprietario MSFS.

**Review (sezioni 0–7), sola lettura.** File aggiunti, e nient'altro:

```
docs/reviews/CLAUDE_EMS_GAP_REVIEW_R130.md
tests/claude_ems_review/README.md
tests/claude_ems_review/ems_core_behaviour_r130.cpp
tests/claude_ems_review/test_ems_limit_authority_r130.py
tests/claude_ems_review/test_ems_gap_invariants_r130.py
```

**Lotto L0 (sezione 8), autorizzato separatamente il 2026-10-31.** Un solo file
di runtime modificato, assegnato in esclusiva, più il test che lo verifica:

```
qml/EmsSetupOverlay.qml                                        (modificato)
tests/claude_ems_review/test_ems_setup_overlay_contract_r130.py (aggiunto)
```

Il divieto di modificare codice di runtime appartiene al **primo incarico**, che
era già chiuso e consegnato (`0d6852c`, poi `60c02ba`) quando L0 è stato
autorizzato. Fuori da `qml/EmsSetupOverlay.qml` nessun file di runtime è stato
toccato.

I test **non** sono dichiarati in CMake né in CI, perché l'incarico lo vieta:
sono quindi `NOT_RUN` in CI per costruzione e si eseguono a mano con i comandi
della sezione 5.

**Nessun limite motore è stato introdotto, corretto o proposto come
numericamente valido.** Dove il codice contiene valori di limite, questa review
ne registra la presenza e l'assenza di provenienza; non afferma quale sarebbe
il valore corretto per il 912 UL o per il 912 ULS, perché l'unica fonte
ammissibile è il manuale applicabile al motore installato, che
`config/product/engine-source-registry-v1.json` dichiara `UNRESOLVED`.

---

## 3. Inventario della superficie EMS su `4aea1e4`

### 3.1 Nucleo indipendente da Qt

| File | Contenuto |
|---|---|
| `include/efis/engine/engine_snapshot.hpp` | `EngineSnapshot`: 18 `Sample<double>` (rpm, MAP, pressione e temperatura olio, pressione e flusso carburante, due quantità serbatoio, tensione di barra, corrente alternatore, 4 CHT, 4 EGT) + `health` + `injected_fault` |
| `include/efis/core/sample.hpp` | `Sample<T>` con `validity`, `quality`, `source`, `raw`, `sequence`, `utc` e `SampleOrigin` (segnale, nodo, canale, unità, `calibration_revision`, `simulated`) |
| `include/efis/core/types.hpp` | `SignalId` (blocco motore 600–647), `Validity`, `Quality`, `Unit`, `is_usable` |
| `include/efis/core/engine_source.hpp` | `EngineSource`, `EngineArbitrator` (arbitraggio **per grandezza**), `EngineSourceRouter` |
| `include/efis/core/engine_runtime.hpp`, `src/core/engine_runtime.cpp` | `RuntimeTracker`: master / engine / tach / flight hours |
| `include/efis/core/engine_runtime_store.hpp`, `src/core/engine_runtime_store.cpp` | `RuntimeStore`: persistenza con checksum FNV-1a, scrittura atomica e rotazione `.bak` |
| `include/efis/core/derived_state.hpp`, `src/core/derived_state.cpp` | `engine_integration_input`: gate di integrazione carburante e ore |
| `include/efis/display/ems_link_presentation.hpp` | `present_ems_link`: politica di presentazione del collegamento EDP |

### 3.2 Protocollo e trasporto

`include/efis/protocol/edp_v1.hpp`, `src/protocol/edp_v1.cpp`,
`src/providers/edp_udp.cpp`. EDP V1, UDP 49003, envelope di 42 byte, CRC32C,
18 campioni. Ogni campione trasporta **valore, validità, qualità, unità, età,
marcatore di simulazione, nodo, canale, revisione di calibrazione e valore
grezzo** (`src/protocol/edp_v1.cpp:206-214` in scrittura, `:315-336` in
lettura). `expected_unit` (`:155-186`) rifiuta un campione la cui unità non
coincide con quella attesa per il segnale.

### 3.3 Nodo EMS Qt e viste

`src/nodes/ems/ems_controller.{hpp,cpp}` (802 righe), `qml/EmsMain.qml`,
`qml/AdaptiveEmsView.qml` (453), `qml/EmsSetupOverlay.qml` (294),
`qml/EmsGaugeV3.qml`, `qml/HmiArcGaugeV4.qml`, `qml/HmiDataStateBadge.qml`,
`qml/HmiCylinderMatrixV2.qml`, `qml/HmiFuelStatus.qml`,
`qml/HmiElectricalSynoptic.qml`, `qml/HmiAirframePanel.qml`, `qml/HmiTokens.qml`.
Consumatori PFD/Canvas della stessa superficie: `qml/ProfessionalSystemsDeck.qml`,
`qml/EngineTemperatureBars.qml`, `src/renderers/qt/pfd_controller.cpp` (1896).

### 3.4 Copertura per area richiesta

| Area | Presente | Assente o parziale |
|---|---|---|
| **Unità** | `Unit` con rpm, inHg, psi, °C, gal, gph, V, A; conversioni di presentazione AVIATION / METRIC / IMPERIAL in `qml/AdaptiveEmsView.qml:153-162` | Nessun litro, nessun bar, nessun litro/ora nel vocabolario `Unit`; EDP vincola ogni segnale a unità statunitensi. Vedi **E-04** |
| **Canali e validità** | `Sample<double>` completo; EDP li trasporta; `EngineArbitrator` arbitra per grandezza | Su `main` la superficie Qt pubblica solo `.value`. Chiuso dallo stack per i 10 canali scalari, **aperto per gli 8 cilindri**. Vedi **E-10** |
| **Profili** | `config/active/aircraft-profile.json`; registro a due motori UL/ULS; intake e review gate; catalogo operativo a 447 parametri | Limiti `INDICATIVE_UNVERIFIED`; nessuna propagazione a runtime di un cambio profilo; identità motore scritta nel QML. Vedi **E-02**, **E-05**, **E-06** |
| **Carburante** | Integratore con gate, computer con riserva, calibrazione a 5 punti, monitor di perdita, log eventi JSONL | Capacità serbatoio letterale; curva di calibrazione lineare per costruzione e derivata da un dato inserito dal pilota; due formule di autonomia divergenti. Vedi **E-05**, **E-14**, **E-15** |
| **Impianto elettrico** | Tre barre, tre sorgenti, otto carichi dal profilo; sinottico con tensione e corrente; selettore guasti di banco | Lo stato delle barre deriva **solo** dal selettore di banco; tre soglie di tensione diverse, nessuna dal profilo. Vedi **E-11** |
| **Ore motore** | Quattro contatori distinti; store con checksum e rotazione | Configurazione di profilo scartata sul ramo di recupero; `flight_hours` strutturalmente a zero; matricola registrata e mai confrontata. Vedi **E-07** |
| **Log** | `recorder.status`, stato Blackbox con freshness, JSONL eventi carburante, cache Blackbox centrale | Log eventi carburante senza limite di crescita né rotazione. Vedi **E-16** |
| **Calibrazione** | Workflow tipo / ingresso / zero / span, revisione, stati NOT CONFIGURED → CONFIGURED → VERIFIED, distinzione BENCH / LIVE | Guadagno e offset calcolati, salvati e **mai applicati**; una sola calibrazione per quattro cilindri; `sensorCalibrationReady` non vincola nulla. Vedi **E-13** |

---

## 4. Finding

Priorità: **A** = presenta un dato errato o inventato come buono, oppure
contraddice una ADR accettata; **B** = funzione dichiarata e non realizzata, o
incoerenza fra due display; **C** = igiene e manutenibilità.

La colonna «Aperto su» distingue `main` (`4aea1e4`) dallo stack EMS
(`22de6c3`, PR #59).

### Quadro sintetico

| ID | Finding | Priorità | Aperto su | ID matrice |
|---|---|---|---|---|
| E-01 | La temperatura liquido non esiste in nessuno strato ed è alias della temperatura olio | A | entrambi | EMS-002, EMS-004, CFG-001 |
| E-02 | Limiti di ripiego inventati, nessuna convalida nell'EMS, «VERIFIED» coniato da una stringa di 4 caratteri | A | entrambi | EMS-004, CFG-001, PRJ-004 |
| E-03 | La scala MAP è diversa fra EMS e PFD/Canvas | A | entrambi | PRJ-004, EMS-002 |
| E-04 | Litro e bar non esistono nel vocabolario unità; gli archi si spostano cambiando sistema | A | entrambi | CORE-002, EMS-002, CFG-001 |
| E-05 | Capacità serbatoio e identità motore scritte nel codice | A | **parte setup chiusa da L0** (sez. 8); capacità serbatoio aperta su entrambi | CFG-001, EMS-004 |
| E-06 | Un cambio di profilo non aggiorna limiti e identità | B | entrambi | CFG-001, EMS-002 |
| E-07 | Ore motore: configurazione di profilo scartata, ore di volo a zero, matricola non confrontata | B | entrambi | EMS-002, PROD-005 |
| E-08 | Tre soglie diverse per «motore in moto» | B | entrambi | EMS-002, ADR-017 |
| E-09 | La dicitura `SIM FALLBACK` di ADR-011 non raggiunge la pagina EMS | A | entrambi | EMS-002, PRJ-003 |
| E-10 | Validità per canale: chiusa per i 10 scalari dallo stack, **aperta per gli 8 cilindri** | A | cilindri: entrambi; scalari: solo `main` | CORE-002, EMS-002 |
| E-11 | Lo stato dell'impianto elettrico deriva dal selettore di banco | B | entrambi | EMS-002, PWR-001 |
| E-12 | La pressione carburante è trasportata e non presentata da nessuna vista | B | entrambi | EMS-002, EMS-003 |
| E-13 | Guadagno e offset di calibrazione non vengono mai applicati | A | entrambi | EMS-002, CORE-002 |
| E-14 | Due formule di autonomia divergenti; una divide per una portata inventata | B | entrambi (pannello FUEL) | EMS-002 |
| E-15 | Il carburante a bordo viene inizializzato dalla simulazione e persistito | B | solo `main` | EMS-001, EMS-002 |
| E-16 | OAT e QNH inventati nell'intestazione EMS; log eventi senza rotazione; trend saturati a 3000 rpm | A (solo `main`) / B / C | OAT-QNH: solo `main`; resto: entrambi | EMS-002, BBX-002 |

---

### E-01 — La temperatura liquido non esiste, ed è alias della temperatura olio — Priorità A

**File e riga.**
- `config/active/aircraft-profile.json:62` — `COOLANT_TEMPERATURE` è dichiarato
  sensore **`"required": true`**.
- `include/efis/core/types.hpp:128-147` — il blocco motore di `SignalId` non
  contiene alcun segnale di temperatura liquido.
- `include/efis/engine/engine_snapshot.hpp:15-32` — `EngineSnapshot` non ha
  alcun campo per il liquido.
- `src/protocol/edp_v1.cpp:155-186` — `expected_unit` non conosce il segnale,
  quindi EDP V1 non lo può trasportare.
- `src/nodes/ems/ems_controller.cpp:546` (stack: `:558`) e
  `src/renderers/qt/pfd_controller.cpp:1069` — la calibrazione di
  `COOLANT_TEMPERATURE` legge `engine_snapshot_.oil_temperature_c`.
- `src/nodes/ems/ems_controller.cpp:792` (stack: `:816`) — il test di banco
  usa la stessa base numerica per le due grandezze.
- `config/active/aircraft-profile.json:30-40` — i limiti non contengono alcuna
  voce per il liquido, benché il sensore sia richiesto.

**Comportamento atteso.** Un segnale di temperatura liquido proprio, con il
suo identificativo, il suo campo nello snapshot, il suo posto in EDP e il suo
limite nel profilo. Entrambi i motori dell'incarico hanno teste raffreddate a
liquido; il profilo lo riconosce dichiarando il sensore obbligatorio. Finché il
canale non esiste, calibrare o leggere «temperatura liquido» deve essere
rifiutato, non soddisfatto con un'altra grandezza.

**Perché è di priorità A.** ADR-004 vieta di mascherare un'informazione
mancante con un valore plausibile. Qui il valore plausibile è una misura reale
di un'altra grandezza, presentata sotto il nome della grandezza assente: è il
caso peggiore fra quelli che la ADR descrive, perché il numero è corretto, è
fresco, ha validità piena e descrive un'altra cosa.

**Evidenza riproducibile.**
`python3 tests/claude_ems_review/test_ems_gap_invariants_r130.py` → casi
`test_e01a`, `test_e01b`, `test_e01c` FAIL (exit 1).
`python3 tests/claude_ems_review/test_ems_limit_authority_r130.py` → classi
`CoolantChannelAbsence` e `ProfileAndRegistryState` PASS, che fissano lo stato
osservato.

**File minimi da cambiare.** `include/efis/core/types.hpp` (nuovo `SignalId` e
`unit_symbol` invariato), `include/efis/engine/engine_snapshot.hpp`,
`src/protocol/edp_v1.cpp` (`expected_unit`, `signal_index`, `engine_sample_count`),
`src/nodes/ems/ems_controller.cpp`, `src/renderers/qt/pfd_controller.cpp`.
**Avvertenza di ownership:** toccare `engine_sample_count` cambia il formato
EDP e quindi il contratto G2 e i vettori ICD. Richiede una ADR e non può essere
un effetto collaterale di un lavoro EMS.

---

### E-02 — Limiti di ripiego inventati, nessuna convalida, «VERIFIED» da 4 caratteri — Priorità A

**File e riga.**
- `qml/AdaptiveEmsView.qml:27-33` —
  `function engineLimit(name,fallback)` restituisce un valore di ripiego
  quando il limite è assente, non finito o non positivo. I ripieghi sono
  `oil_pressure_min_psi` 25, `oil_pressure_max_psi` 100,
  `oil_temperature_max_c` 120, `cht_max_c` **230**, `egt_max_c` 920,
  `rpm_max` **3000**.
- `qml/ProfessionalSystemsDeck.qml:9,31,37,38,46` — gli stessi ripieghi, con
  `cht_max_c` **240**.
- `qml/EngineTemperatureBars.qml:10` — `chtMaximumC: 240` come valore di
  default della proprietà.
- `src/nodes/ems/ems_controller.cpp:331` — l'EMS copia la mappa dei limiti
  verbatim: `engine_limits_ = engine.value("limits").toObject().toVariantMap();`
  Nessun controllo di intervallo, finitezza o coerenza in tutta
  `loadAircraftProfile` (`:317-376`).
- `src/renderers/qt/pfd_controller.cpp:699-705` — il PFD **sì** convalida gli
  intervalli, e `:707` produce `INVALID_LIMITS`.
- `src/renderers/qt/pfd_controller.cpp:699` e `:730` — `verifyStagedEngine`
  accetta come riferimento al manuale qualunque stringa di almeno 4 caratteri
  e scrive `{"state","VERIFIED_LIMITS"}` con audit `ACCEPTED` e messaggio
  «Engine limits verified».
- `config/product/engine-source-registry-v1.json` — `activation_allowed: false`,
  `review_state: NOT_REVIEWED`, `limits: []`,
  `decision: NO_GO_UNTIL_APPLICABLE_SOURCES_REVIEWED`. La funzione di staging
  **non consulta mai questo registro**: `grep "engine-source-registry"
  src/renderers/qt/pfd_controller.cpp` non produce risultati.
- `config/product/engine-source-review-gate-v1.json` — richiede
  `serial-applicability`, `manual-revision`, `limit-page-traceability`,
  **`ul-uls-separation`**, `legacy-limit-disposition` e tre ruoli di revisione.
  Nessuno di questi è verificato dal percorso di staging.

**Comportamento atteso.** Un limite assente non si sostituisce con un numero:
l'arco non si disegna. Il progetto possiede già la primitiva giusta e non la
usa qui — `include/efis/instruments/dial.hpp:76-88` definisce
`LimitsProvenance::{absent, indicative, verified}` con il commento «un
quadrante senza archi dichiara di non sapere dove siano i limiti, un quadrante
con archi inventati afferma di saperlo», ed è applicata in
`src/apps/render_dials_main.cpp:88-91`. La parola `VERIFIED` deve derivare dal
registro motore con documento, revisione, pagina e firme, non da una stringa
libera.

**Mitigazione esistente, parziale.** `qml/AdaptiveEmsView.qml:25-26,231-234`
mostra un distintivo «LIMITS INDICATIVE» quando
`engineLimits.status !== "VERIFIED"`, e `qml/HmiArcGaugeV4.qml:57-72` aggiunge
un «!» su ogni quadrante. È il comportamento corretto e va conservato. Resta
che il distintivo dichiara non verificato un arco **che viene comunque
disegnato su numeri inventati**: il pilota legge un fondo scala preciso e un
avviso generico. Si aggiunga che nessun percorso del codice può scrivere
`limits.status = "VERIFIED"` nel profilo attivo — `verifyStagedEngine` scrive
`state` al livello del documento e non `limits.status` — quindi oggi il
distintivo non potrebbe spegnersi nemmeno con limiti legittimamente verificati.

**Conseguenza quantificata con profilo assente.** `rpm_max` di ripiego è 3000
mentre il profilo dichiara 5800 (`config/active/aircraft-profile.json:27,32`).
Il quadrante RPM va a fondo scala a 3000 e la fascia verde, derivata come
`rpmMaximum*.24 … rpmMaximum*.95` (`qml/AdaptiveEmsView.qml:216`), finisce a
2850. Un regime di crociera resterebbe fuori scala.

**Evidenza riproducibile.** `test_ems_gap_invariants_r130.py` casi `e02a`–`e02f`
FAIL. `test_ems_limit_authority_r130.py` classi
`LimitFallbacksDivergeBetweenDisplays` e `LimitValidationAsymmetry` PASS; il
caso `test_rpm_fallback_is_far_below_the_declared_maximum` fissa 3000 contro
5800.

**File minimi da cambiare.** `qml/AdaptiveEmsView.qml`,
`qml/ProfessionalSystemsDeck.qml`, `qml/EngineTemperatureBars.qml`,
`qml/HmiArcGaugeV4.qml`, `src/nodes/ems/ems_controller.cpp` (convalida in
`loadAircraftProfile`), `src/renderers/qt/pfd_controller.cpp`
(`verifyStagedEngine` deve leggere il registro motore).

---

### E-03 — La scala MAP è diversa fra EMS e PFD/Canvas — Priorità A

**File e riga.**
- `qml/AdaptiveEmsView.qml:217` — MAP: minimo 0, massimo 40 inHg, verde
  5 → 34 inHg.
- `qml/ProfessionalSystemsDeck.qml:32` — MAP: minimo 10, massimo 35 inHg,
  verde 15 → 29 inHg.

**Comportamento atteso.** ADR-006 fonda il nodo EMS sulla condivisione dei
file QML proprio perché «le due implementazioni sono verificabili l'una contro
l'altra». Due scale diverse per la stessa grandezza rompono quella garanzia.

**Evidenza riproducibile, aritmetica.** Con lo stesso campione:

| MAP | EMS RPi4 | PFD/Canvas RPi5 |
|---:|---|---|
| 30 inHg | dentro il verde (5–34) | **sopra** il verde (15–29) |
| 12 inHg | dentro il verde | **sotto** il verde |
| 5 inHg | sul fondo scala | **fuori scala**, sotto il minimo 10 |

`test_ems_limit_authority_r130.py` classe `ManifoldPressureScalesDiverge` PASS
(fissa i tre casi); `test_ems_gap_invariants_r130.py` casi `e03a`, `e03b` FAIL.

**Nota di provenienza da conservare.** Il profilo dichiara
`manifold_pressure_sensor: true`
(`config/active/aircraft-profile.json:43`) con
`configuration_status: OWNER_DECLARED_INDICATIVE` (`:20`). Se quel sensore non
sia installato sul motore reale non è deducibile da questo repository, e la
review **non** si pronuncia: il registro motore è `UNRESOLVED`. Va risolto
nell'intake, non nel codice.

**File minimi da cambiare.** `qml/AdaptiveEmsView.qml`,
`qml/ProfessionalSystemsDeck.qml`, più un componente comune che tenga la scala
in un solo posto.

---

### E-04 — Litro e bar non esistono; gli archi si spostano cambiando unità — Priorità A

**File e riga.**
- `include/efis/core/types.hpp:151-179` — `Unit` contiene `gallon`,
  `gallon_per_hour`, `pound_per_square_inch`, `inch_mercury`, `hectopascal`,
  `pascal`. **Non** contiene litro, litro/ora né bar.
- `src/protocol/edp_v1.cpp:155-186` — `expected_unit` vincola il flusso a
  `gallon_per_hour`, le quantità a `gallon`, le pressioni a
  `pound_per_square_inch`, il MAP a `inch_mercury`. Il decodificatore
  (`:323`) **rifiuta** un campione con unità diversa.
- `config/active/aircraft-profile.json:125-131` — il profilo dichiara
  `"fuel": "L"`; `:84` dichiara `tank_capacity_l: [40, 40]`; `:15` dichiara
  `fuel_capacity_l: 80`.
- `qml/AdaptiveEmsView.qml:153-162` — la conversione esiste solo in
  presentazione.
- `qml/AdaptiveEmsView.qml:217,223` — gli estremi di scala e la fascia verde
  sono **due letterali indipendenti**, uno imperiale e uno metrico, invece di
  una conversione.

**Comportamento atteso.** Cambiare il sistema di presentazione cambia le cifre
mostrate, non i limiti. Un limite va dichiarato una volta nella sua unità di
origine e convertito; se la deriva è intenzionale per avere tacche metriche
tonde, deve riguardare il passo delle tacche, non la fascia verde.

**Evidenza riproducibile, aritmetica.** Deriva fra il letterale metrico
dichiarato e la conversione esatta del letterale imperiale:

| Arco | Imperiale | Conversione esatta | Letterale metrico | Deriva |
|---|---:|---:|---:|---:|
| MAP massimo | 40 inHg | 1354,56 hPa | 1350 | **4,56 hPa** |
| MAP inizio verde | 5 inHg | 169,32 hPa | 170 | 0,68 hPa |
| MAP fine verde | 34 inHg | 1151,37 hPa | 1150 | 1,37 hPa |
| Flusso massimo | 30 gph | 113,56 L/h | 115 | **1,44 L/h** |
| Flusso inizio verde | 3 gph | 11,36 L/h | 11 | 0,36 L/h |
| Flusso fine verde | 23 gph | 87,06 L/h | 87 | 0,06 L/h |

`test_ems_limit_authority_r130.py` classi `UnitVocabulary` e
`MetricLiteralsDriftFromTheirConversion` PASS; `test_ems_gap_invariants_r130.py`
casi `e04a`, `e04b`, `e04c` FAIL. Il test legge i letterali dal QML, quindi la
deriva si misura sulla sorgente e non su numeri trascritti nel test.

**Nota.** La separazione fra misura e presentazione è corretta e va
conservata. Il problema è il vocabolario `Unit`, che non permette di
rappresentare con provenienza un'installazione misurata in litri e bar, e la
duplicazione dei letterali. L'estensione di `Unit` va fatta **in coda**, come
dice il commento in `types.hpp:168-169`, perché gli identificativi già scritti
nelle registrazioni non possono spostarsi.

**File minimi da cambiare.** `include/efis/core/types.hpp` (aggiunte in coda),
`qml/AdaptiveEmsView.qml`, `qml/ProfessionalSystemsDeck.qml`. L'estensione di
`expected_unit` in `src/protocol/edp_v1.cpp` modifica il contratto EDP e
richiede una ADR.

---

### E-05 — Capacità serbatoio e identità motore scritte nel codice — Priorità A

> **Stato dopo il lotto L0 (sezione 8).** La parte su `qml/EmsSetupOverlay.qml`
> è chiusa: identità, regime e sistemi del velivolo vengono ora dal controller,
> e i campi che il controller non pubblica dichiarano l'assenza. Resta aperta la
> capacità dei serbatoi in `qml/AdaptiveEmsView.qml`, che L0 non possiede. Il
> testo che segue descrive la baseline `4aea1e4`.

**File e riga.**
- `qml/AdaptiveEmsView.qml:255` — `tankCapacity: root.fuel(20)`: capacità di
  20 galloni come letterale. Il profilo dichiara 40 L per serbatoio
  (`config/active/aircraft-profile.json:84`), cioè 10,567 gal: il fondo scala
  dell'indicatore è quasi il doppio della capacità dichiarata.
- `qml/EmsSetupOverlay.qml:57` — la pagina ENGINE INSTALLATION mostra
  `s:"4 CYL • 80 HP • 5800 RPM"` come **letterale**, e
  `(controller.engineProfileName||"ROTAX 912 UL")` come ripiego di identità.
- `qml/EmsSetupOverlay.qml:215` — `"ROTAX 912 UL • MAINTENANCE PROGRAM"`.
- `qml/EmsSetupOverlay.qml:292` — `"PROFILE: ASSO-V-JOLLY-912-UL"`.
- `qml/EmsSetupOverlay.qml:60` — `v:"2 × 40 L"` come letterale.

**Conseguenza diretta sull'incarico.** L'incarico riguarda **entrambi** i
motori. Con un profilo 912 ULS caricato, la pagina di installazione continuerà
a dichiarare «80 HP» e, se il profilo non si carica, a dichiarare «ROTAX 912
UL» con la sua targhetta di potenza. È esattamente la confusione UL/ULS che
`config/product/engine-source-review-gate-v1.json` elenca fra i
`required_findings` con il nome `ul-uls-separation`.

**Caso peggiore collaterale.** `src/nodes/ems/ems_controller.cpp:321-324`
ritorna presto se il file non si apre, e l'identità resta `UNCONFIGURED`:
corretto. Se invece il file si apre ed è **mal formato**, `:325-329` procede e
compone `engine_profile_name_` come `"" + " " + ""`, cioè **un singolo spazio**:
la pagina mostra un campo motore vuoto invece di dichiararsi non configurata, e
il ripiego QML di `EmsSetupOverlay.qml:57` non scatta perché la stringa non è
vuota.

**Comportamento atteso.** Cilindri, potenza, regime massimo, capacità dei
serbatoi e chiave di profilo vengono dal profilo. In assenza di profilo si
dichiara l'assenza; non si nomina un motore.

**Evidenza riproducibile.** `test_ems_limit_authority_r130.py` classe
`HardCodedAircraftFacts` PASS, incluso
`test_declared_capacity_does_not_match_that_literal` che calcola 10,567 gal
contro il letterale 20. `test_ems_gap_invariants_r130.py` casi `e05a`–`e05d`
FAIL.

**File minimi da cambiare.** `qml/EmsSetupOverlay.qml`,
`qml/AdaptiveEmsView.qml`, `src/nodes/ems/ems_controller.cpp`
(`loadAircraftProfile`: rifiutare un documento non analizzabile invece di
proseguire, ed esporre la capacità dei serbatoi).

---

### E-06 — Un cambio di profilo non aggiorna limiti e identità — Priorità B

**File e riga.**
- `src/nodes/ems/ems_controller.hpp:61-64` (stack: `:80-83`) — `engineProfileName`,
  `engineSerialNumber`, `engineLimits` ed `engineCapabilities` sono
  `Q_PROPERTY ... CONSTANT`. Nessun segnale di notifica.
- `src/nodes/ems/ems_controller.hpp:230` — `selectOperationalProfile` è
  `Q_INVOKABLE`: il profilo operativo **si può** cambiare a runtime.
- `src/nodes/ems/ems_controller.cpp:65` — `loadAircraftProfile()` è chiamata
  una sola volta, dal costruttore.

**Comportamento atteso.** Un cambio di profilo motore deve aggiornare limiti,
nome, matricola e capacità, oppure essere rifiutato esplicitamente a runtime.
Con `CONSTANT`, QML non rilegge mai la proprietà: la pagina resta sui limiti e
sull'identità del profilo caricato all'avvio.

**Evidenza riproducibile.** `test_ems_limit_authority_r130.py` classe
`ProfileChangeIsNotPropagated` PASS (verifica la presenza di `CONSTANT` in
tutte e quattro le Q_PROPERTY e la singola chiamata di caricamento);
`test_ems_gap_invariants_r130.py` casi `e06a`, `e06b` FAIL.

**File minimi da cambiare.** `src/nodes/ems/ems_controller.{hpp,cpp}`,
`src/renderers/qt/pfd_controller.hpp` per la proprietà gemella.

---

### E-07 — Ore motore: configurazione scartata, ore di volo a zero, matricola non confrontata — Priorità B

**File e riga.**
- `src/nodes/ems/ems_controller.cpp:65` applica la configurazione del profilo
  (`:334-340`: `engine_run_threshold_rpm`, `tach_reference_rpm`). Subito dopo,
  `:76-77` la **sovrascrive con due letterali** sul ramo di recupero:
  `runtime_tracker_ = RuntimeTracker{{500.0, 5000.0}, saved->totals};`
  Identico sullo stack (`22de6c3:77`).
- `src/core/engine_runtime.cpp:31` — `if (airborne) totals_.flight_hours += hours;`
- `src/nodes/ems/ems_controller.cpp:783` (stack: `:807`) —
  `runtime_tracker_.update(input.accountable_rpm, seconds, false)`: il terzo
  argomento è il letterale `false`. `flightHours` è esposta
  (`ems_controller.hpp:68,162`) e mostrata
  (`qml/EmsSetupOverlay.qml:71-75`).
- `src/nodes/ems/ems_controller.cpp:76` confronta soltanto `profile_id`.
  `:606-607` salva `engine_serial_number_` nel record, e nessuno lo rilegge.

**Comportamento atteso.** Il riferimento tach del profilo vale anche quando
esiste uno stato salvato. Un contatore di ore di volo non si mostra se nessun
ingresso può farlo avanzare. Una matricola registrata si confronta: un motore
sostituito non eredita le ore del precedente, e un cambio di `profile_id` non
azzera in silenzio i contatori.

**Evidenza riproducibile, eseguita.**
`/tmp/ems_core_behaviour_r130` → 29 PASS, exit 0, fra cui:
- `runtime_profile_reference_is_discarded_on_recovery`: con riferimento tach
  5500 un'ora a 5000 rpm vale 0,9091 h di tach-time; con il letterale 5000 vale
  1,0000 h. Divergenza oltre il 9% per ogni ora di funzionamento, su un
  contatore che alimenta il programma di manutenzione
  (`src/nodes/ems/ems_controller.cpp:594`).
- `flight_hours_cannot_accumulate_without_an_airborne_input`: dopo dieci ore a
  regime di crociera `flight_hours` è `0.0`, mentre `engine_hours` è `10.0`; lo
  stesso contatore funziona se riceve `airborne = true`.

`test_ems_gap_invariants_r130.py` casi `e07a`, `e07b`, `e07c` FAIL.

**Nota positiva.** `RuntimeStore` è fail-closed in modo corretto: checksum
FNV-1a, scrittura su file temporaneo, rotazione `.bak`, e una violazione porta
a `INTEGRITY FAULT` che **blocca** anche i salvataggi successivi
(`src/nodes/ems/ems_controller.cpp:83-86,601`). Da conservare.

**File minimi da cambiare.** `src/nodes/ems/ems_controller.cpp` (ordine di
inizializzazione, confronto matricola, ingresso `airborne` o rimozione del
contatore dalla vista), `qml/EmsSetupOverlay.qml`.

---

### E-08 — Tre soglie diverse per «motore in moto» — Priorità B

**File e riga.**
- `config/active/aircraft-profile.json:50` — `engine_run_threshold_rpm: 500`.
- `src/core/derived_state.cpp:18` — `rpm.value > 400.0`, letterale locale, che
  governa l'integrazione del carburante.
- `src/nodes/ems/ems_controller.cpp:300` (stack: `:312`) —
  `engine_snapshot_.rpm.value<400.0`, secondo letterale locale, che governa lo
  stato STANDBY del monitor di perdita.

**Comportamento atteso.** Una sola soglia, dichiarata dal profilo. ADR-017
stabilisce il principio per le soglie di freschezza — «nuove soglie non devono
essere introdotte come letterali locali» — e lo stesso principio vale qui.

**Evidenza riproducibile, eseguita.** `/tmp/ems_core_behaviour_r130`, caso
`fuel_integrates_in_the_window_where_the_engine_is_not_running`: a 450 rpm con
4,8 gph e un secondo di passo, `engine_integration_input` dichiara
`fuel_usable = true` e addebita carburante ai serbatoi, mentre nello stesso
istante `RuntimeTracker` con la soglia di profilo 500 dichiara il motore non in
funzione e lascia `engine_hours` a `0.0`. Il consumo viene registrato, le ore
no.

`test_ems_gap_invariants_r130.py` casi `e08a`, `e08b`, `e08c` FAIL.

**File minimi da cambiare.** `include/efis/core/derived_state.hpp`,
`src/core/derived_state.cpp` (soglia come parametro), `src/nodes/ems/ems_controller.cpp`.

---

### E-09 — La dicitura `SIM FALLBACK` di ADR-011 non raggiunge la pagina EMS — Priorità A

**File e riga.**
- `include/efis/display/ems_link_presentation.hpp:31-34` — la politica produce
  correttamente `engine_state = "SIM FALLBACK • WAITING"` e
  `"SIM FALLBACK • LINK LOST"`, con `data_state = "LOST"` e
  `use_network = false`.
- `src/nodes/ems/ems_controller.cpp:719-721` — i tre campi diventano
  `engineDataState`, `dataSourceState` e `sensorNodeState`.
- `src/nodes/ems/ems_controller.cpp:728-735` — con `use_network = false` lo
  snapshot mostrato è quello del **provider di simulazione**.
- `qml/AdaptiveEmsView.qml` — `dataSourceState` compare **12 volte**;
  `engineDataState` **zero volte**. Verificato anche su `22de6c3`:
  `grep -c engineDataState` → 0, e `SIM FALLBACK` non compare.
- `qml/ProfessionalSystemsDeck.qml:28,64` — il deck PFD/Canvas **sì** mostra
  `"ENGINE • " + engineDataState`, alimentato da
  `qml/AdaptivePfd.qml:83` e `qml/AdaptiveCanvasView.qml:85`.

**Comportamento atteso.** ADR-011 è esplicita: «Dopo `LOST` può mostrare la
simulazione di banco, ma soltanto con la dicitura `SIM FALLBACK • LINK LOST`:
un valore simulato non può presentarsi come reale». La dicitura esiste, è
calcolata correttamente, e **non viene mostrata proprio sul nodo dedicato al
motore**: sull'RPi4 il pilota vede un distintivo rosso «LOST» sopra aghi che si
muovono, perché il provider di simulazione continua ad alimentarli. Sul PFD
RPi5 la stessa condizione è invece etichettata correttamente. La pagina che
ADR-006 definisce condivisa fra i due programmi diverge quindi sul punto che
ADR-011 regola per nome.

**Mitigazione esistente, parziale.** Il distintivo `LOST` è rosso, i quadranti
scendono a opacità 0,34, perdono le fasce normali e prendono un bordo di
avvertimento (`qml/HmiArcGaugeV4.qml:40-50`), e i trend derivati si fermano
(`qml/AdaptiveEmsView.qml:164`). È corretto e va conservato. Ciò che manca è la
distinzione fra «non ho dati» e «ti sto mostrando la simulazione»: il
`config/ui/avionix-ems-responsive-state-matrix-v1.json` congela sei stati
(LIVE, STALE, DEGRADED, LOST, FAIL, SIM) e **non contiene SIM FALLBACK**,
quindi nemmeno le golden congelano questa condizione.

**Evidenza riproducibile, eseguita.** `/tmp/ems_core_behaviour_r130`, caso
`sim_fallback_wording_lives_only_in_the_engine_state_field`: la politica
restituisce la dicitura in `engine_state` e `"LOST"` in `data_state` per
entrambi gli stati `waiting` e `lost`, con `use_network = false`.
`test_ems_gap_invariants_r130.py` casi `e09a`, `e09b` FAIL.

**File minimi da cambiare.** `qml/AdaptiveEmsView.qml` (consumare
`engineDataState`), `qml/HmiDataStateBadge.qml`. L'aggiunta dello stato alla
matrice golden e la rigenerazione delle golden **non** appartengono a questo
lavoro: toccano `config/ui/` e `tests/golden/`, cioè registri e output
generati.

---

### E-10 — Validità per canale: scalari chiusi dallo stack, cilindri ancora aperti — Priorità A

**Stato su `main` (`4aea1e4`).** `src/nodes/ems/ems_controller.cpp:623-644`:
ogni getter restituisce `.value` e nulla più. La validità per canale che
`Sample<double>` porta, che EDP trasporta per tutti e 18 i campioni e che
CORE-002 registra come `DONE`, viene **scartata al confine Qt**. L'unico stato
esposto è quello del collegamento, che è per snapshot e non per canale: un
guasto di un singolo canale — che `include/efis/core/engine_source.hpp:39-45`
definisce «la norma più che l'eccezione» — produce un numero plausibile senza
alcun marcatore. Prima del primo polling lo snapshot è azzerato per
costruzione, quindi la pagina mostra 0 psi e 0 V come letture.

**Stato sullo stack (`22de6c3`).** Chiuso per i dieci canali scalari. Il
controller espone `engineRpmValid`, `manifoldPressureValid`, `oilPressureValid`,
`oilTemperatureValid`, `fuelPressureValid`, `fuelFlowValid`, `fuelLeftValid`,
`fuelRightValid`, `busVoltageValid`, `alternatorCurrentValid` tramite
`efis::engine_channel_usable`, e la vista li consuma con
`channelValid(name)` (`22de6c3:qml/AdaptiveEmsView.qml:23`). Gear e flap
perdono i valori finti (`DOWN & LOCKED` e 15°) e diventano NaN con
`airframeSourceState = "LOST"`. **Questo finding non va riportato come aperto
per gli scalari.**

**Che cosa resta aperto su entrambi.** Gli otto canali dei cilindri. `chtValues`
ed `egtValues` restano `QVariantList` di soli `double`
(`22de6c3:src/nodes/ems/ems_controller.hpp:49`), e il controllo nella vista è
`values.every(v => typeof v === "number" && isFinite(v))`
(`22de6c3:qml/AdaptiveEmsView.qml:26`). Un cilindro la cui validità EDP è
`unavailable`, `out_of_range` o `disagree` ma il cui valore è un numero finito
— l'ultima lettura buona, o zero — passa il controllo e viene presentato come
valido, e contribuisce a `maximumCht`, che pilota la logica CAS «CHT HIGH»
(`qml/AdaptiveEmsView.qml:78,88`).

**Comportamento atteso.** La validità per cilindro raggiunge la vista, e la
matrice dei cilindri distingue un cilindro non disponibile da un cilindro
freddo.

**Evidenza riproducibile.** `/tmp/ems_core_behaviour_r130`, caso
`an_unavailable_channel_blocks_both_integrators`: il nucleo si comporta
correttamente per `unavailable`, `stale`, `out_of_range` e `disagree`, il che
circoscrive il difetto al confine di presentazione e non al nucleo.
`test_ems_gap_invariants_r130.py`: `e10a` FAIL su `main` e **PASS** sullo
stack; `e10b` (`chtValid`) FAIL su entrambi.

**File minimi da cambiare.** `src/nodes/ems/ems_controller.{hpp,cpp}`,
`src/renderers/qt/pfd_controller.{hpp,cpp}`, `qml/HmiCylinderMatrixV2.qml`,
`qml/EngineTemperatureBars.qml`, `qml/AdaptiveEmsView.qml`.

---

### E-11 — Lo stato dell'impianto elettrico deriva dal selettore di banco — Priorità B

**File e riga.**
- `qml/EmsSetupOverlay.qml:27` —
  `function busState(bus)` decide `POWERED`, `LOW VOLTAGE`, `ISOLATED` o
  `BATTERY SUPPLY` **solo** da `controller.electricalTestMode`. Non legge
  `busVoltage`, né alcuna misura.
- `qml/EmsSetupOverlay.qml:152-154` — anche `ONLINE`, `OFFLINE` e `LOW` delle
  sorgenti derivano dallo stesso selettore.
- `src/nodes/ems/ems_controller.cpp:582-585` — `cycleElectricalTestMode()`
  cicla una stringa fra cinque modi di banco.
- Tre soglie di tensione, nessuna dal profilo:
  `qml/AdaptiveEmsView.qml:83,90` → caution sotto 12,5 V;
  `qml/AdaptiveEmsView.qml:146` → checklist 13,2–14,8 V;
  `qml/HmiElectricalSynoptic.qml:15` → `busNormal` 13,2–14,8 V.
  Il profilo dichiara `nominal_voltage_v: 12`
  (`config/active/aircraft-profile.json:89`) e la stringa `nominal_voltage_v`
  non compare in nessuno dei tre file.

**Comportamento atteso.** Con il selettore su `NORMAL` la pagina mostra tutte
le barre `POWERED` e tutte le sorgenti `ONLINE`, anche con tensione di barra
nulla o collegamento `LOST`. Lo stato di una barra deve derivare da una misura
o dichiararsi non misurato; il profilo dichiara tre barre e otto carichi, che
oggi vengono caricati e mai confrontati con un dato.

**Evidenza riproducibile.** `test_ems_limit_authority_r130.py` classe
`ElectricalThresholdsAreUnsourced` PASS; `test_ems_gap_invariants_r130.py`
casi `e11a`, `e11b` FAIL.

**File minimi da cambiare.** `qml/EmsSetupOverlay.qml`,
`qml/HmiElectricalSynoptic.qml`, `qml/AdaptiveEmsView.qml`,
`src/nodes/ems/ems_controller.cpp` per esporre la tensione nominale.

---

### E-12 — La pressione carburante è trasportata e non presentata — Priorità B

**File e riga.** Il canale esiste in ogni strato:
`include/efis/engine/engine_snapshot.hpp:22`,
`src/protocol/edp_v1.cpp:162` (`SignalId::fuel_pressure` → psi),
`src/providers/engine_simulation_provider.cpp:52` (4,6 psi simulati),
`src/nodes/ems/ems_controller.hpp:33,125` (`fuelPressure`).
`grep -l fuelPressure qml/*.qml` → **nessun risultato**.
La catena CAS di `qml/AdaptiveEmsView.qml:79-108` non ha un ramo per la
pressione carburante.

**Conseguenza.** Il guasto iniettabile `low_fuel_pressure`
(`src/providers/engine_simulation_provider.cpp:77-78`) porta la pressione a
1,2 psi e la salute a `warning`. Nessuna vista mostra la pressione, e il CAS
non la annuncia: cambia il colore di salute e la stringa
`engineFaultMode` (`src/nodes/ems/ems_controller.cpp:657`), che però è una
etichetta di iniezione di banco, non un messaggio CAS. Un canale esercitato
end-to-end non è osservabile sul display.

**Comportamento atteso.** Il canale viene presentato con il suo limite, o
dichiarato esplicitamente non presentato nel profilo. Il profilo lo dichiara
`required: false` (`config/active/aircraft-profile.json:65`) e dichiara i suoi
limiti (`:38-39`), quindi oggi la configurazione se lo aspetta.

**Evidenza riproducibile.** `test_ems_limit_authority_r130.py` classe
`FuelPressureHasNoConsumer` PASS; `test_ems_gap_invariants_r130.py` casi
`e12a`, `e12b` FAIL.

**File minimi da cambiare.** `qml/AdaptiveEmsView.qml`,
`qml/ProfessionalSystemsDeck.qml`.

---

### E-13 — Guadagno e offset di calibrazione non vengono mai applicati — Priorità A

**File e riga.**
- `src/nodes/ems/ems_controller.cpp:556-559` — da zero e span si calcolano
  `gain = spanReference/(span-zero)` e `offset = -zero*gain`, e lo stato passa
  a `CONFIGURED`.
- `:563-564` — entrambi vengono persistiti in `QSettings`.
- `:358-359` — entrambi vengono riletti nella mappa di configurazione.
- **Nessun sito li applica a un valore.** I getter (`:623-644`) restituiscono
  `.value` grezzo del campione. L'unico uso di `gain` a destra di una
  moltiplicazione è il calcolo dell'offset nella riga che lo ha prodotto.
  Identico in `src/renderers/qt/pfd_controller.cpp:1077-1078`.
- `:799` — anche il test del sensore assegna `sensor_test_value_ = sensor_test_raw_`:
  il valore «calibrato» del test è il grezzo.
- `:547-548` — per CHT ed EGT la cattura legge solo `cht_c.front()` e
  `egt_c.front()`, e il guadagno è memorizzato **per nome di segnale**: una
  sola calibrazione per quattro cilindri.
- `:463-470` e `qml/EmsSetupOverlay.qml:89` — `sensorCalibrationReady` ha un
  solo consumatore in tutto il QML, ed è una **etichetta**. Non vincola la
  presentazione dei valori, non vincola il CAS, non vincola l'attivazione.

**Comportamento atteso.** Una calibrazione acquisita si applica al valore
indicato, oppure non si chiama calibrazione. `SampleOrigin.calibration_revision`
esiste per sapere «con quale taratura quel numero è stato prodotto»
(`include/efis/core/sample.hpp:28-32`); oggi il nodo incrementa
`calibration_revision_` e non lo attacca a nessun campione — la stringa
`origin.calibration_revision` non compare in `ems_controller.cpp`. Il criterio
di chiusura di **EMS-002** nella matrice è «canali calibrati, integratori
fail-closed, provenienza, health, release e workflow touch verificati»: il
workflow esiste, la calibrazione non agisce.

**Evidenza riproducibile.** `test_ems_limit_authority_r130.py` classe
`CalibrationIsNeverApplied` PASS, con tre casi: il guadagno è calcolato, è
persistito, non è applicato; la revisione non raggiunge alcun campione; il
flag di prontezza ha un solo consumatore QML.
`test_ems_gap_invariants_r130.py` casi `e13a`, `e13b`, `e13c` FAIL.

**File minimi da cambiare.** `src/nodes/ems/ems_controller.cpp`,
`src/renderers/qt/pfd_controller.cpp`. La decisione architetturale su **dove**
applicare la taratura — nel nodo di display o nel publisher, che è il solo che
conosce il valore grezzo — precede l'implementazione ed è materia di ADR,
perché ADR-011 definisce l'EMS un display read-only.

---

### E-14 — Due formule di autonomia divergenti — Priorità B

**File e riga.**
- `qml/AdaptiveEmsView.qml:64` — `fuelEnduranceHours`:
  `fuelUsableGallons/Math.max(.1,controller.fuelFlow)`. Sottrae la riserva,
  corretto, ma con portata nulla divide per 0,1 e restituisce un numero finito
  e grande: con 15 gal utilizzabili mostrerebbe «150.0 HR».
- `qml/AdaptiveEmsView.qml:283` — il pannello FUEL calcola
  `((controller.fuelLeft+controller.fuelRight)/Math.max(.1,controller.fuelFlow))`:
  **non** sottrae la riserva e usa la quantità misurata anziché quella
  rimanente. Due numeri diversi per «autonomia stimata» nella stessa pagina.

**Stato sullo stack.** `22de6c3:qml/AdaptiveEmsView.qml:76` corregge la
proprietà principale: `channelValid("fuelFlow") && controller.fuelFlow>0 ? … : NaN`.
Il pannello FUEL alla riga `309` **conserva** `Math.max(.1,controller.fuelFlow)`
e continua a ignorare la riserva. Il finding resta quindi aperto sulla seconda
formula, su entrambi i rami.

**Comportamento atteso.** Una sola formula, che dichiara l'autonomia non
calcolabile quando la portata non è utilizzabile o è nulla.

**Evidenza riproducibile.** `test_ems_gap_invariants_r130.py` casi `e14a`,
`e14b` FAIL.

**File minimi da cambiare.** `qml/AdaptiveEmsView.qml`.

---

### E-15 — Il carburante a bordo è inizializzato dalla simulazione e persistito — Priorità B

**File e riga.**
- `src/nodes/ems/ems_controller.cpp:756-760` — al primo ciclo, se
  `fuel_on_board_initialized_` è falso, `fuel_on_board_gallons_` viene posto a
  `fuelLeft() + fuelRight()` e **persistito**. All'avvio il collegamento è
  `WAITING` (`:60-62`), quindi la sorgente è il provider di simulazione, che
  fornisce 10,57 gal per serbatoio
  (`src/providers/engine_simulation_provider.cpp:54-55`). Un dato inserito dal
  pilota viene quindi seminato da un valore simulato e scritto nelle
  impostazioni utente.
- `src/nodes/ems/ems_controller.cpp:214,216,234,240` — riserva e carburante a
  bordo sono limitati a `qBound(0.0, …, 100.0)` galloni, senza alcun confronto
  con la capacità dichiarata dal profilo (80 L ≈ 21,1 gal). Si possono
  inserire 100 gal su un velivolo da 21.
- `src/nodes/ems/ems_controller.cpp:37-46,247-256` — la curva di calibrazione
  serbatoio: sotto cinque punti `calibratedFuel` restituisce il grezzo
  (`:38`), presentato come quantità calibrata; e i punti sono costruiti con
  `gallons = capacity*i/4` dove `capacity = fuel_on_board_gallons_/2`
  (`:253-254`). La curva è **lineare per costruzione** e derivata dal numero
  inserito dal pilota: non può rappresentare un serbatoio non lineare, che è
  la ragione per cui si calibra un serbatoio.

**Stato sullo stack.** `22de6c3` corregge la parte peggiore:
`fuelOnBoardGallons` e `fuelRemainingGallons` restituiscono NaN finché
`fuel_on_board_initialized_` è falso, e la vista li tratta come non
disponibili. I limiti a 100 gal e la linearità della curva **restano**.

**Comportamento atteso.** Il carburante a bordo è un dato del pilota: in
assenza si dichiara assente. I limiti di inserimento derivano dalla capacità
del profilo. Una curva di calibrazione registra coppie grezzo/quantità
misurate, non interpolazioni di un totale dichiarato.

**Evidenza riproducibile.** Lettura delle righe citate; nessun test automatico
proposto, perché la verifica richiede il nodo Qt in esecuzione. Marcato
**NOT_RUN** nella sezione 5.

**File minimi da cambiare.** `src/nodes/ems/ems_controller.cpp`.

---

### E-16 — OAT e QNH inventati, log senza rotazione, trend saturati — Priorità B / C

**File e riga.**
- `qml/AdaptiveEmsView.qml:190-191` — l'intestazione del nodo EMS mostra
  `" LOCAL 24 °C OAT 1013 HPA QNH"` come **letterali**, riscritti ogni secondo
  dal timer. Il nodo EMS non ha dati aria: ADR-006 lo definisce un processo che
  non chiede Network né conosce il volo. Due dati di aria inventati presentati
  come misure. **Priorità A su `main`; rimosso sullo stack `22de6c3`** — il
  `grep` di `OAT`, `1013` e `QNH` sulla versione dello stack non produce
  risultati. Riportato per completezza della baseline, non come lavoro da fare.
- `qml/AdaptiveEmsView.qml:165-168` — le normalizzazioni dei trend sono
  letterali indipendenti dai limiti: `engineRpm/3000`, `oilPressure/100`,
  `maximumCht/250`, `busVoltage/16`. Con `rpm_max` dichiarato 5800, il trend
  RPM satura sopra 3000: un regime di crociera resta appiattito al bordo
  superiore del grafico. **Aperto su entrambi.** Priorità B.
- `src/nodes/ems/ems_controller.cpp:268-272` — `recordFuelEvent` scrive in
  append su `state/blackbox-fuel-events.jsonl` senza limite di dimensione né
  rotazione; `loadFuelEventHistory` (`:263-266`) rilegge l'intero file a ogni
  avvio e solo poi lo tronca a 50 voci in memoria. Il monitor di perdita può
  cambiare stato fino a una volta al secondo (`:292`), e ogni transizione
  scrive. Crescita non limitata sul filesystem dell'RPi4. **Aperto su
  entrambi.** Priorità C.

**Evidenza riproducibile.** Lettura delle righe citate. Il letterale del trend
e quello di OAT/QNH sono verificabili con `grep` sui due rami, come indicato.

**File minimi da cambiare.** `qml/AdaptiveEmsView.qml`,
`src/nodes/ems/ems_controller.cpp`.

---

## 5. Esiti dei test — PASS / FAIL / NOT_RUN, software e hardware

### 5.1 Software, eseguito in questa sessione

| Suite | Comando | Esito | Exit code |
|---|---|---|---|
| Nucleo motore, C++ | build: `g++ -std=c++20 -Wall -Wextra -Wpedantic -I include -o /tmp/ems_core_behaviour_r130 tests/claude_ems_review/ems_core_behaviour_r130.cpp src/core/engine_runtime.cpp src/core/derived_state.cpp` poi `/tmp/ems_core_behaviour_r130` | **PASS** — `29 checks, 0 failures`. Build senza alcun warning | `0` |
| Autorità limiti, stato osservato | `python3 tests/claude_ems_review/test_ems_limit_authority_r130.py` | **PASS** — `Ran 51 tests … OK` | `0` |
| Contratto L0, implementazione | `python3 tests/claude_ems_review/test_ems_setup_overlay_contract_r130.py` | **PASS** — `Ran 28 tests … OK`. Aggiunta con la sezione 8 | `0` |
| Invarianti desiderati | `python3 tests/claude_ems_review/test_ems_gap_invariants_r130.py` | **FAIL atteso** — `Ran 39 tests … FAILED (failures=51)` sulla baseline, **48 dopo L0**. Ogni fallimento è l'evidenza di un finding | `1` |

Dipendenze: GCC con C++20 e Python 3 con la sola libreria standard, più `git`
per la sola classe `OpenStackRelationship`, che verifica C-0 e la mappa delle
collisioni della sezione 7 e si limita a uno skip se lo stack non è stato
fetchato. Nessun Qt, nessuna rete, nessuna scrittura su disco, nessun hardware.

### 5.2 Software, NOT_RUN

| Che cosa | Perché |
|---|---|
| CTest del repository | Richiede Qt6, assente in questo ambiente: `qmake6` non presente, nessun pacchetto CMake Qt in `/usr/lib/x86_64-linux-gnu/cmake`. Non è stato tentato alcun aggiramento |
| Le tre suite dell'audit in CI | Per costruzione: l'incarico vieta di modificare CMake e CI, quindi non sono dichiarate |
| Suite Python esistenti del repository | `pytest` assente; non installato, per non alterare l'ambiente |
| Rendering dei quadranti e golden EMS | Richiedono Qt e, per gli strumenti, `eglfs`. Vedi 5.3 |
| E-15 in esecuzione | Richiede il nodo Qt avviato; verificato per lettura |

### 5.3 Hardware — nessuna verifica, per contratto

Nessun accesso a RPi4, RPi5, Blackbox, Pixhawk, CAN, I2C, pannelli,
alimentazione, rete di avionica o release. Di conseguenza **nulla** di questo
documento costituisce acceptance.

ADR-006 stabilisce il limite che vale per ogni finding di presentazione:
con la piattaforma `offscreen` Qt carica il backend software dello scene graph,
che **non disegna i nodi di geometria personalizzati**, e `CircularGaugeItem` è
uno di quelli. Una cattura offscreen prova il layout e non prova gli
strumenti. Quindi le fasce verdi, i fondo scala e i distintivi discussi in
E-02, E-03, E-04 e E-09 **non possono essere chiusi da una cattura sintetica**:
servono `eglfs` e il pannello reale. Restano aperti, come li lascia la matrice,
leggibilità, touch, sunlight/night, frame rate, termica, replay EDP e fault sui
tre display.

Nessun limite motore è stato proposto come valido: l'intake
(`config/product/engine-source-intake-v1.json`) resta `NOT_RUN` e il review
gate resta `NO_GO_PENDING_ENGINE_SOURCE_REVIEW` per entrambi i motori.

### 5.4 Osservazione sull'evidenza golden esistente

Da trattare come finding di integrità dell'evidenza, non come difetto di
runtime, e da indirizzare a chi possiede `config/ui/` e `tests/golden/`.

`qml/HmiGoldenController.qml:18` — la fixture che genera le golden EMS dichiara
`{rpm_max:5800, oil_pressure_min_psi:29, oil_pressure_max_psi:102,
oil_temperature_max_c:130, cht_max_c:135, egt_max_c:900}`.
`config/active/aircraft-profile.json:30-40` — il profilo che il runtime carica
dichiara `oil_pressure_min_psi: 25`, `oil_temperature_max_c: 120`,
`cht_max_c: 230`. Tre limiti su sei divergono; solo `rpm_max` coincide.

Ne segue che le 18 golden congelate per la matrice EMS 6×3 sono state
rasterizzate con una serie di limiti **che il sistema in esecuzione non
carica**. Le golden congelano quindi la semantica degli stati — che è il loro
scopo dichiarato — e **non** attestano gli archi del profilo attivo. Nessuna
delle due serie ha provenienza documentata; questa review non si pronuncia su
quale sia corretta, perché il registro motore è `UNRESOLVED`.

Verificabile con `test_ems_limit_authority_r130.py`, caso
`test_golden_fixture_limits_differ_from_the_active_profile` (PASS).

---

## 6. Proposta testuale di aggiornamento di stato e matrice

**Da applicare a Codex. Questa review non modifica i registri.** Nessuna
variazione di stato e nessuna percentuale nuova sono proposte: solo evidenza
aggiunta alle righe esistenti, come consente `docs/status/README.md` quando lo
stato non cambia.

### 6.1 `docs/status/EFIS_NEXT_MASTER_MATRIX.md`

**Nota di coordinamento.** Lo stack aperto ha già aggiunto evidenza alle righe
`EMS-002` ed `EMS-003` per i lavori N4/N5 e R1-04/N7-N8, fra cui la validità per
canale su PFD ed EMS. Le proposte che seguono sono quindi **additive** a quel
testo e non lo sostituiscono; vanno applicate dopo il merge dello stack, perché
`docs/status/EFIS_NEXT_MASTER_MATRIX.md` e `EFIS_NEXT_STATE.md` sono fra i file
che lo stack modifica.


**EMS-002 — Nodo EMS software. Stato proposto: PARTIAL (invariato).**
Aggiungere all'evidenza: «Audit indipendente R1-30 del 2026-10-30 su `main`
`4aea1e4`, report `docs/reviews/CLAUDE_EMS_GAP_REVIEW_R130.md`, 16 finding
(8 di priorità A). Tre suite isolate in `tests/claude_ems_review/`, non
dichiarate in CMake: nucleo C++ 29/29 PASS, autorità limiti 51/51 PASS,
invarianti desiderati 39 casi / 51 fallimenti attesi. Il criterio "canali
calibrati" **non** è soddisfatto: guadagno e offset sono calcolati, persistiti
e mai applicati a un valore indicato, e `calibration_revision` non raggiunge
alcun campione. La dicitura `SIM FALLBACK • LINK LOST` richiesta da ADR-011 è
calcolata e non mostrata sulla pagina EMS, mentre il deck PFD/Canvas la mostra.
Lo stato delle barre elettriche deriva dal solo selettore di guasti di banco.
`flight_hours` non può avanzare, e il riferimento tach del profilo è scartato
sul ramo di recupero.»
Aggiungere ai rischi: «la validità per canale è chiusa dallo stack #53/#56/#59
per i dieci canali scalari e resta aperta per gli otto canali dei cilindri.»

**EMS-004 — Limiti motore da manuale. Stato proposto: TODO (invariato).**
Aggiungere all'evidenza: «Audit R1-30: i valori di ripiego dei limiti sono
inventati in entrambe le viste e divergono fra loro — `cht_max_c` 230 nel nodo
EMS contro 240 nel deck PFD/Canvas. Il nodo EMS non applica alcuna convalida ai
limiti del profilo, mentre il PFD applica un controllo di intervallo:
asimmetria fra due consumatori dello stesso profilo. `verifyStagedEngine`
scrive `state: VERIFIED_LIMITS` sulla base di un riferimento al manuale di
almeno quattro caratteri e **non consulta**
`config/product/engine-source-registry-v1.json`, quindi nessuno dei cinque
`required_findings` del review gate, `ul-uls-separation` compreso, è imposto dal
codice. Nessun percorso del codice può scrivere `limits.status = VERIFIED` nel
profilo attivo. La temperatura liquido è dichiarata sensore obbligatorio dal
profilo, non esiste in `SignalId`, `EngineSnapshot` né EDP V1, e la sua
calibrazione legge la temperatura olio. `LimitsProvenance` esiste già in
`include/efis/instruments/dial.hpp` e non è usata dalle viste EMS. L'audit non
propone alcun valore di limite: intake `NOT_RUN` e review gate
`NO_GO_PENDING_ENGINE_SOURCE_REVIEW` restano invariati per entrambi i motori.»

**CFG-001 — Catalogo operativo. Stato proposto: PARTIAL (invariato).**
Aggiungere all'evidenza: «Audit R1-30: identità motore, potenza, numero di
cilindri, regime massimo, capacità dei serbatoi e chiave di profilo sono
letterali nel QML (`qml/EmsSetupOverlay.qml:57,60,215,292`;
`qml/AdaptiveEmsView.qml:255`), quindi un profilo 912 ULS continuerebbe a
dichiarare 80 HP. `engineLimits`, `engineProfileName`, `engineSerialNumber` ed
`engineCapabilities` sono `Q_PROPERTY CONSTANT`: un cambio di profilo a runtime
non le aggiorna. Il profilo dichiara il carburante in litri mentre l'intera
catena EMS è in galloni, e il vocabolario `Unit` non contiene litro né bar.»

**CFG-001, aggiunta per il lotto L0.** Aggiungere all'evidenza: «2026-10-31,
lotto L0 su `qml/EmsSetupOverlay.qml` (file assegnato in esclusiva, unico della
superficie EMS non toccato dallo stack): rimossi identità motore, potenza,
numero di cilindri, capacità dei serbatoi, modello del velivolo e chiave di
profilo cablati nel QML. Identità, matricola, regime massimo con il proprio
stato di provenienza, carrello, flap e detent derivano ora dalle proprietà
pubblicate da `EmsController`; i campi che il controller non pubblica
dichiarano `NOT PUBLISHED BY PROFILE` invece di nominare un motore. Un profilo
illeggibile non produce più un campo vuoto. Verifica statica
`tests/claude_ems_review/test_ems_setup_overlay_contract_r130.py` 28/28 PASS;
nessun QML istanziato e nessuna cattura, per assenza di Qt nell'ambiente:
acceptance visiva sul pannello ancora aperta. Cilindri, potenza e capacità dei
serbatoi restano non mostrati perché non sono sulla superficie del controller.»

**PRJ-004 — Gate G3, contratto HMI SkyView/EMS. Stato proposto: VERIFY-HW
(invariato).** Aggiungere all'evidenza: «Audit R1-30: la scala MAP non è la
stessa fra il nodo EMS e il deck PFD/Canvas — 0–40 inHg con verde 5–34 contro
10–35 con verde 15–29 — quindi 30 inHg è dentro il verde su un display e sopra
il verde sull'altro, e 5 inHg è fuori scala sul secondo. Gli archi si spostano
cambiando sistema di unità, perché gli estremi metrici sono letterali
indipendenti anziché conversioni: deriva massima 4,56 hPa sul MAP e 1,44 L/h
sul flusso. La fixture golden `qml/HmiGoldenController.qml:18` usa tre limiti su
sei diversi da quelli del profilo attivo, quindi le 18 golden congelano la
semantica degli stati e non gli archi del profilo caricato a runtime. La
matrice degli stati non contiene `SIM FALLBACK`.»

**CORE-002 — Snapshot, provenance, timestamp, quality, validity. Stato
proposto: DONE (invariato).** Aggiungere all'evidenza: «Audit R1-30: il nucleo
e EDP V1 conservano e trasportano correttamente validità, qualità, unità,
`calibration_revision`, marcatore di simulazione e valore grezzo per tutti e 18
i campioni, e `engine_integration_input` è fail-closed per `unavailable`,
`stale`, `out_of_range` e `disagree` (29/29 PASS). Il difetto osservato è a
valle, al confine di presentazione Qt, e non in questa riga.»

**PROD-005 — Update/rollback software. Stato proposto: DONE (invariato).**
Aggiungere ai rischi: «Audit R1-30: `RuntimeStore` confronta soltanto
`profile_id` e non la matricola, che pure registra. Un motore sostituito con lo
stesso `profile_id` eredita le ore del precedente; un cambio di `profile_id`
azzera i contatori in silenzio. Le ore motore alimentano il programma di
manutenzione.»

**EMS-001 — Simulazione motore. Stato proposto: DONE (invariato).**
Aggiungere ai rischi: «Audit R1-30: su `main` il carburante a bordo, dato del
pilota, veniva seminato dai valori simulati dei serbatoi al primo ciclo e
persistito; lo stack #59 lo corregge restituendo NaN finché non è inizializzato.
I limiti di inserimento restano 0–100 galloni, senza confronto con la capacità
dichiarata di 80 L.»

### 6.2 `docs/status/EFIS_NEXT_STATE.md`

Aggiungere, nella sezione dedicata al nodo EMS, un paragrafo di questo tenore:

«**Audit indipendente EMS R1-30, 2026-10-30.** Review di sola lettura su `main`
`4aea1e4`, in `docs/reviews/CLAUDE_EMS_GAP_REVIEW_R130.md`, con test isolati in
`tests/claude_ems_review/` non dichiarati in CMake. Sedici finding, otto di
priorità A: temperatura liquido inesistente e alias della temperatura olio;
limiti di ripiego inventati e divergenti fra i due display, con `VERIFIED`
coniato da una stringa libera; scala MAP diversa fra EMS e PFD/Canvas;
vocabolario unità senza litro e bar, con archi che si spostano cambiando
sistema; identità e capacità del velivolo scritte nel codice; dicitura
`SIM FALLBACK` di ADR-011 calcolata e non mostrata sulla pagina EMS; validità
per canale assente sugli otto canali dei cilindri; calibrazione mai applicata
al valore indicato. La validità per canale è chiusa dallo stack EMS aperto
#53/#56/#59 per i dieci canali scalari e resta aperta per i cilindri. La
sezione 0 registra i conflitti di baseline e contratto senza correggerli: la
baseline dichiarata `9a0171ea`, il piano di esecuzione parallela e lo stesso
handoff vivono **solo sullo stack non integrato** e non in `main`, quindi da un
checkout del solo `main` l'istruzione di auditare `main` e quella di leggere il
piano non sono soddisfacibili insieme; resta inoltre aperto il disallineamento
fra il branch proposto dal piano e quello assegnato dalla sessione. L'audit non
propone alcun valore di limite motore: intake e review gate restano
rispettivamente `NOT_RUN` e `NO_GO` per il 912 UL e per il 912 ULS, e nessuna
verifica hardware è stata eseguita.»

---

## 7. Proposta di assegnazione esclusiva per l'implementazione successiva

L'incarico, e il piano di esecuzione parallela che lo contiene, stabiliscono che
per attivare il filone B in scrittura servano «checkout/branch separati e una
lista di file esclusivi», **dopo** questa review. Questa sezione propone quella
lista.

### 7.1 Collisione misurata con lo stack aperto

Dato che lo stack #52/#53/#56/#59 non è integrato e tocca largamente la stessa
superficie, la lista di file esclusivi non ha senso senza la mappa delle
collisioni. Misurata con `git diff --name-only main..22de6c3`: su 16 file
candidati, **11 sono già modificati dallo stack e 5 sono liberi**. La mappa è
riprodotta dal caso `test_the_collision_map_of_section_seven_still_holds`, che
fallisce se un file cambia lato.

| File | Stato rispetto allo stack |
|---|---|
| `qml/EmsSetupOverlay.qml` | **libero** |
| `qml/HmiDataStateBadge.qml` | **libero** |
| `include/efis/core/types.hpp` | **libero** |
| `include/efis/engine/engine_snapshot.hpp` | **libero** |
| `src/protocol/edp_v1.cpp` | **libero** |
| `qml/AdaptiveEmsView.qml` | collide |
| `qml/ProfessionalSystemsDeck.qml` | collide |
| `qml/EngineTemperatureBars.qml` | collide |
| `qml/HmiArcGaugeV4.qml` | collide |
| `qml/HmiCylinderMatrixV2.qml` | collide |
| `qml/HmiElectricalSynoptic.qml` | collide |
| `src/nodes/ems/ems_controller.cpp` | collide |
| `src/nodes/ems/ems_controller.hpp` | collide |
| `src/renderers/qt/pfd_controller.cpp` | collide |
| `include/efis/core/derived_state.hpp` | collide |
| `src/core/derived_state.cpp` | collide |

Lo stack introduce inoltre `qml/EmsStatusHeader.qml` e una batteria di test QML
EMS — `tests/qml/tst_engine_channel_validity.qml`,
`tests/qml/tst_ems_engine_state.qml`, `tests/qml/tst_ems_header_layout.qml`,
`tests/qml/tst_airframe_validity.qml` — oltre a
`tests/test_ems_golden_review.py` e `tools/ui/prepare_ems_golden_review.py`.
Un'eventuale implementazione successiva dovrebbe estendere quella batteria,
non aprirne una terza accanto a `tests/claude_ems_review/`.

### 7.2 Lotti proposti

| Lotto | Finding | File | Precondizione |
|---|---|---|---|
| **L0 — identità del velivolo** | E-05 (parte QML) | `qml/EmsSetupOverlay.qml` | **IMPLEMENTATO il 2026-10-31**, vedi sezione 8. Era l'unico lotto EMS interamente su file liberi |
| L1 — autorità e provenienza dei limiti | E-02, E-03, E-04 (presentazione), E-06 | `qml/AdaptiveEmsView.qml`, `qml/ProfessionalSystemsDeck.qml`, `qml/EngineTemperatureBars.qml`, `qml/HmiArcGaugeV4.qml`, `src/nodes/ems/ems_controller.{hpp,cpp}` | **Dopo** il merge dello stack: tutti i file collidono. Non introduce limiti, rimuove i ripieghi e unifica le scale |
| L2 — contatori e soglie | E-07, E-08 | `src/nodes/ems/ems_controller.cpp`, `include/efis/core/derived_state.hpp`, `src/core/derived_state.cpp` | Dopo lo stack; serializzare con L1 e L4 su `ems_controller.cpp` |
| L3 — etichettatura della simulazione e validità cilindri | E-09, E-10 (cilindri) | `qml/AdaptiveEmsView.qml`, `qml/HmiDataStateBadge.qml`, `qml/HmiCylinderMatrixV2.qml`, `src/nodes/ems/ems_controller.hpp` | Dopo lo stack, che ha già riscritto questo confine per i canali scalari |
| L4 — carburante, elettrico, log | E-11, E-12, E-14, E-15 (residui), E-16 (residui) | `src/nodes/ems/ems_controller.cpp`, `qml/HmiElectricalSynoptic.qml`, `qml/AdaptiveEmsView.qml` | Dopo lo stack; serializzare con L1 e L2 |
| Non lavoro EMS — contratto dati | E-01, E-04 (estensione `Unit` e `expected_unit`) | `include/efis/core/types.hpp`, `include/efis/engine/engine_snapshot.hpp`, `src/protocol/edp_v1.cpp` | **Richiede ADR.** Cambia il contratto G2, i vettori ICD e il conteggio dei campioni EDP. I file sono liberi, la decisione non lo è |
| Non lavoro EMS — autorità di verifica | E-02 (registro nel percorso di staging), 5.4 (fixture e golden) | `src/renderers/qt/pfd_controller.cpp`, `config/ui/`, `tests/golden/`, `config/product/` | Registri canonici e output generati: ownership dell'integratore, come stabilisce il piano |

### 7.3 Conseguenza per la sequenza

Quattro dei cinque lotti EMS collidono con lo stack aperto, e
`src/nodes/ems/ems_controller.cpp` è il collo di bottiglia di tre di essi.
Ne segue una sola sequenza priva di conflitti:

1. ~~**ora**, in parallelo allo stack: L0, su file liberi~~ — **fatto**, sezione 8;
2. in parallelo ma fuori dal filone B: l'ADR per il contratto dati (E-01, E-04);
3. **dopo il merge** dello stack: L1, poi L2 e L4 serializzati, poi L3.

Avviare L1–L4 adesso produrrebbe esattamente i conflitti che il piano di
esecuzione parallela esiste per evitare.

---

## 8. Lotto L0 — implementazione del 2026-10-31

Autorizzato dall'utente dopo la consegna della review, con assegnazione in
esclusiva del solo `qml/EmsSetupOverlay.qml`. È l'unico lotto EMS interamente
su file che lo stack #52/#53/#56/#59 non modifica, quindi il solo avviabile
senza attendere il merge.

### 8.1 Che cosa cambia

Il file non nomina più alcun motore e alcun velivolo. Dei sei campi della
pagina ENGINE INSTALLATION, tre vengono ora dal controller e tre dichiarano
l'assenza, perché `EmsController` non pubblica il dato:

| Campo | Prima | Dopo |
|---|---|---|
| ENGINE | `controller.engineProfileName \|\| "ROTAX 912 UL"`, sottotitolo `"4 CYL • 80 HP • 5800 RPM"` | `engineProfileName`, con `rpm_max` dal profilo **seguito dal suo stato di provenienza**; cilindri e potenza dichiarati non pubblicati |
| ENGINE SERIAL | `controller.engineSerialNumber \|\| "TO BE ENTERED"` | `engineSerialNumber` |
| AIRFRAME SYSTEMS | `"RETRACT GEAR • FLAPS"`, detent `"0 / 10 / 15 / 20 / 30°"` | derivati da `landingGearInstalled`, `retractableGear`, `flapsInstalled` e `flapDetents` |
| AIRFRAME | `"ASSO V JOLLY"` / `"PIONEER 300 EQUIVALENT"` | `NOT PUBLISHED BY PROFILE` |
| CONFIGURATION | `"OWNER BUILD"` | `NOT PUBLISHED BY PROFILE` |
| FUEL | `"2 × 40 L"` | `NOT PUBLISHED BY PROFILE` |

Inoltre: l'intestazione del programma di manutenzione usa `engineProfileName`
invece di `"ROTAX 912 UL • MAINTENANCE PROGRAM"`; la nota di manutenzione non
assume più il costruttore e resta **più corta** dell'originale (123 caratteri
contro 129); il piede mostra `OPERATIONAL PROFILE: ` più
`operationalSelectedProfile`, che è il profilo che il controller pubblica
davvero, invece del letterale `"PROFILE: ASSO-V-JOLLY-912-UL"` — il
`profile_id` del profilo attivo non è sulla superficie Qt, e i due non sono la
stessa cosa.

### 8.2 Due scelte che vale la pena motivare

**Un profilo illeggibile non produce più un campo vuoto.** `loadAircraftProfile`
compone il nome come `manufacturer + " " + model`: su un JSON non analizzabile
entrambi sono vuoti e il nome diventa **un singolo spazio**, che il vecchio
ripiego `|| "ROTAX 912 UL"` non intercettava perché la stringa non è vuota. Il
nuovo `textOr` fa `trim()` e dichiara l'assenza. È il caso peggiore registrato
in E-05, chiuso.

**Il regime massimo non si mostra mai nudo.** `engineRating()` affianca a
`rpm_max` il valore di `limits.status`, che oggi è `INDICATIVE_UNVERIFIED`: il
numero resta leggibile e porta con sé la propria provenienza. È la parte di
E-02 raggiungibile da questo lotto senza toccare i quadranti, che collidono.

**Nessuna etichetta non vincolata è stata introdotta.** L'audit stesso segnala
l'overflow delle etichette, quindi il testo più lungo prodotto da L0 è stato
accorciato a 64 caratteri e il delegate della griglia ha ricevuto `width` ed
`elide`, che prima non aveva su nessuno dei due campi di testo.

### 8.3 Che cosa L0 non chiude

Cilindri, potenza, capacità dei serbatoi, modello e variante del velivolo
**restano non mostrati**, perché `EmsController` non li pubblica: aggiungerli
richiede `src/nodes/ems/ems_controller.{hpp,cpp}`, che appartiene a L1 e
collide con lo stack. L0 ha quindi chiuso la metà di E-05 che conta per
ADR-004 — smettere di affermare un'identità che il nodo non conosce — e ha
lasciato aperta la metà che richiede di pubblicare il dato vero. Anche
`tankCapacity: root.fuel(20)` in `qml/AdaptiveEmsView.qml` resta invariato.

### 8.4 Verifica

Nessun Qt in questo ambiente, quindi il QML non è stato istanziato e **non
esiste alcuna cattura**: la verifica è statica e dichiarata tale.

`python3 tests/claude_ems_review/test_ems_setup_overlay_contract_r130.py` →
**PASS**, `Ran 28 tests … OK`, exit `0`. Copre: assenza dei letterali vietati e
di ogni nome di motore nel testo mostrato; provenienza del regime massimo;
derivazione di carrello, flap e detent dalle proprietà pubblicate; `trim()` sul
nome; vincolo `width`/`elide` sulle etichette introdotte; bilanciamento dei
delimitatori; e **risoluzione di ogni riferimento `controller.*` contro le
`Q_PROPERTY` e le `Q_INVOKABLE` di `ems_controller.hpp`**, che è il solo modo di
accorgersi senza Qt di un riferimento a una proprietà inesistente.

Effetto sulle altre suite: `test_ems_gap_invariants_r130.py` passa da 51 a **48**
fallimenti, perché `e05b`, `e05c` ed `e05d` diventano verdi; `e05a` resta rosso
per la capacità dei serbatoi. `test_ems_limit_authority_r130.py` resta a 51 casi
PASS, con il caso sui letterali del setup riscritto per registrare la chiusura.

Contratto altrui non rotto: `tests/test_avionix_hmi_contract.py::test_ems_setup_exposes_read_only_edp_diagnostics`
dipende da sedici metriche EDP e dalla stringa `"DATA LINK"` in questo file;
tutte e diciassette sono state verificate presenti dopo la modifica, ed è un
caso del test di L0. La suite nativa non è eseguibile qui per assenza di Qt:
resta **NOT_RUN**.

### 8.5 Confini rispettati da L0

Un solo file di runtime, `qml/EmsSetupOverlay.qml`, 36 righe aggiunte e 9
rimosse. Nessun tocco a `ems_controller`, schemi, protocolli, CMake, CI,
registri canonici, golden od output generati. Nessun limite motore introdotto o
modificato. La riga della matrice che ne deriva è nella sezione 6 come proposta
testuale, non applicata.

---

## 9. Dichiarazione finale

Il prodotto resta DEVELOPMENT / NON-CERT. Né il Rotax 912 UL né il Rotax 912
ULS risultano validati da questo lavoro, e nessun limite motore è stato
introdotto, corretto o approvato. Tutti gli esiti di questa review e del lotto
L0 sono software e documentali; nessuna verifica hardware è stata eseguita,
nessun QML è stato istanziato e nessuna cattura è stata prodotta. Una cattura
sintetica non sostituirebbe comunque l'acceptance sul display reale, e il lotto
L0 cambia testo mostrato su un pannello: la sua acceptance visiva resta aperta
e appartiene alla campagna C-BENCH.
