# Checkpoint — Project Control fail-closed P1-P3

Data: 2026-09-19

Branch: `safety/project-control-fail-closed-v1`

ID: `PRJ-001`

## Chiusura software

- Stati illegali rifiutati, senza alias o normalizzazione silenziosa.
- Dodici righe storiche riportate ai sei stati ammessi senza promozioni.
- Master Matrix obbligatoria per ogni modifica sorvegliata.
- Copertura completa di `src/` e `tools/` e workflow attivo su `safety/**`.
- Test pre-fix: FAIL su tutte le tre classi P1-P3.
- Test post-fix: 5/5 PASS.

## Limiti

Nessuna verifica hardware e nessuna modifica avionica. La CI GitHub del branch
resta il gate successivo; P4-P8 non fanno parte di questo incremento.

Stato: `PARTIAL`, verificato software; non `DONE`.
