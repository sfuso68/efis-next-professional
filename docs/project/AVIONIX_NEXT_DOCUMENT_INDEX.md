# AVIONIX NEXT — indice documentale e regole di autorità

Aggiornamento: 2026-09-17
Stato: baseline documentale WS0/G0, DEVELOPMENT / NON-CERT

## Repository autorevole

Il software AVIONIX/EFIS Next è sviluppato in
`sfuso68/efis-next-professional`. Il repository `sfuso68/efis-evolved` resta
storico e non è una base di merge. Il Master Brief R0 è stato importato dal
repository storico senza importarne la genealogia.

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
| `AVIONIX_NEXT_PROJECT_AUDIT.md` | audit completo sezione 24 e proposta successiva | aggiornato all'HEAD del branch di lavoro |
| `AVIONIX_G2_FREEZE_READINESS.md` | confronto read-only di contratti, codice e test DDP/EDP | storico; superato dal freeze approvato |
| `docs/protocols/AVIONIX_DATA_ICD_V1.md` | contratto sovraordinato DDP/EDP, vettori ed evidence bundle | FROZEN-SW; ARM64/HW aperti |
| `AVIONIX_G3_HMI_BATCH20.md` | venti controlli fail-closed del contratto SkyView/EMS | IMPLEMENTED-SW; hardware aperto |
| `AVIONIX_G3_SKYVIEW_GOLDEN_BATCH20.md` | bundle golden V2 EMS/SkyView e venti controlli di integrità | VERIFIED-SW; hardware aperto |
| `AVIONIX_G3_SKYVIEW_RESPONSIVE_BATCH20.md` | venti controlli responsive e correzione overlay meteo SkyView | VERIFIED-SW; hardware aperto |
| `AVIONIX_G3_EMS_STATE_BATCH20.md` | sei stati EMS compact e venti controlli fail-closed | VERIFIED-SW; hardware aperto |
| `AVIONIX_NEXT_DECISION_REGISTER.md` | decisioni G0/G1/G2 separate da evidenze e implementazione | DEC-G2-001 approvata; altre aperte |
| `AVIONIX_NEXT_EXECUTION_BACKLOG.md` | attività ordinate per gate e registro evidenze | controllato |
| `docs/status/EFIS_NEXT_STATE.md` | stato verificato e livelli di evidenza | autorevole |
| `docs/status/EFIS_NEXT_MASTER_MATRIX.md` | requisiti, stato e criteri di chiusura | autorevole |
| `docs/adr/ADR-001` … `ADR-015` | decisioni architetturali | vigenti secondo stato ADR |
| `CLAUDE_CODE_HANDOFF_2026-08-21.md` | handoff storico | riferimento |
| `CHECKPOINT-*` | evidenze e checkpoint cronologici | riferimento |

Il checkpoint `docs/status/CHECKPOINT-2026-09-17-G2-ARM64-PENDING.md`
registra l'autorizzazione ARM64 senza trasformarla in evidenza: i risultati dei
tre nodi sono ancora pendenti.

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
| C-008 | Nome AVIONIX NEXT vs EFIS NEXT | nessun rename automatico di servizi/repository | decisione prodotto dell'utente |
| C-009 | Due bare repository sul NAS | nessuna cancellazione; `git-repos/efis.git` è il backup ufficiale corrente | decisione di conservazione/rimozione |

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
