// Nodo EMS: la sola sezione motore, come processo indipendente.
//
// Condivide con il PFD completo il modello del motore, il provider e i file QML
// della pagina: non e' una copia della pagina EMS, e' la stessa pagina caricata
// da un processo che non porta con se' assetto, terreno, mappe e meteo. Cio' che
// si migliora qui compare nel PFD senza alcun travaso, perche' e' lo stesso file.

#include "ems_controller.hpp"
#include "circular_gauge_item.hpp"

#include <QCursor>
#include <QDebug>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QWindow>
#include <qqml.h>

#include <memory>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("EFIS Next EMS");
    qmlRegisterType<CircularGaugeItem>("EfisNext.Native", 1, 0, "CircularGaugeItem");

    EmsController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("ems", &controller);
    engine.loadFromModule("EfisNextEms", "EmsMain");
    if (engine.rootObjects().isEmpty()) return 1;

    qInfo().noquote() << "event=startup product=efis-next component=ems version=0.1.0"
                      << "qt=" << qVersion()
                      << "platform=" << QGuiApplication::platformName();

    // La stessa misura di salute del rendering del PFD: un nodo che disegna
    // strumenti motore a schermo intero deve poter dimostrare a che ritmo lo fa,
    // altrimenti un fermo immagine e un valore stabile si somigliano troppo.
    if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front())) {
        auto frames = std::make_shared<quint64>(0);
        auto elapsed = std::make_shared<QElapsedTimer>();
        elapsed->start();
        QObject::connect(window, &QQuickWindow::frameSwapped, window,
                         [frames] { ++(*frames); });
        auto* metrics = new QTimer(window);
        metrics->setInterval(5000);
        QObject::connect(metrics, &QTimer::timeout, window, [frames, elapsed, &controller] {
            const auto milliseconds = elapsed->restart();
            const double fps = milliseconds > 0
                ? static_cast<double>(*frames) * 1000.0 / static_cast<double>(milliseconds)
                : 0.0;
            qInfo().noquote() << "event=render_health node=ems fps="
                              << QString::number(fps, 'f', 1)
                              << "frame_ms=" << QString::number(fps > 0.0 ? 1000.0 / fps : 0.0, 'f', 2)
                              << "sequence=" << controller.sequence();
            *frames = 0;
        });
        metrics->start();
    }

    if (qEnvironmentVariableIsSet("EFIS_KIOSK")) {
        if (auto* window = qobject_cast<QWindow*>(engine.rootObjects().front())) {
            window->showFullScreen();
            QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
        }
    }
    return app.exec();
}
