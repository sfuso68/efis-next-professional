# EMS Professional RPi4 — Concept e specifica V1

## 1. Obiettivo

Trasformare il Raspberry Pi 4 in un nodo EMS completo, autonomo e leggibile in
volo, con una grafica di livello professionale e non con l'aspetto di una
dashboard generica. Il nodo usa la stessa codebase EFIS NEXT e lo stesso modello
dati del PFD, ma possiede viste e priorita' progettate intorno al motore.

Il risultato deve poter funzionare:

- come display EMS dedicato da 7 o 10,1 pollici;
- come pagina EMS integrata nel PFD/MFD;
- come sorgente della banda motore compatta su qualunque altro pannello;
- in simulazione, banco sensori e dati reali senza cambiare QML;
- anche se il Data Hub non e' disponibile, usando un ingresso locale previsto
  dal profilo hardware.

## 2. Principio grafico

Non si copia il layout di un prodotto commerciale. Si adottano i principi che
rendono efficaci i sistemi professionali:

- gerarchia forte: il parametro che richiede azione domina la pagina;
- valori primari grandi, unita' e nomi subordinati;
- archi colorati sottili e precisi, non anelli decorativi pesanti;
- fondo quasi nero, pannelli traslucidi e separazione per spaziatura;
- verde solo per il campo normale, ambra e rosso riservati alle anomalie;
- cyan per selezioni e informazioni, magenta solo per guida/navigazione;
- animazioni limitate a transizioni utili, mai decorative;
- stato LIVE/SIM/STALE/FAIL sempre visibile;
- touch target minimi di 44 px alla risoluzione logica 1024×600.

La pagina corrente viene considerata un prototipo funzionale, non il layout
finale. Il nuovo sistema usa griglia, tipografia e componenti vettoriali comuni
al design system EFIS NEXT.

## 3. Architettura delle viste

### 3.1 ENGINE — vista di volo

E' la pagina predefinita. Deve rispondere in meno di un secondo alle domande:
"quanta potenza sto chiedendo?", "il motore e' sano?", "quanto carburante resta?".

Layout proposto:

- fascia superiore di stato con sorgente, nodo EMS, registratore e allarme CAS;
- due strumenti dominanti: RPM e MAP oppure percentuale potenza;
- tre strumenti secondari grandi: pressione olio, temperatura olio, fuel flow;
- blocco CHT/EGT a colonne sottili, con cilindro piu' caldo evidenziato;
- carburante sinistra/destra con totale, endurance e riserva prevista;
- tensione e corrente compatti, disposti verticalmente;
- barra inferiore con azioni contestuali: `LEAN`, `FUEL`, `TREND`, `CHECKLIST`.

Gli strumenti circolari aumentano circa del 35–40% rispetto al prototipo
attuale. I nomi entrano nel quadrante, mentre le unita' rimangono sotto il valore.

### 3.2 LEAN — analisi cilindri

Pagina dedicata a EGT/CHT, non un semplice ingrandimento della pagina principale.

- una colonna per cilindro con CHT ed EGT sovrapposti ma cromaticamente distinti;
- valore numerico e delta dal cilindro medio;
- riconoscimento del primo e ultimo picco;
- delta dal picco per ogni cilindro;
- spread CHT e spread EGT;
- cronologia breve di 120 secondi;
- stato `LEAN ASSIST ARMED / FINDING PEAK / COMPLETE`;
- funzione disabilitabile dai profili motore che non prevedono miscela manuale.

Le soglie e la procedura non vengono inventate dal software: derivano dal
profilo motore e dal manuale applicabile.

### 3.3 FUEL — autonomia e gestione serbatoi

- serbatoi rappresentati con geometria coerente e valore numerico dominante;
- fuel used, fuel remaining, fuel flow e imbalance;
- endurance a consumo corrente;
- autonomia stimata alla velocita' al suolo ricevuta dalla rete;
- carburante previsto a destinazione quando esiste un piano di volo;
- comando protetto `ADD FUEL` con conferma e registrazione blackbox;
- confronto fra carburante misurato e calcolato, con avviso di divergenza.

### 3.4 TREND — diagnostica in volo

- grafici reali, non linee decorative;
- finestre selezionabili 2, 10 e 30 minuti;
- RPM, MAP, oil pressure, oil temperature, fuel pressure, bus voltage;
- selezione di CHT/EGT singolo o spread;
- marcatori per decollo, allarme, cambio sorgente e intervento pilota;
- cursore touch per rileggere valore e timestamp.

### 3.5 MAINTENANCE — pagina a terra

- ore motore e tempo dall'ultima manutenzione;
- cicli di avviamento;
- massimi/minimi dell'ultimo volo;
- elenco sensori con LIVE/STALE/FAIL, seriale e revisione calibrazione;
- test allarmi, test canali e acquisizione banco;
- esportazione del volo e rapporto anomalie;
- accesso protetto alla configurazione tramite pressione prolungata del logo.

Questa pagina non e' raggiungibile accidentalmente durante il volo: le funzioni
che cambiano configurazione sono inibite quando RPM o velocita' indicano moto.

## 4. Reversione e banda compatta

Il nodo EMS dedicato deve poter mostrare una pagina di emergenza semplificata se
il PFD principale non e' disponibile. Non diventa un secondo PFD completo, ma
puo' visualizzare una banda minima ricevuta dal Pixhawk/Data Hub:

- IAS;
- quota;
- assetto semplificato;
- prua;
- allarme principale.

Viceversa PFD e MFD ricevono una banda EMS compatta con RPM, oil pressure, oil
temperature, fuel remaining e stato CAS. I layout 100%, 50% e bottom band sono
progettati separatamente: non sono semplici riduzioni automatiche.

## 5. Dati e sicurezza

Ogni grandezza mostrata porta con se':

- valore ingegneristico e valore grezzo;
- unita';
- sorgente e nodo;
- timestamp e sequenza;
- validita' LIVE/STALE/DEGRADED/FAIL/SIM;
- revisione di calibrazione;
- limiti derivati dal profilo aeromobile/motore.

Un dato scaduto non conserva il colore verde. L'ultimo valore puo' restare
visibile per continuita', ma viene desaturato, barrato e accompagnato dalla sua
eta'. Un valore simulato e' sempre dichiarato.

La priorita' CAS e': warning rosso, caution ambra, advisory cyan. Il messaggio
deve dire grandezza e azione o condizione, per esempio `OIL PRESS LOW`, non un
generico `ENGINE WARNING`.

## 6. Piattaforma RPi4

Target iniziale:

- Raspberry Pi 4 a 64 bit;
- Debian/Raspberry Pi OS compatibile con Qt 6.8.x;
- display 1024×600 come baseline, scaling per 1280×800 e 1920×1080;
- Qt Quick/QML con componenti C++ per grafici ad alto aggiornamento;
- rendering a 40 FPS minimo stabile, obiettivo 50 FPS;
- aggiornamento dati motore 10–20 Hz;
- memoria applicazione obiettivo inferiore a 600 MiB;
- avvio pagina utile entro 8 secondi dal lancio del servizio;
- watchdog, riavvio controllato e log strutturato.

Qt non e' il limite grafico: il limite attuale e' il design dei componenti e la
densita' del layout. Qt Quick puo' produrre archi vettoriali, shader, grafici e
transizioni professionali sul RPi4, purche' si eviti di costruire ogni elemento
come molti oggetti QML sovrapposti.

## 7. Componenti da realizzare

1. `EmsArcGaugeV2`: arco sottile, tick multilivello, valore dominante, bug e
   bande ricavate dal profilo.
2. `CylinderMatrixV2`: CHT/EGT dinamici, hottest/first-to-peak e spread.
3. `CasMessageStack`: massimo tre messaggi ordinati per severita'.
4. `FuelModelPanel`: misurato, calcolato, endurance e imbalance.
5. `TrendPlot`: buffer circolare reale e marcatori eventi.
6. `EmsPageRail`: navigazione verticale compatta e traslucida.
7. `DataIntegrityBadge`: sorgente, eta', qualita' e simulazione.
8. `EmsCompactBand`: widget condiviso fra PFD, MFD e display dedicato.

## 8. Sequenza di implementazione

### Fase 1 — fondazione visiva

- fissare griglia e proporzioni per 1024×600;
- realizzare gauge V2 e matrice cilindri;
- sostituire il menu orizzontale con rail verticale contestuale;
- produrre screenshot golden a tre risoluzioni.

### Fase 2 — dati reali

- sostituire EngineSimulationProvider con un'interfaccia selezionabile;
- collegare EFIS Data Hub via Ethernet;
- mantenere fallback CAN locale configurabile;
- propagare stato, eta' e calibrazione fino al QML.

### Fase 3 — funzioni evolute

- trend reali;
- fuel computer;
- Lean Assist abilitato dal profilo;
- CAS e checklist contestuali;
- pagina Maintenance.

### Fase 4 — qualifica sul RPi4

- test termico e throttling;
- FPS e frame-time per 60 minuti;
- avvio a freddo e recovery dopo perdita alimentazione;
- perdita Ethernet, perdita CAN e dati congelati;
- leggibilita' diurna/notturna e touch con guanti;
- confronto visivo automatico delle pagine.

## 9. Criterio di completamento V1

La V1 e' completa quando il nodo RPi4 mostra dati simulati e reali con la stessa
interfaccia, mantiene almeno 40 FPS, supera le prove di perdita delle sorgenti,
registra ogni allarme e cambio sorgente, e presenta le cinque viste senza
sovrapposizioni alle tre risoluzioni target.

## 10. Stato implementazione grafica — 22 agosto 2026

La fondazione visiva della Fase 1 e' ora implementata nella pagina QML condivisa
dal nodo EMS dedicato e dal PFD:

- cinque quadranti con tick minori, intermedi e maggiori e scala numerata;
- RPM e manifold pressure dominanti, strumenti secondari ingranditi;
- matrice CHT/EGT a quattro cilindri ampliata;
- rail verticale contestuale;
- pannello CAS/AIRFRAME configurato dal profilo aeromobile;
- tre consensi indipendenti del carrello retrattile;
- flap con detent, posizione misurata e posizione comandata;
- impaginazione verificata a 1024x600 e 800x480.

Le immagini di verifica sono:

- `assets/ems-professional-implemented-1024x600.png`;
- `assets/ems-professional-implemented-800x480.png`.

Carrello e flap non sono obbligatori: in assenza delle rispettive sezioni nel
profilo, i widget restano nascosti. I valori mostrati in questa fase provengono
ancora dal simulatore e non costituiscono dati di volo.

## 11. Revisione visuale V3 — riferimento professionale

La precedente fondazione e' stata sostituita da una composizione V3 progettata
sul riferimento visuale approvato. Il nuovo `EmsGaugeV3` separa la faccia
statica prerenderizzata dagli elementi dinamici e comprende fondo radiale,
cornice multianello, bande operative, graduazioni multilivello, scala numerata,
lancetta animata e mozzo stratificato.

La pagina assegna priorita' visiva a RPM e MAP, mantiene tre strumenti secondari,
affianca CAS/AIRFRAME e matrice CHT/EGT, e chiude con fuel/endurance/electrical e
quattro azioni contestuali. Le catture verificate sono:

- `assets/ems-v3-implemented-1024x600.png`;
- `assets/ems-v3-implemented-800x480.png`.

### Verifica Linux ARM64 V3.2

La revisione V3.2 e' stata compilata ed eseguita sul banco Debian ARM64 con
Qt 6.8.2. Le facce Canvas usano una famiglia generica portabile e la cattura
non produce avvisi di font. Sono state verificate entrambe le geometrie target:

- `assets/ems-v3-2-linux-1024x600.png`;
- `assets/ems-v3-2-linux-800x480.png`.

La V3.2 aggiunge icone vettoriali indipendenti dal sistema, sagoma carrello,
scale numeriche CHT/EGT e pannelli touch per Lean Assist, Fuel Computer, Trend
e Checklist.
