# R1-08 — Dieci passi EMS: intestazione e verità dei dati

Data: 2026-10-22. Candidato DEVELOPMENT / NON-CERT, non distribuito.
ID: PRJ-004, EMS-002, VER-001. Stati invariati.
Branch `fix/ems-responsive-header-r108-v1`, base PR #56
`306d145491a71406516832e23377f1f4f31ff447`. Include #53→#56, NON lo stack
meteo #55→#57→#58 o la quarantena writer #52. Non è un merge cumulativo.
R1-07 resta riservato al verticale EDP reale: non è implicitamente completato.

| Passo | Implementazione/verifica | Limite |
|---|---|---|
| 1 | CI #58 riconfermata Core/Project Control SUCCESS | Run 39743278411/39743278359, commit 2f1251a; nessun merge |
| 2 | Riprodotta la causa logo: coordinate scalate ma font con minimo non scalato | Il difetto era anche nella baseline 800×480, non solo in #56 |
| 3 | Creato EmsStatusHeader condiviso dalla pagina EMS | Registrato nei due moduli QML PFD ed EMS; non cambia la banda motore del PFD |
| 4 | Logo e label misurati, titolo tra gruppi non sovrapposti | Non più coordinate x arbitrarie per EFIS/NEXT/EMS |
| 5 | Rimossi OAT 24°C e QNH 1013 inventati | Non è un nuovo adapter air-data: OAT/QNH assenti dall'intestazione finché non esiste contratto valido |
| 6 | Clock LOCAL con binding persistente | Override della fixture può essere cambiato anche dopo un tick; non è un clock UTC GNSS qualificato |
| 7 | Target setup >=44 px e pressione lunga 1,2 s | Tocco breve non apre setup; prova mouse Qt, non touch del pannello |
| 8 | Test geometrici sulle 18 combinazioni, età massima e SIM FORCED | Suite totale 54 esiti Qt PASS, inclusi init/cleanup, non 54 requisiti |
| 9 | Build PFD/EMS, regressioni e catture | Release MSVC/AOT PASS, 37 Python PASS, 18 PNG Windows/software prodotti; native exe Windows non eseguiti |
| 10 | Evidenza, matrice, stato, cruscotto e proposta PR | Nessun PNG golden/tolleranza sostituito, nessuna attivazione o acceptance hardware |

## Evidenza e riproduzione

`docs/status/evidence/r1-08-ems-header-20261022/verification.json` lega sorgenti,
test e immagini tramite SHA-256. Tre catture rappresentative sono allegate
nella stessa directory; tutte le 18 sono generate in output/ems-header-review.
Il fixture versionato è `tests/qml-header-review/tst_review_capture.qml`.

Da root repository, usare una directory di output nuova/vuota e un runner Qt
compatibile già installato:

```powershell
New-Item -ItemType Directory output/ems-header-review
$env:QT_QPA_PLATFORM='offscreen'
$env:QT_QUICK_BACKEND='software'
# Windows offscreen: indicare una directory locale con font già installati
# Segoe UI regular/bold e Segoe UI Symbol. Non distribuire i font nel repository.
$env:QT_QPA_FONTDIR='PERCORSO_LOCALE_FONT_REVIEW'
& C:/Qt/6.11.1/msvc2022_64/bin/qmltestrunner.exe -input tests/qml
& C:/Qt/6.11.1/msvc2022_64/bin/qmltestrunner.exe -input tests/qml-header-review
```

Non usare `-Force` per sostituire una precedente serie di catture. Su Linux
usare il font environment della CI; un font diverso produce immagini diverse.
Il primo tentativo Windows senza font dir ha prodotto quadratini; il tentativo
con tutta C:/Windows/Fonts ha scelto un font condensato non rappresentativo.
Entrambi esclusi dalla review. La serie finale usa solo Segoe UI regular/bold
e Symbol in directory locale, senza installazioni o modifiche ai font globali.
I font non sono committati. Questa configurazione non approva Linux/ARM64.

## Esito della revisione e gate aperti

Il logo non si interseca più; badge e clock rimangono separati dal titolo.
Il valore fittizio OAT/QNH non compare più. Le prove conservano masking
STALE/LOST/FAIL e gli stati SIM. Le catture non costituiscono approvazione
complessiva di quadranti, scale, unità, contrasto o leggibilità sul pannello.

La baseline Linux #56 è ancora quella precedente: il nuovo header e il masking
per canale cambiano deliberatamente i pixel. Il confronto resta bloccante e
non viene aggirato cambiando soglie, PNG o manifesto. Occorrono rerender
Linux aggiornato, review completa e proposta esplicita di baseline prima del
merge. Il gate locale completo non è registrato per il rilevamento Python;
i nuovi binari MSVC sono compilati/linkati ma non avviati per la policy OS.
Il runner Qt già installato ha eseguito davvero QML e input sintetici.

Nessun nodo contattato o modificato in questo blocco; niente deploy, restart,
reboot, firmware, rete, CAN o calibrazione. Alimentazione/acceptance fisica
restano gate. Nessuna fonte motore o profilo è stato promosso a VERIFIED.

Prossimo: acquisire artifact Linux di questa revisione; correggere ulteriori
difetti prima di approvare golden. Poi R1-05b radar con observation time e
R1-07 EDP reale su stack coerenti, senza anticipare l'hardware.
