# R1-09 — Dieci passi consolidati nella PR #59

2026-10-22. DEVELOPMENT / NON-CERT. Nessun merge/deploy/hardware.
ID: PRJ-001, PRJ-004, EMS-002, VER-001; stati invariati.
Stesso branch `fix/ems-responsive-header-r108-v1`: questo follow-up consolida
R1-08, non apre una nuova catena di PR. Base del blocco: `0c83113aa4529b038d05cf171a15f9da6f131c54`.

| Passo | Risultato consolidato | Evidenza/limite |
|---|---|---|
| 1 | Snapshot delle otto PR e dipendenze | SHA e run in pr-snapshot.json; non monitoraggio continuo, main non aggiornato |
| 2 | CI #59 distinta per causa | Core/sanitizer/build/test Qt PASS; Project Control PASS; solo golden precedente FAIL |
| 3 | Artifact Linux verificato indipendentemente | ZIP 12411617225, SHA esterno verificato, 18 viste; non basta il manifesto co-committato |
| 4 | Report per immagine legato a candidato e riferimento | Commit, archive SHA, reference manifest SHA e pixel SHA; due viste CHANGES_REQUIRED, altre 16 NOT_REVIEWED |
| 5 | Corretto secondo overlap CAS | Titolo e LIMITS INDICATIVE separati; banner e airframe non ancorati con offset confliggenti |
| 6 | Sei regressioni geometriche | Tre risoluzioni × limiti indicativi/verificati in fixture; non verifica dei limiti reali |
| 7 | Decoder PNG limitato | Dimensioni, CRC/chunk, codifica, espansione zlib e trailing data verificati; stesso formato RGB/RGBA statico supportato |
| 8 | Tool di review fail-closed | ZIP confinato, niente symlink/duplicati/traversal/overwrite; anche ACCEPT totale non autorizza baseline |
| 9 | Verifiche ripetibili | 60 esiti Qt, 47 Python e 18 Python Linux isolato PASS; overlap dei test, non somma di requisiti |
| 10 | Unificazione del punto corrente | Stato, matrice, cruscotto, planning, evidenze e commit nello stesso branch/PR; CI follow-up da verificare |

## Candidati: quadro unico al momento dello snapshot

| PR | Oggetto | Dipendenza | Core / Control osservati | Disposizione |
|---|---|---|---|---|
| #52 | Quarantena writer | main | nessun run restituito dalla query PR | verificare CI prima di integrazione; non assumere PASS |
| #53 | Stato motore separato dal volo | main | PASS / PASS | candidato, non integrato |
| #54 | Presentazione radar/expiry | main | PASS / PASS | candidato indipendente da #55, non assunto incluso |
| #55 | Posizione radar valida | main | PASS / PASS | base dello stack meteo |
| #56 | Validità canali/fuel | #53 | FAIL golden / PASS | review grafica bloccante |
| #57 | Observation time METAR/TAF/radar | #55 | PASS / PASS | legacy radar reale UNKNOWN TIME, disponibilità non chiusa |
| #58 | Confini manutenzione e Project Control | #57 | PASS / PASS | candidato su stack meteo, non nello stack EMS |
| #59 | Intestazione EMS e R1-09 | #56 | FAIL golden / PASS su 0c83113 | draft; nuovo follow-up da verificare |

Snapshot non significa mergeability: la query normalizzata non prova assenza
di conflitti. In una futura finestra autorizzata integrare per dipendenze,
risolvere le sovrapposizioni nei controller, WeatherService e documenti,
rigenerare gli output e ripetere CI sul risultato. Non usare reset/rebase/force
o cancellazioni per nascondere divergenze. Nessuna PR è stata unita qui.
Il cruscotto serverha continua a seguire main: non mostra automaticamente
questi candidati. Il cruscotto del branch riflette solo la matrice del branch.

## Review Linux della versione 0c83113

Run `39745779697`, artifact `12411617225`, SHA-256 ZIP:
`6b95b568313f3ed101906d66481701fd275b8d281304b1959d6c1173cbbc0e2f`.
I dati sono di Qt 6.8.2 offscreen/OpenGL software del job Linux; non sono
stati reinterpretati come catture ARM64. Il report confronta davvero i pixel
con il riferimento; non promuove i PNG soltanto perché il manifesto coincide.

Ispezione diretta delle viste Linux 800 LIVE/LOST: logo corretto ma CAS MESSAGES
si sovrappone a LIMITS INDICATIVE. Due decisioni CHANGES_REQUIRED, nessuna
decisione ACCEPT. Le altre 16 viste restano esplicitamente NOT_REVIEWED.
La vista 1280 STALE è stata osservata ma non approvata come baseline.
La correzione CAS del follow-up è verificata Qt/Windows; il vecchio artifact
non dimostra il nuovo codice. Occorre un nuovo artifact Linux.

## Uso del preparatore di review

Da root del repository, creare un report in un percorso nuovo FUORI dalla
directory del riferimento:

```text
python -m tools.ui.prepare_ems_golden_review --archive <zip> --sha256 <SHA esterno verificato> --reference tests/golden/ems-responsive-states-linux --source-commit <SHA completo> --run-id <run> --artifact-id <artifact> --output <nuovo-report.json>
```

Per allegare decisioni utilizzare `--review <decisioni.json>`, nello schema
dimostrato da `linux-review-decisions.json`. Ogni decisione lega file e pixel;
ACCEPT/CHANGES_REQUIRED richiedono reviewer e motivazione. Un cambio candidato,
commit o riferimento invalida il binding. Il nome del reviewer resta una
dichiarazione del chiamante, NON una firma digitale o verifica d'identità.
Gli ID CI/source sono dichiarati dal chiamante e vanno confrontati con GitHub:
lo strumento offline verifica i byte, non autentica GitHub né il processo Qt.

Exit 0 significa pacchetto valido preparato, NON golden approvata. L'esito può
essere PENDING_REVIEW, CHANGES_REQUIRED o REVIEW_COMPLETE_NEEDS_AUTHORIZATION.
`baseline_update_authorized` e `flight_approval` rimangono sempre false. Non
esiste opzione per copiare/promuovere la baseline. I file di riferimento non
vengono modificati e un report esistente non viene sovrascritto.

PNG: massimo 8 MiB codificati e 4.194.304 pixel, solo 8-bit RGB/RGBA non
interlacciato; limite espansione esatto da IHDR. ZIP: massimo 16 MiB codificati,
64 MiB espansi, 19 voci esatte (18 PNG + manifest). Il confronto di metriche
non cambia le tolleranze del gate CI. Nessuna qualifica contro modifiche
concorrenti ostili o attestation del renderer viene dichiarata.

## Verificato e aperto

Build/link PFD+EMS e AOT MSVC Release PASS. Qt Windows 60 esiti (init/cleanup
inclusi), Python 47 PASS, ripetizione Linux 18 PASS nel container esistente,
rete disabilitata e sorgente read-only. I 18 sono un sottoinsieme dei 47.
18 nuove catture Windows, due allegate per CAS; non nuova golden Linux.
Fixture DEGRADED allineata a 600 ms come il capturer canonico.

Nessun nodo avionico contattato, nessuna attivazione, reboot, calibrazione,
rete o firmware modificato. Il container su serverha è solo verifica software.
Restano review/rerender Linux aggiornato, glyph/font target, pannello e power;
R1-05b prodotto radar+observation time e R1-07 verticale EDP reale sono i
successivi sviluppi separati. Non aumentare lo stato di avanzamento hardware
per questi risultati. Evidenza: `docs/status/evidence/r1-09-consolidation-20261022/`.
