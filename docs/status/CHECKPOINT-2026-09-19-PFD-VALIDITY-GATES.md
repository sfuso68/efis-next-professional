# Checkpoint — PFD validity gates V1

Data: 2026-09-19

Branch: `safety/pfd-validity-gates-v1`

Requisito principale: `PFD-001`

## Obiettivo

Impedire che il PFD rappresenti come attendibili dati di volo non validi o
valori dimostrativi non provenienti dal modello dati.

## Modifica software

- Gate fail-closed nel renderer SVG per assetto, prua, ground speed, velocità
  verticale, quota e rotta selezionate e flight director.
- Soppressione dei valori hard-coded `AP ALT 12000` e `TREND +2`.
- Gate equivalenti nella catena Qt/QML e proprietà di validità esplicite nel
  `PfdController`.
- Scale e cursori dei nastri non vengono costruiti attorno a un valore corrente
  non valido; il valore principale diventa `---`.
- Conteggio visibile degli allarmi ulteriori nel banner compatto.

## Evidenza software

- Build core MSVC: PASS.
- Test mirato `efis_core_tests`: PASS.
- Build Qt/QML completa, inclusa generazione AOT QML: PASS.
- CTest Windows: 29/30 PASS nella suite completa. Il test Qt finale passa da
  solo in 14,74 s ma, ripetendo l'intera suite, va in timeout a 90 s; il gate
  complessivo locale resta aperto in attesa della CI.

## Evidenza hardware

Nessuna. Il branch non è stato distribuito su PFD, MFD o EMS.

## Criteri prima della chiusura

1. CI del branch interamente verde.
2. Review separata senza unione con gli altri rilievi dell'audit.
3. Fault injection sul PFD RPi5 per perdita e recupero delle sorgenti.
4. Verifica visuale a 1024x600 e controllo che nessun dato precedente resti
   presentato come valido.

Stato: `PARTIAL`, verificato software; non `DONE` e non flight approved.
