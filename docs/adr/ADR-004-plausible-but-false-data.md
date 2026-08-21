# ADR-004 — Il dato plausibile ma falso

Stato: accettata, 21 agosto 2026
Riferimenti: `CLAUDE_CODE_HANDOFF_2026-08-21.md` paragrafi 19 e 22, ADR-005

## Contesto

Il sistema sapeva accorgersi di tre modi in cui un dato può mancare:

- il nodo tace, e `NodeHealthMonitor` lo porta a `stale` e poi a `lost`;
- il nodo trasmette ma ripete la stessa sequenza, e il battito scartato non
  rinvia il timeout, quindi il nodo invecchia come se fosse muto;
- il campione non viene aggiornato entro `stale_after`, e `FlightModel` lo marca
  `stale`.

Restavano scoperti due casi, e sono i due che fanno male.

**Il valore fermo.** Un nodo che trasmette regolarmente, con sequenza crescente e
checksum valido, portando un valore che non cambia più perché il sensore dietro è
morto. Il `received_at` continua ad aggiornarsi, il campione resta valido per
sempre, e lo strumento mostra un numero fermo con l'aria di essere corretto. È il
pitot ostruito, l'inerziale che si latcha, il barometro che si pianta. Nessuno dei
tre controlli precedenti lo vede, perché tutti e tre guardano il *flusso*, non il
*contenuto*.

**La sorgente sola.** `Validity::disagree` esisteva nel modello dati fin
dall'inizio e non lo produceva nessuno: non era implementato alcun confronto
incrociato. Un sensore che sbaglia in modo coerente resta quindi invisibile,
perché non c'è niente con cui contraddirlo.

La differenza rispetto a un guasto ordinario è che qui lo strumento non dice
"non lo so": dice un numero. Il paragrafo 22 dell'handoff vieta di mascherare gli
errori con valori plausibili di fallback; questi due casi sono la stessa cosa
prodotta dall'hardware invece che dal codice.

## Decisione

Si introducono due meccanismi nel nucleo, entrambi indipendenti dal trasporto e
privi di dipendenze Qt, coerentemente con ADR-002.

### Rilevamento della stasi

`StasisMonitor` sorveglia le grandezze configurate e dichiara quando smettono di
muoversi. Quattro scelte, tutte conseguenza di come si guasta l'hardware vero e
non di come è comodo scrivere il codice.

**La soglia è per segnale, non globale.** L'assetto di un aeroplano in volo non è
mai perfettamente costante; una quota selezionata dal pilota lo è legittimamente
per tutto il volo. Una soglia unica sarebbe sbagliata per entrambi.

**Alcune grandezze si escludono invece di tararle.** Per una quota selezionata o
un corso impostato il rilevamento si disattiva. Una soglia molto alta sarebbe
comunque una soglia sbagliata, e produrrebbe un allarme il cui significato
nessuno saprebbe interpretare.

**Il confronto è sulla risoluzione dichiarata, non sull'uguaglianza.** Un sensore
quantizzato a un piede ripete lo stesso numero per più campioni senza essere
guasto, e un rumore di un bit non dimostra che stia misurando. La soglia è la
risoluzione dello strumento.

**Un dato già scaduto non viene sorvegliato.** La sua immobilità discende dal
guasto già rilevato: contarla darebbe due segnalazioni per una causa sola, e la
seconda sarebbe fuorviante.

L'esito si chiama `suspect` e non `fault`. Il monitor osserva, non diagnostica:
può darsi che il volo sia davvero stabile. La reazione corretta è segnalarlo al
pilota, non sostituire il valore né nasconderlo.

### Confronto incrociato

`CrossCheck` confronta due grandezze ottenute per vie fisicamente indipendenti,
sul principio che due catene separate non si guastano allo stesso modo nello
stesso istante. Due scelte.

**La persistenza è obbligatoria.** Raffiche, ritardi di filtro e tempi di
campionamento diversi producono scostamenti istantanei del tutto normali.
Dichiarare il disaccordo al primo campione riempirebbe lo strumento di allarmi
falsi, e un allarme che grida sempre è un allarme che nessuno guarda più. Entro
la finestra di persistenza lo scostamento si dichiara come accordo, non come
ignoto: uno scostamento transitorio previsto è comportamento normale, non
mancanza di informazione.

**In caso di disaccordo si marcano entrambi i campioni.** Il confronto stabilisce
che le due catene non concordano, non quale abbia torto. Marcarne una sola
presenterebbe l'altra come sana, cioè esattamente l'informazione che non si
possiede. `Validity::disagree` non è utilizzabile, quindi nessuno dei due valori
viene presentato come buono.

Con un termine di paragone inutilizzabile il risultato è `unknown` e la
persistenza si azzera: confrontare con un dato scaduto produrrebbe un disaccordo
che dipende dal guasto già noto.

## Conseguenze

Il nucleo acquisisce due sorveglianze che guardano il contenuto e non il flusso.
Restano da compiere tre passi perché diventino protezione reale e non soltanto
codice disponibile:

1. **Taratura per segnale.** Le soglie di stasi e le tolleranze di confronto
   vanno stabilite per ogni grandezza, con il criterio dichiarato, e verificate
   su registrazioni reali. Finora è dimostrato il meccanismo, non la taratura.
2. **Collegamento allo stato di volo.** Il monitor va alimentato da chi compone
   `FlightSnapshot`, e l'esito deve raggiungere il motore di allarme e il
   display. Oggi i due meccanismi esistono e sono provati in isolamento.
3. **Confronti concreti da attivare.** I candidati sono la velocità verticale
   barometrica contro quella satellitare, e la velocità indicata contro quella al
   suolo con tolleranza ampia, sufficiente a intercettare il guasto grossolano e
   non la deriva sottile. Il confronto fra quota barometrica e quota satellitare
   in valore assoluto non è utilizzabile, perché le due differiscono per
   regolazione barometrica e separazione del geoide: va confrontata la loro
   variazione, non il loro valore.

Resta inoltre inutilizzata l'autodiagnosi dell'autopilota: `EKF_STATUS_REPORT` e
`VIBRATION` sono nella tabella dei messaggi e vengono decodificati, ma l'adapter
non li traduce in campioni. Sono varianze di innovazione e livelli di vibrazione
già calcolati da ArduPilot, cioè il confronto incrociato più economico
disponibile, e oggi vengono buttati.

## Alternative considerate

**Sostituire il valore sospetto con una stima.** Scartata: produrrebbe
esattamente il dato plausibile ma falso che si vuole eliminare, con l'aggravante
di essere generato da noi.

**Dichiarare il valore fermo direttamente non disponibile.** Scartata: la stasi è
un'osservazione, non una diagnosi. Rimuovere un valore che potrebbe essere
corretto priva il pilota di un'informazione utile in una fase in cui gli serve.
Segnalare e lasciare decidere è più onesto e più sicuro.

**Attribuire il disaccordo alla sorgente meno affidabile.** Scartata: richiede
una gerarchia di fiducia che il confronto non è in grado di validare. Il compito
di scegliere una sorgente appartiene a `SourceArbitrator`, ADR-005, e presuppone
di sapere quale è sana. Qui non lo si sa, e fingerlo sarebbe peggio.

## Verifica

`tests/safety_tests.cpp`, eseguibile separato da quello del nucleo perché
verifica una categoria diversa: non che il sistema calcoli il valore giusto, ma
che si accorga quando il valore che riceve non significa più nulla.

Comprende il caso principale in forma esplicita: un campione che continua ad
arrivare fresco e con validità piena, che ogni controllo preesistente considera
buono, e di cui solo il valore non cambia più.

Prove di mutazione eseguite sul Raspberry Pi 5. Disattivando la soglia di stasi
fallisce «a value that stops changing while still arriving must be flagged»;
disattivando la persistenza del confronto fallisce «a deviation persisting beyond
the dwell must be declared». Ripristinato il codice, entrambe tornano verdi. Le
due proprietà sono quindi realmente coperte e non dichiarate a vuoto.

Build pulita con `-Wall -Wextra -Wpedantic -Wconversion` su GCC 14.2 aarch64 e
con `/W4 /permissive-` su MSVC 19.51.
