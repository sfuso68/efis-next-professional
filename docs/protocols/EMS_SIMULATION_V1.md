# EMS Simulation V1

## Scopo

Sorgente deterministica per progettazione, test grafico e fault injection della
pagina Engine Monitoring System. Tutti i campioni usano `SourceId::simulation`
e `Validity::simulated`; non sono utilizzabili per il volo.

## Canali

- RPM e manifold pressure;
- oil pressure e oil temperature;
- fuel pressure e fuel flow;
- fuel left/right;
- bus voltage e alternator current;
- CHT ed EGT per quattro cilindri.

## Scenari

1. `NONE`
2. `LOW OIL PRESS`
3. `HIGH CHT 3`
4. `ALTERNATOR`
5. `LOW FUEL PRESS`

Il ciclo è controllabile dalla pagina EMS tramite il pannello Fault Injection.
Ogni scenario modifica il livello di salute `normal`, `caution` o `warning`.

## Separazione futura

Il modello `EngineSnapshot` non dipende da Qt né dal simulatore di volo. Un
provider reale potrà alimentarlo da moduli CAN, seriali o analogici senza
modificare la pagina EMS. Limiti, unità e configurazione per tipo motore saranno
spostati in un profilo aeromobile versionato prima del collegamento hardware.
