# FUSO AVIONICS NEXT — indice documentale e regole di autorità

Aggiornamento: 2026-09-22
Stato: baseline documentale WS0/G0, DEVELOPMENT / NON-CERT

## Repository autorevole

Definizione simulatore corrente: `docs/development/SIMULATOR_MASTER_SPEC_V1.md`
e `config/simulation/development-plan-v1.json`, aggiornati R1-21 del2026-10-26.
Dodici pacchetti,quattordici domini e sedici scenari minimi; specifica di
sviluppo, non evidenza di acceptance o autorizzazione hardware. Prevale sulle
descrizioni dimostrative di SYSTEM-SIMULATOR per gli obiettivi SIL, senza
derogare ad ADR/stato/matrice o sostituire i contratti wire congelati.

Il prodotto **FUSO AVIONICS NEXT**, con alias tecnico `EFIS NEXT`, è sviluppato in
`sfuso68/efis-next-professional`. Il repository `sfuso68/efis-evolved` resta
storico e non è una base di merge. Il Master Brief R0 è stato importato dal
repository storico senza importarne la genealogia.

ADR-018 definisce la replica: GitHub è l'autorità condivisa; serverha ospita il
mirror locale primario e la working copy Claude; il NAS riceve `main` e tag
come archivio; la Blackbox conserva una copia temporanea non autorevole. Il
precedente bare NAS `git-repos/efis.git` è `LEGACY` e resta intatto.

## Ordine di autorità

In caso di conflitto si applica questo ordine:

1. evidenza corrente verificata su hardware o prodotta da test ripetibile;
2. `docs/status/EFIS_NEXT_STATE.md` e
   `docs/status/EFIS_NEXT_MASTER_MATRIX.md`;
3. ADR accettate, in particolare ADR-010, ADR-013 e ADR-015;
4. Master Project Brief R0 come piano e specifica di programma;
5. checkpoint, handoff e documenti storici;
6. codice e documentazione del repository storico.

Il Master Brief non può sostituire automaticamente una decisione ADR già
accettata o un'evidenza hardware più recente. Il conflitto deve essere chiuso
con una nuova ADR o con una revisione esplicita del brief.

## Documenti principali presenti

| Documento | Ruolo | Stato |
|---|---|---|
| `AVIONIX_NEXT_MASTER_PROJECT_BRIEF_CODEX_R0.md` | specifica e piano generale AVIONIX Next | importato integralmente |
| `AVIONIX_NEXT_PROJECT_STATE_AND_PLAN_2026-09-22.md` | quadro direzionale corrente: architettura, gate, workstream, rischi, attività interattive e planning | corrente alla baseline `main` `61070b8`; PR #29/C12 registrata come integrata, verifica hardware aperta |
| `AVIONIX_NEXT_PROGRESS_VS_PLAN_2026-09-20.md` | quadro direzionale dettagliato di avanzamento, scostamenti e piano alla data indicata | snapshot storico; banner di rinvio allo stato generato corrente |
| `AVIONIX_NEXT_PROJECT_AUDIT.md` | audit completo sezione 24 e proposta successiva | aggiornato all'HEAD del branch di lavoro |
| `AVIONIX_G2_FREEZE_READINESS.md` | confronto read-only di contratti, codice e test DDP/EDP | storico; superato dal freeze approvato |
| `docs/protocols/AVIONIX_DATA_ICD_V1.md` | contratto sovraordinato DDP/EDP, vettori ed evidence bundle | FROZEN-SW; ARM64/HW aperti |
| `AVIONIX_G3_HMI_BATCH20.md` | venti controlli fail-closed del contratto SkyView/EMS | IMPLEMENTED-SW; hardware aperto |
| `AVIONIX_G3_SKYVIEW_GOLDEN_BATCH20.md` | bundle golden V2 EMS/SkyView e venti controlli di integrità | VERIFIED-SW; hardware aperto |
| `AVIONIX_G3_SKYVIEW_RESPONSIVE_BATCH20.md` | venti controlli responsive e correzione overlay meteo SkyView | VERIFIED-SW; hardware aperto |
| `AVIONIX_G3_EMS_STATE_BATCH20.md` | sei stati EMS compact e venti controlli fail-closed | VERIFIED-SW; hardware aperto |
| `AVIONIX_G3_EMS_RESPONSIVE_BATCH20.md` | matrice EMS 6×3 e venti controlli multi-risoluzione | VERIFIED-SW; hardware aperto |
| `AVIONIX_G3_HMI_HARDWARE_ACCEPTANCE_BATCH20.md` | harness e venti controlli per acceptance PFD/MFD/EMS | PREPARED-SW/NOT_RUN; hardware aperto |
| `AVIONIX_G3_HMI_EVIDENCE_PACKAGER_BATCH20.md` | assemblatore immutabile e venti controlli evidence HMI | VERIFIED-SW/NOT_RUN; hardware aperto |
| `docs/verification/SYNTHETIC_VISION_BENCHMARK_V1.md` | contratto C9 per rotta, soglie ed evidence terrain | PREPARED-SW/NOT_RUN; hardware aperto |
| `docs/verification/SYNTHETIC_VISION_SESSION_V1.md` | session manager C10 PFD+MFD per acquisizione C9 | VERIFIED-SW/NOT_RUN; hardware aperto |
| `docs/verification/SYNTHETIC_VISION_CAPTURE_ASSEMBLER_V1.md` | assembler C11 di log, immagini, fault e firma per ciascun ruolo C10 | VERIFIED-SW/NOT_RUN; hardware aperto |
| `AVIONIX_G1_SWITCH_ACCEPTANCE_BATCH20.md` | venti controlli per accettazione al banco del candidato MokerLink | PREPARED-SW/NOT_RUN; hardware aperto |
| `docs/deployment/MOKERLINK_G1_ACCEPTANCE.md` | runbook non esecutivo e requisiti evidence switch | PREPARED; richiede autorizzazione separata |
| `AVIONIX_NEXT_DECISION_REGISTER.md` | decisioni G0/G1/G2 separate da evidenze e implementazione | DEC-G2-001 approvata; altre aperte |
| `AVIONIX_NEXT_EXECUTION_BACKLOG.md` | attività ordinate per gate e registro evidenze | controllato |
| `docs/status/EFIS_NEXT_STATE.md` | stato verificato e livelli di evidenza | autorevole |
| `docs/status/EFIS_NEXT_MASTER_MATRIX.md` | requisiti, stato e criteri di chiusura | autorevole |
| `docs/adr/ADR-001` … `ADR-018` | decisioni architetturali | vigenti secondo stato ADR |
| `CLAUDE_CODE_HANDOFF_2026-08-21.md` | handoff storico | riferimento |
| `CHECKPOINT-*` | evidenze e checkpoint cronologici | riferimento |

Il checkpoint `docs/status/CHECKPOINT-2026-09-17-G2-ARM64-PENDING.md` conserva
lo stato precedente all'esecuzione. L'esito successivo e autorevole è
`docs/status/CHECKPOINT-2026-09-18-G2-ARM64-VERIFIED.md`: PFD, MFD ed EMS hanno
superato validator e 11/11 test con digest coerenti. La verticale live resta
esclusa e aperta.

## Riferimenti richiesti dal brief ma non reperiti

I seguenti file non risultano nelle branch Git esaminate né nell'home NAS
ispezionata. Non possono essere considerati acquisiti:

- `CLAUDE_CODE_ASSO5_SENSOR_NODE_HANDOFF_R0.md`;
- `AVIONIX_NEXT_ASSO5_EMS_Rotax912ULS_P4.docx`;
- `AVIONIX_NEXT_Linee_Prodotto_Documento_Riferimento_R0.docx`;
- `EFIS_NEXT_ALLINEAMENTO_CLAUDE.md`;
- `LSA_Rotax912ULS_Falken_IFM_Impianto_Elettrico_P0.pdf`.

Il loro recupero è una dipendenza di G0 e, per i documenti ASSO5/EMS, di G1
e G3. Nessun dato tecnico contenuto soltanto in questi documenti può essere
presunto.

## Registro conflitti iniziale

| ID | Conflitto | Decisione operativa corrente | Chiusura richiesta |
|---|---|---|---|
| C-001 | Il brief era solo in `efis-evolved` | copia integrale nel repo Professional; nessun merge di genealogie | chiuso da questa baseline |
| C-002 | Rete preliminare del brief vs segmentazione reale | prevalgono ADR-010/015: FLIGHT-DATA `10.77.10.0/24`, VIDEO `10.77.20.0/24`, nessun bridge/NAT/routing | revisione R1 del brief |
| C-003 | Nodo `asso5-sensor` `.89` vs EMS reale `.36` | sono ruoli distinti; non si presume che `.36` sia il nodo ASSO5 | identificare hardware dedicato |
| C-004 | EMS e VIDEO-CORE potenzialmente confusi | prevale ADR-013: due RPi4 fisicamente distinti | acquisire/identificare VIDEO-CORE |
| C-005 | Blackbox RPi4 nel testo programmatico vs RPi5 reale | resta RPi5 `efisblackbox` finché una nuova ADR non dimostra il contrario | revisione architetturale formale |
| C-006 | Waveshare 5P come rete complessiva | ammesso solo nel banco FLIGHT-DATA minimo; non sufficiente al prodotto finale | selezione switch finale e VIDEO |
| C-007 | `sv3d-fondamenta` vs renderer Professional | il branch storico è sorgente di requisiti/algoritmi, non base di merge | audit selettivo WS4 |
| C-008 | Nome AVIONIX NEXT vs EFIS NEXT | chiuso il 2026-09-22: prodotto `FUSO AVIONICS NEXT`, alias tecnico `EFIS NEXT`; nessun rename automatico di servizi/repository | migrazione tecnica futura solo con compatibilità e rollback |
| C-009 | Copie Git multiple tra serverha, Blackbox e NAS | chiuso da ADR-018: GitHub autorità condivisa, serverha mirror primario, bare Professional NAS archivio, Blackbox temporanea, `git-repos/efis.git` legacy | restore drill e futuro ritiro controllato della copia Blackbox |
| C-010 | STATE indicava un ID release EMS stale, diverso dal registro canonico e dal bundle hardware | chiuso: prevalgono `EFIS_NEXT_ACTIVE_RELEASES.md`, `EMS-002` e bundle `release-immutability-20260920`; ID corretto `ems-7655799-primary-v1` | nessuna: riconciliazione documentale, stato `EMS-002` invariato `PARTIAL` |

## Evidenza dell'audit R0

- GitHub Professional `main`: `d9231f4b00f3821da530553925166569861dd435`;
- NAS `professional/main`: stesso commit;
- branch storico `sv3d-fondamenta`: `62d7be0eb4a0c3eb393a7314e335b24d59afc1c6`;
- PFD, MFD, EMS, Blackbox, server, portatile e NAS raggiunti in SSH in sola
  lettura il 2026-09-16;
- Pixhawk `10.77.10.3` raggiunto dalla Blackbox;
- generatore stato `--check`: PASS;
- nessuna modifica software o hardware eseguita durante l'audit.

## Riesame operativo 2026-09-17

L'audit completo richiesto dalla sezione 24 è ora conservato in
`AVIONIX_NEXT_PROJECT_AUDIT.md`. Il riesame non sostituisce l'evidenza hardware
del 16 settembre: aggiorna repository, branch, divergenza, component map,
workstream, gate e attività software successive. L'HEAD `dfb3510` ha superato
EFIS Project Control #85 ed EFIS Core CI #69. Non è stato autorizzato alcun
merge, deployment o nuovo intervento hardware.

## Riesame operativo 2026-09-20

Il confronto aggiornato fra Master Brief, gate G0–G7, workstream WS0–WS12,
Master Matrix e nodi osservati è in
`AVIONIX_NEXT_PROGRESS_VS_PLAN_2026-09-20.md`. La baseline integrata resta
`origin/main` `67aee2e`. Il branch `work/release-supervision-contract-v1`
`8dc7895`, proposto dalla PR #15, aggiunge contratti e preflight non esecutivi
per le release immutabili. La PR non è ancora parte di `main` e non prova alcun
deployment sui nodi.

## Contratto dati offline AVIONIX

Il contratto offline `AVIONIX_DATA_CONTRACT_V1.md`, il manifesto macchina e il
validatore verificano la coerenza fra DDP/EDP, piano di rete e costanti sorgente.
Il checkpoint `CHECKPOINT-2026-09-16-AVIONIX-OFFLINE-GATES.md` conserva i test
fail-closed. Questa evidenza è esclusivamente software: non sostituisce la prova
verticale ARM64 né autorizza modifiche hardware o deployment.

## Topologia Git verificata 2026-09-21

ADR-018 chiude AVX-G0-02 e AVX-G0-03. I quattro riferimenti `main` letti su
GitHub, serverha, Blackbox e NAS coincidevano a
`5891f754ee06948349f711c57c90e8700137d09f`. Il timer serverha→NAS era attivo,
l'ultimo servizio oneshot era concluso con successo e la chiave NAS dedicata
ha rifiutato l'accesso shell. Il limite OpenSSH del DS214, il restore drill e
il ritiro della copia Blackbox restano rischi/attività espliciti, non nascosti.

## Quadro direzionale corrente 2026-09-22

`AVIONIX_NEXT_PROJECT_STATE_AND_PLAN_2026-09-22.md` consolida lo stato corrente
senza sostituire Master Matrix, registro release o evidence. Il rapporto usa la
baseline integrata `61070b8`, registra C12/PR #29 come integrata e
distingue esplicitamente attività software, verifiche hardware e prove che
richiedono l'operatore. Il precedente rapporto del 2026-09-20 resta uno
snapshot storico e non deve essere usato per dedurre release o percentuali
correnti.

## Synthetic Vision C12 — 2026-09-22

`docs/verification/SYNTHETIC_VISION_LOD_PROJECTION_V1.md` e
`docs/status/CHECKPOINT-2026-09-22-SYNTHETIC-VISION-LOD-PROJECTION.md`
documentano la correzione software del LOD al confine N45/N46. L'evidenza è
`VERIFIED_SW / VERIFY_HW`: non sostituisce il benchmark C9 su PFD e MFD reali.
