# CORE-001 — Baseline sanitizer Linux

Data di revisione: 2026-09-30

Scopo: documentare la baseline necessaria a rendere bloccante il job
`linux-sanitizers` del workflow `EFIS Core CI`.

## Esecuzioni revisionate

| Run | Commit `main` | Esito `linux-sanitizers` | Durata |
|---:|---|---|---:|
| [#174](https://github.com/sfuso68/efis-next-professional/actions/runs/36321746928) | `6561051` | success | 2m 19s |
| [#177](https://github.com/sfuso68/efis-next-professional/actions/runs/36323333462) | `914470b` | success | 1m 58s |
| [#180](https://github.com/sfuso68/efis-next-professional/actions/runs/36339495509) | `c1d409b` | success | 2m 13s |
| [#189](https://github.com/sfuso68/efis-next-professional/actions/runs/36554438490) | `7aa45b6` | success | 3m 13s |
| [#199](https://github.com/sfuso68/efis-next-professional/actions/runs/36686371047) | `4d7154e` | success | 2m 57s |
| [#202](https://github.com/sfuso68/efis-next-professional/actions/runs/36700690796) | `3c7bb4d` | success | 3m 07s |

Le sei esecuzioni sono consecutive fra quelle revisionate su `main` e coprono
modifiche documentali, tooling, test, inventario hardware e codice PFD. Non e'
stato osservato alcun fallimento tollerato del job sanitizer in questa serie.

## Modifica autorizzata dal risultato

Viene rimossa esclusivamente la proprieta' `continue-on-error: true`. Restano
invariati:

- `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1`;
- `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`;
- build Debug senza Qt;
- `-fsanitize=address,undefined` e `-fno-sanitize-recover=all`;
- CTest con timeout di 90 secondi.

I trigger `push` di `EFIS Core CI` e `EFIS Project Control` includono inoltre
`ci/**`, così un branch che cambia la policy CI viene verificato prima del
merge e non dipende soltanto dall'evento della pull request.

## Limiti dell'evidenza

- piattaforma verificata: runner GitHub Actions Linux x86-64;
- nessun test ARM64 o nodo reale;
- nessuna approvazione al volo;
- la futura migrazione di `ubuntu-latest` richiede sorveglianza, non una
  disabilitazione preventiva del gate.
