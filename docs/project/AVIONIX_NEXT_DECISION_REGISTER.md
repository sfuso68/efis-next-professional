# AVIONIX NEXT — decision register

Aggiornamento: 2026-09-21
Stato: registro decisioni; ogni approvazione resta esplicita e tracciata

Questo registro separa le decisioni del proprietario dalle constatazioni
tecniche. Una raccomandazione non modifica repository, servizi, hardware o
configurazioni e non diventa vincolante finché non viene approvata.

| ID | Gate | Decisione | Raccomandazione | Stato | Effetto dopo approvazione |
|---|---|---|---|---|---|
| DEC-G0-001 | G0 | Nome prodotto e alias tecnico | `AVIONIX NEXT` come prodotto; `EFIS NEXT` resta alias tecnico/compatibilità fino a G6 | PENDING_USER | ADR di naming, nessun rename automatico |
| DEC-G0-002 | G0 | Secondo bare repository NAS | applicare i ruoli GitHub/serverha/NAS/Blackbox congelati da ADR-018; conservare le copie legacy senza cancellarle | APPROVED 2026-09-21 | autorità e retention definite; nessuna cancellazione |
| DEC-G0-003 | G0 | Cinque documenti mancanti | acquisire gli originali dal proprietario, calcolare hash e classificarli; vietata la ricostruzione presunta | BLOCKED_EXTERNAL | import controllato in `docs/reference/` o registro esterno |
| DEC-G0-004 | G0/G1 | Nodo ASSO5 e quattro Sequent | inventario fisico/read-only con modello, revisione, DIP, I2C, alimentazione e foto | BLOCKED_HW | abilita G1 senza autorizzare uscite o scritture |
| DEC-G2-001 | G2 | Freeze ICD DDP/EDP ed evidence bundle | approvare il solo freeze documentale definito nel readiness report | APPROVED 2026-09-17 | documento ICD, vector, validator; nessun deployment |

## DEC-G0-001 — naming

La raccomandazione evita un rename prematuro di binari, servizi, namespace,
repository e directory già distribuiti. `AVIONIX NEXT` diventa il nome del
prodotto e della documentazione nuova; `EFIS NEXT` resta l'alias tecnico
esplicito fino alla baseline G6. Un'eventuale migrazione viene poi gestita con
compatibilità e rollback, non con sostituzioni globali.

Risposta sufficiente: `Approvo DEC-G0-001`.

## DEC-G0-002 — repository NAS duplicato

La decisione è chiusa da ADR-018. GitHub è l'autorità condivisa; serverha è il
mirror locale primario e replica `main` e tag nel bare Professional sul NAS.
La copia Blackbox resta temporanea e non autorevole. Il precedente
`/volume1/homes/sfuso/git-repos/efis.git` resta `LEGACY`, intatto e fuori dalla
catena di pubblicazione. Nessuna copia viene cancellata da questa decisione.

## DEC-G0-003 — fonti mancanti

La ricerca nelle branch esaminate, nel mirror locale e in `sources/` non ha
trovato:

1. `CLAUDE_CODE_ASSO5_SENSOR_NODE_HANDOFF_R0.md`;
2. `AVIONIX_NEXT_ASSO5_EMS_Rotax912ULS_P4.docx`;
3. `AVIONIX_NEXT_Linee_Prodotto_Documento_Riferimento_R0.docx`;
4. `EFIS_NEXT_ALLINEAMENTO_CLAUDE.md`;
5. `LSA_Rotax912ULS_Falken_IFM_Impianto_Elettrico_P0.pdf`.

Il gate richiede file originali, non riassunti ricreati. Al ricevimento saranno
registrati nome, dimensione, SHA-256, origine, data e autorità; il contenuto non
sarà automaticamente promosso sopra evidenze hardware o ADR accettate.

## DEC-G0-004 — inventario hardware

Richiede presenza davanti al banco. La ricognizione sarà read-only e non
comprenderà cambi DIP, EEPROM, cablaggi, alimentazioni o attivazione di uscite.
Il comando operativo verrà preparato solo quando il proprietario indica il nodo
fisico e conferma la finestra di prova.

## DEC-G2-001 — freeze documentale

Lo scope e i nove disallineamenti sono in
`AVIONIX_G2_FREEZE_READINESS.md`. L'approvazione autorizza documentazione,
vettori e validator; non autorizza cambi wire, installazioni, rete avionica o
prove hardware.

Risposta sufficiente:
`Confermo il freeze documentale G2 dell'ICD AVIONIX e dell'evidence bundle.`

Decisione ricevuta dal proprietario il 2026-09-17 con la formula sopra. Lo
scope approvato è stato applicato senza modifiche wire, runtime, deployment,
rete o hardware. La baseline risultante è `FROZEN-SW`; la verifica ARM64 e le
prove target restano aperte.

## Regole di chiusura

- Ogni decisione viene chiusa singolarmente con data, autore e riferimento.
- `PENDING_USER` non significa approvata.
- `BLOCKED_EXTERNAL` e `BLOCKED_HW` non diventano `DONE` senza evidenza.
- Nessuna decisione autorizza implicitamente merge, deployment o modifica
  hardware.
