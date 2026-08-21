# ADR-006: la sezione motore come nodo

- Stato: accettata
- Data: 2026-08-21

## Contesto

Il RPi4 che ospita l'EFIS vecchio torna in servizio come banco della sezione
motore. Le due strade possibili erano installarci il PFD completo avviato sulla
pagina EMS, oppure costruire un processo che contenga solo il motore.

Il PFD completo richiede Quick3D, Network e Concurrent per terreno, meteo e
mappe: su un RPi4 sono il carico più pesante del display, e nessuno dei tre
serve a leggere un motore. Soprattutto, un guasto in una qualsiasi di quelle
parti fermerebbe anche gli strumenti motore.

## Decisione

La sezione motore diventa `efis-ems-qt`, un processo proprio. È
l'applicazione della ADR-002: i confini interni devono poter diventare confini
di processo senza cambiare il significato dei modelli pubblici, e qui il confine
esisteva già — `efis::engine::EngineSnapshot` non dipende né da Qt né dal
simulatore di volo.

Il nodo dichiara i propri componenti Qt e non chiede Quick3D, Network né
Concurrent.

`EmsController` pubblica esattamente le grandezze del motore. Non è una versione
ridotta di `PfdController`: è la superficie che la pagina EMS consuma, e le due
implementazioni sono verificabili l'una contro l'altra proprio perché la pagina
è la stessa.

## Vincolo: i file QML sono condivisi, non copiati

`efis-ems-qt` carica gli stessi `qml/AdaptiveEmsView.qml`,
`qml/CommandRail.qml`, `qml/ProfessionalCircularGauge.qml` e `qml/Theme.qml` del
PFD, dichiarati in un modulo con URI distinto.

È il vincolo che dà senso al nodo. Una copia della pagina renderebbe il banco
motore un ramo morto: ciò che vi si migliora resterebbe lì, e le due versioni
divergerebbero al primo intervento. Chi aggiunge una pagina al nodo la aggiunge
al modulo, non la duplica nella cartella.

Chiunque tocchi `qml/AdaptiveEmsView.qml` sta modificando due programmi, e deve
verificarli entrambi.

## Conseguenze

- una modifica alla pagina motore fatta sul RPi4 è già nel PFD del RPi5, senza
  travaso e senza allineamento manuale;
- il nodo si costruisce dove il PFD non si costruisce, perché chiede meno;
- `qml/AdaptiveEmsView.qml` acquista un secondo consumatore, quindi il contratto
  che si aspetta dal proprio `controller` diventa un'interfaccia vera fra due
  implementazioni invece che una convenzione interna a una sola.

## Il pannello resta di uno solo dei due

Rilevato collegandosi al nodo dopo aver preso la decisione, e va scritto qui
perché smentisce l'ipotesi con cui era stata presa. L'EFIS vecchio non è
un'applicazione web: è un'applicazione Qt che gira con `QT_QPA_PLATFORM=eglfs`,
cioè direttamente su KMS/DRM, e **prende il DRM master in esclusiva**. Il RPi4
non ha né Wayland né X, e il suo target di default è `multi-user.target`.

Quindi «separati e operativi» sul pannello non è possibile: il secondo programma
che parte non ottiene il display. La separazione resta vera per tutto il resto —
sorgenti, cartella, unità systemd, e i due servizi `efis-web` ed `efis-plan`,
che non toccano il pannello e continuano a girare accanto al nodo motore.

Il 21 agosto 2026 la scelta è stata dare il display al nodo EMS, disabilitando
`efis.service` all'avvio senza rimuovere nulla, e senza dichiarare
`Conflicts=efis.service` nell'unità. La mutua esclusione resta perciò a carico
di chi la ricorda: è una scelta, non una svista, e chi cambiasse idea trova in
`docs/deployment/RPI4_EMS.md` la ragione per cui aggiungerla sarebbe
difendibile.

Conseguenza operativa per l'interfaccia: senza compositore non esiste `grim`,
quindi la cattura dal display vero — che sul RPi5 è la sola verifica valida per
una modifica di layout — su questo nodo non si può fare con gli strumenti
esistenti. Finché non c'è una cattura interna al programma, la pagina motore si
verifica guardando il pannello.

## Limiti correnti

Il nodo si alimenta dal solo `EngineSimulationProvider`, come dichiara la
`docs/protocols/EMS_SIMULATION_V1.md`: nessun dato di questo nodo descrive un
motore reale. Limiti, unità e configurazione per tipo motore restano da spostare
nel profilo aeromobile versionato prima di qualsiasi collegamento hardware.
