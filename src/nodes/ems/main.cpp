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
#include <QImage>
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

    // Cattura interna.
    //
    // Sul RPi5 la prova valida per una modifica di layout e' la cattura dal
    // display vero con grim. Il nodo EMS gira su eglfs, senza compositore, e
    // grim li' non esiste: senza questa via la pagina motore si potrebbe
    // verificare solo guardando il pannello, cioe' da nessuna sessione remota e
    // in nessun registro. Con la piattaforma offscreen la cattura non richiede
    // il display, quindi non lo toglie a chi lo sta usando.
    //
    // EFIS_CAPTURE_SIZE esiste perche' la geometria di riferimento della pagina
    // e' 1024x600 ma il pannello di questo nodo e' 800x480: catturare alla
    // dimensione sbagliata proverebbe un layout che nessuno vedra'.
    const auto capturePath = qEnvironmentVariable("EFIS_CAPTURE");
    if (!capturePath.isEmpty()) {
        if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front())) {
            const auto size = qEnvironmentVariable("EFIS_CAPTURE_SIZE");
            const auto parts = size.split(QLatin1Char('x'));
            if (parts.size() == 2) {
                bool okWidth = false, okHeight = false;
                const int width = parts.at(0).toInt(&okWidth);
                const int height = parts.at(1).toInt(&okHeight);
                if (okWidth && okHeight && width > 0 && height > 0) {
                    window->resize(width, height);
                }
            }
            // L'attesa non e' cosmetica: la pagina va catturata quando il motore
            // e' gia' avviato, altrimenti si fotografano gli zeri iniziali.
            const int delay = qEnvironmentVariableIntValue("EFIS_CAPTURE_DELAY_MS") > 0
                ? qEnvironmentVariableIntValue("EFIS_CAPTURE_DELAY_MS") : 3000;
            QTimer::singleShot(delay, window, [window, capturePath] {
                const auto image = window->grabWindow();
                const bool saved = !image.isNull() && image.save(capturePath);
                qInfo().noquote() << "event=capture node=ems saved=" << saved
                                  << "path=" << capturePath
                                  << "size=" << image.width() << "x" << image.height();
                QGuiApplication::exit(saved ? 0 : 2);
            });
        }
    }

    if (qEnvironmentVariableIsSet("EFIS_KIOSK")) {
        if (auto* window = qobject_cast<QWindow*>(engine.rootObjects().front())) {
            window->showFullScreen();
            QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
        }
    }
    return app.exec();
}
