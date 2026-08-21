# EFIS UI Design System v0.2

## Principi

- leggibilità prima della decorazione;
- geometria vettoriale indipendente dalla risoluzione;
- layout per classi fisiche, non semplice zoom di pixel;
- allarmi cromatici esclusivi: rosso warning, ambra caution;
- cyan per selezioni e navigazione, verde per stato normale;
- failure flag esplicito e mai silenzioso.

## Token iniziali

| Token | Valore |
|---|---|
| background | `#071019` |
| panel | `#0B1722` |
| primary text | `#F4F7FA` |
| secondary text | `#9AAEBD` |
| cyan | `#00C8F0` |
| green | `#58D330` |
| amber | `#FFB000` |
| warning | `#FF3B30` |
| sky | `#087AC1` |
| earth | `#795126` |

## Classi layout

- Compact: 800x480, densità ridotta.
- Panel: 1024x600 / 1280x800.
- Cockpit: 1920x1080 o superiore.
- Round: 240-480 px, singolo strumento LVGL.

Il reference renderer SVG serve per validare geometria e golden image. Il
renderer operativo userà Qt Quick/RHI sul PFD e LVGL sugli ESP32-S3.

## Gerarchia PFD Professional

Il PFD usa livelli funzionali stabili, indipendenti dalla risoluzione:

1. **System header**: identità prodotto, profilo, sequenza e salute globale.
2. **FMA**: AP/FD, modo laterale, modo verticale, velocità e armamento NAV.
3. **Primary flight field**: assetto, pitch ladder, bank scale, slip indicator,
   flight director e simbolo aeromobile.
4. **Data rails**: IAS e ALT con bug selezionati e failure flag espliciti.
5. **Navigation field**: HSI/CDI e VSI sovrapposti senza coprire i riferimenti
   primari.
6. **System strip**: GS, track, course, vertical speed, sensori e validità NAV.
7. **Maintenance layer**: overlay touch separato dalla presentazione operativa.

## Estensioni operative v0.3

- heading tape a 60 gradi con bug course cyan;
- vettori di tendenza a sei secondi su IAS e ALT;
- BARO e radio-altimetro con origine della sorgente sempre dichiarata;
- selezione touch PFD/MFD persistente soltanto per la sessione corrente;
- shell MFD a schede per Flight, Navigation, Air Data, Map, FPV/Video e
  Platform. Le funzioni non ancora collegate riportano `RESERVED` o `READY FOR
  DATA` e non simulano disponibilità operativa.

La classe Panel mantiene tutti e sette i livelli riducendo etichette e
spaziature. La classe Cockpit aumenta densità e dettagli, non la semplice scala
dei componenti. Warning e caution possono occupare il centro dell'header/FMA,
ma non devono mai oscurare assetto, IAS o ALT.
