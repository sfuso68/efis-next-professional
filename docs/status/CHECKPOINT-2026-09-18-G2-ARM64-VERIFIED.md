# Checkpoint — G2 ICD replay ARM64 verificato — 2026-09-18

## Esito

Il replay read-only autorizzato del freeze ICD AVIONIX
`c331a9d3846d1f59addcea76f9db530174c8ec11` è stato completato sui tre target
previsti. Ogni nodo ha eseguito il validator, gli stessi 11 unit test e il
calcolo dei digest congelati.

| Ruolo | Nodo | Piattaforma | Esito | Report SHA-256 |
|---|---|---|---|---|
| PFD | `efis-pfd-01` / `192.168.1.45` | RPi5, `aarch64`, Python 3.13.5 | PASS, 11/11 | `80f6258c2fc7c469470768fef35687e1bea7424532a1b3b4bcc039867201841f` |
| MFD | `pi5` / `192.168.1.49` | RPi5, `aarch64`, Python 3.13.5 | PASS, 11/11 | `5fb12c74488f6b0ff46d51d027e176b115dd3cc0db5937587bc34399354c7d3e` |
| EMS | `efis` / `192.168.1.36` | RPi4, `aarch64`, Python 3.13.5 | PASS, 11/11 | `e66951087e5687339353faff04fb51cdfd99bf5728b27f8267bc119049644bea` |

I quattro digest di ICD, schema evidence, vettori e validator coincidono sui
tre target e con il freeze workstation x86-64. I report e i bundle individuali
sono archiviati sotto `docs/status/evidence/`; il bundle aggregato è
`g2-arm64-consolidated-20260918/evidence.json`.

## Classificazione

- `AVX-G2-01`: `ACCEPTED` per freeze e replay multipiattaforma.
- `PRJ-003`: resta `VERIFY-HW`, perché il requisito di gate include ancora la
  prova del collegamento dati reale.
- `AVX-G2-02`: resta `PROPOSED` e non autorizzato. Nessuna prova verticale
  sensore→RPi4→Blackbox→RPi5 è stata eseguita da questa attività.

## Limiti

- Nessun flusso Ethernet DDP/EDP live è stato provato.
- Nessun Pixhawk, CAN, sensore, display, touch, timing o fault fisico è stato
  esercitato.
- La Blackbox non faceva parte dei tre target del replay autorizzato.
- L'esito non costituisce approvazione al volo.
