# Strumenti analogici — specifica V1

Stato: fasi 0, 1 e 2 realizzate; fase 3 ai passi 1 e 2; fase 4 da realizzare
Data: 21 agosto 2026
Riferimenti: `CLAUDE_CODE_HANDOFF_2026-08-21.md` §2, §5, §18, §19, §22;
`EFIS_UI_DESIGN_SYSTEM.md`; ADR-002; ADR-005

## Scopo

Una sezione del pannello dove il pilota sceglie strumenti analogici e li dispone
su pagine, per condurre il volo con una presentazione a quadranti invece che a
nastri e numeri.

Non e' una modalita' di svago. Gli strumenti mostrano **gli stessi dati
arbitrati** del PFD digitale, con le stesse bandierine di avaria e la stessa
marcatura di dato simulato o scaduto. Uno strumento analogico che mente e'
pericoloso quanto uno digitale che mente, e ha in piu' il vantaggio di sembrare
familiare.

## Decisioni

Undici decisioni prese con il proprietario il 21 agosto 2026.

| # | Decisione | Scelta |
|---|---|---|
| 1 | Ruolo | Presentazione alternativa dei dati reali |
| 2 | Grafica | Vettoriale originale nel linguaggio EFIS NEXT |
| 3 | Virata e scivolata | Simulatore esteso con modello di virata corretta |
| 4 | Disposizione | Modelli classici con slot sostituibili |
| 5 | Altimetro | Regolazione barometrica vera nel core |
| 6 | Marcatura simulazione | Cornice di pagina e banda in alto |
| 7 | Classi di display | Pannello **e** classe Round 240–480 px da subito |
| 8 | Regolazioni in volo | OBS, prua e QNH regolabili; configurazione solo a terra |
| 9 | Unita' | Selezionabili dal pilota nel setup |
| 10 | CHT ed EGT | Un quadrante a quattro lancette |
| 11 | Profili | Standard e Professional, con capability distinte |

### Note sulle decisioni meno ovvie

**Grafica originale (2).** Le marcature aeronautiche sono convenzioni e si
usano; l'aspetto distintivo degli strumenti di un costruttore reale no. Il §2
dell'handoff lo vieta, e un quadrante fotorealistico sarebbe comunque meno
leggibile su un 1024×600 e piu' costoso da disegnare sul RPi5.

**Virata (3).** Il rateo non si ricava derivando la prua. La derivata di una
prua filtrata segue la prua con ritardo, amplifica il rumore, e in virata
stretta non coincide con il rateo vero: sarebbe un numero plausibile che non
misura nulla, cioe' cio' che il §22 vieta.

**Classe Round da subito (7).** Costa di piu' adesso, ma fissa i vincoli di
leggibilita' piu' severi mentre la geometria e' ancora malleabile. Rende anche
possibile il singolo strumento su display accessorio previsto dal §2.

**Unita' selezionabili (9).** Il profilo attivo dichiara oggi `INHG`, ereditato
dal catalogo legacy e mai verificato. In Italia il QNH arriva in ettopascal. La
scelta resta al pilota e viene salvata nel profilo.

**Quattro lancette per i cilindri (10).** L'informazione utile non e' il valore
assoluto di un cilindro ma la **dispersione** fra i quattro: un cilindro che si
scosta dagli altri si vede a colpo d'occhio solo se sono mostrati insieme. Con
un selettore bisognerebbe guardarli a turno, e nessuno lo fa finche' non e'
tardi.

## Architettura

Quattro confini, e nessuna freccia punta all'indietro.

**La geometria vive nel core, senza Qt.** Una funzione descrive dove stanno
lancette, tacche, archi e bandierine. Due consumatori la usano: il nodo scene
graph che disegna sullo schermo e il renderer SVG che produce le immagini di
confronto. Averne una sola e' cio' che rende gli strumenti verificabili senza
scheda grafica. Se le due rappresentazioni divergessero, il confronto a immagine
non direbbe piu' nulla sullo strumento vero.

**Coordinate normalizzate.** Origine al centro, raggio unitario: la stessa
descrizione serve 240 px e 1024. La dimensione entra solo dove cambia le
scelte, cioe' in quante tacche e quante scritte ci stanno.

**Il legame avviene per identificativo di segnale.** Non per nome di proprieta':
con strumenti scelti dal pilota ogni strumento nuovo richiederebbe altrimenti
codice nuovo. Il pannello deve poter chiedere una grandezza che nessuno aveva
previsto sarebbe stata chiesta.

**Lo strumento e' presentazione, mai sorgente.** Non calcola grandezze
avioniche, non arbitra, non inventa. Se la grandezza non esiste, lo strumento
non esiste.

## Regole di sicurezza

Sono la ragione per cui questa sezione ha un eseguibile di prova proprio.

1. **La lancetta non supera il fondo scala.** Lasciata proseguire rientrerebbe
   dall'altra parte e mostrerebbe una lettura bassa credibile al posto di una
   alta impossibile.
2. **Il superamento si dichiara.** La lancetta a fondo scala da sola sarebbe
   indistinguibile da una lettura pari al fondo scala.
3. **Uno strumento senza dato non mostra la lancetta.** Parcheggiarla su zero
   significherebbe mostrare una misura, perche' zero e' una lettura possibile.
4. **Dato assente e dato scaduto restano distinti.**
5. **Le fasce vengono dal profilo**, non dal codice, e se il profilo e'
   `INDICATIVE` lo strumento lo dichiara: un arco verde su limiti non verificati
   e' una dichiarazione di sicurezza che nessuno ha firmato.
6. **Una fascia fuori scala non si disegna**, invece di comparire schiacciata
   contro un estremo dove indicherebbe un limite che li' non c'e'.
7. **Le tacche si diradano con la dimensione**, raddoppiando il passo, cosi' le
   superstiti cadono ancora sui valori tondi.

## Strumenti previsti

Ricavabili dai sensori attuali: orizzonte artificiale, anemometro con fasce,
altimetro a tre lancette con finestra di Kollsman, variometro, girodirezionale,
virosbandometro con pallina, CDI VOR/ILS; contagiri, pressione di alimentazione,
pressione e temperatura olio, pressione e flusso carburante, quantita'
carburante bi-ago, voltmetro, amperometro, CHT ed EGT a quattro lancette.

**Non previsti, e non per dimenticanza:** termometro aria esterna, perche' non
esiste un sensore OAT; vacuometro, perche' non esiste una pompa a vuoto, e
metterlo su un impianto che non ce l'ha sarebbe decorazione che in volo puo'
ingannare.

## Fasi

**Fase 0 — fondamenta. Realizzata.** Geometria dei quadranti nel core
(`include/efis/instruments/dial.hpp`), regolazione barometrica e quota indicata
(`altimetry`), rateo di virata e accelerazione laterale (`flight_dynamics`),
accesso alle grandezze per identificativo (`signal_access`), renderer SVG per
poterli guardare (`efis-render-dials`).

**Fase 1 — i sei strumenti di volo. Realizzata.** Orizzonte, anemometro, altimetro,
variometro, girodirezionale, virosbandometro. Ognuno con bandierina, dato
scaduto e marcatura di simulazione.

**Fase 2 — strumenti motore. Realizzata.** Compresi CHT ed EGT a quattro lancette, con
archi dai limiti del profilo motore.

**Fase 3 — pagine e configurazione. Ai passi 1 e 2.** Fatti: catalogo dei
tipi, modelli classici con slot sostituibili, validazione contro la superficie,
e attivazione atomica con backup, audit e rollback, come per il profilo
aeromobile. Restano: `config/panels/` versionato e la voce `ANALOG` nel
selettore, con il suo sottomenu.

**Fase 4 — navigazione e display circolari.** CDI con OBS regolabile in volo,
pagina a strumento singolo per la classe Round, prova termica prolungata.

### Definition of Done per ogni strumento

Immagine di confronto a 240, 320, 480 e 1024 px; comportamento verificato per
dato valido, simulato, scaduto e assente; cattura dal vero sul RPi5 a 1024×600;
frametime e memoria misurati, non solo FPS medio.

## Lavoro aperto

- **Sorveglianza del rateo di virata.** Deciso di aggiungerla con soglia e
  risoluzione proprie: in crociera rettilinea il rateo resta legittimamente
  prossimo a zero per minuti, quindi la regola non puo' essere quella della
  velocita'. Va tarata su un volo reale registrato, non a tavolino.
- **Preferenza di unita' nel profilo aeromobile**, con la relativa voce di
  setup.
- **Capability** `analog_basic` e `analog_full` da definire nel modello
  centrale, senza `if professional` sparsi nella UI.
