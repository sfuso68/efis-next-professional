#include "pfd_controller.hpp"
#include "gpu_map_item.hpp"
#include "synthetic_vision_item.hpp"
#include "circular_gauge_item.hpp"
#include "connectivity_service.hpp"
#include "weather_service.hpp"

#include <QGuiApplication>
#include <QCursor>
#include <QDebug>
#include <QElapsedTimer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>
#include <QQuickWindow>
#include <QTimer>
#include <qqml.h>

#include <memory>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("EFIS Next PFD");
    qmlRegisterType<GpuMapItem>("EfisNext.Native", 1, 0, "GpuMapItem");
    qmlRegisterType<SyntheticVisionItem>("EfisNext.Native", 1, 0, "SyntheticVisionItem");
    qmlRegisterType<CircularGaugeItem>("EfisNext.Native", 1, 0, "CircularGaugeItem");

    PfdController controller;
    ConnectivityService connectivity;
    WeatherService weather;
    weather.setSimulation(controller.aircraftProfileState() == QStringLiteral("SIMULATION"));
    weather.updatePosition(controller.latitude(), controller.longitude());
    QObject::connect(&controller, &PfdController::snapshotChanged, &weather, [&controller, &weather] {
        weather.updatePosition(controller.latitude(), controller.longitude());
    });
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("pfd", &controller);
    engine.rootContext()->setContextProperty("connectivity", &connectivity);
    engine.rootContext()->setContextProperty("weather", &weather);
    engine.rootContext()->setContextProperty(
        "startupPage", qEnvironmentVariable("EFIS_START_PAGE", "PFD"));
    engine.loadFromModule("EfisNext", "Main");
    if (engine.rootObjects().isEmpty()) return 1;

    qInfo().noquote() << "event=startup product=efis-next component=pfd version=0.1.0"
                      << "qt=" << qVersion() << "platform=" << QGuiApplication::platformName();

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
            qInfo().noquote() << "event=render_health fps=" << QString::number(fps, 'f', 1)
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
