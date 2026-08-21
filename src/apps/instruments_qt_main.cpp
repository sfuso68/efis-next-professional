// Pagina analogica sul display vero.
//
// Applicazione a se' e non una pagina dentro efis-pfd-qt: gli strumenti
// entreranno nel selettore con la fase 3, quando esistera' la configurazione
// che li dispone. Fino ad allora serve poterli vedere a 1024x600 sul vetro del
// Raspberry, che e' l'unica prova valida per una modifica di interfaccia.
//
// L'ambiente si comanda con due variabili:
//   EFIS_KIOSK=1        schermo intero, senza decorazioni
//   EFIS_FAIL=attitude  toglie una grandezza, per guardare la bandierina

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSurfaceFormat>

#include <cstdlib>

int main(int argc, char** argv) {
    QGuiApplication app{argc, argv};

    QQmlApplicationEngine engine;
    const char* failure = std::getenv("EFIS_FAIL");
    engine.rootContext()->setContextProperty(
        QStringLiteral("efisInjectedFailure"),
        QString::fromLatin1(failure != nullptr ? failure : ""));
    engine.rootContext()->setContextProperty(
        QStringLiteral("efisKiosk"),
        std::getenv("EFIS_KIOSK") != nullptr);

    engine.loadFromModule("EfisInstruments", "InstrumentsMain");
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
