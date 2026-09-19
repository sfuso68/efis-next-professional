#include "pfd_controller.hpp"
#include "gpu_map_item.hpp"
#include "synthetic_vision_item.hpp"
#include "circular_gauge_item.hpp"
#include "connectivity_service.hpp"
#include "weather_service.hpp"
#include "sensor_bench_controller.hpp"

#include <QGuiApplication>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <qqml.h>

#include <memory>

int main(int argc, char* argv[]) {
    qSetMessagePattern(QStringLiteral(
        "%{time yyyy-MM-ddTHH:mm:ss.zzz} %{message}"));
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("EFIS Next PFD");
    const bool kioskMode = qEnvironmentVariableIntValue("EFIS_KIOSK") > 0;
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
    // Tutti gli oggetti esposti al QML devono vivere piu' a lungo dell'engine.
    // In particolare il pannello bench puo' essere ancora valutato durante lo
    // smontaggio della scena dopo una cattura automatica.
    SensorBenchController sensorBench;
    QQmlApplicationEngine engine;
    const auto directoryUrl = [](const char* environmentName, const QString& fallback) {
        const QString configured = qEnvironmentVariable(environmentName);
        const QString path = QDir(configured.isEmpty() ? fallback : configured).absolutePath();
        return QUrl::fromLocalFile(path + QLatin1Char('/')).toString();
    };
    engine.rootContext()->setContextProperty("pfd", &controller);
    engine.rootContext()->setContextProperty("connectivity", &connectivity);
    engine.rootContext()->setContextProperty("weather", &weather);
    engine.rootContext()->setContextProperty("efisKiosk", kioskMode);
    engine.rootContext()->setContextProperty(
        "efisTerrainGpuRoot",
        directoryUrl("EFIS_TERRAIN_GPU_ROOT", QDir::current().filePath("data/terrain/gpu")));
    engine.rootContext()->setContextProperty(
        "efisImageryTextureRoot",
        directoryUrl("EFIS_IMAGERY_TEXTURE_ROOT",
                     QDir::current().filePath("output/imagery/active-textures")));
    engine.rootContext()->setContextProperty("bench", &sensorBench);
    engine.rootContext()->setContextProperty(
        "startupPage", qEnvironmentVariable("EFIS_START_PAGE", "PFD"));
    engine.rootContext()->setContextProperty(
        "hmiGoldenDataState", qEnvironmentVariable("EFIS_HMI_GOLDEN_STATE", "SIM"));
    engine.rootContext()->setContextProperty(
        "hmiGoldenDataAgeMs", qEnvironmentVariableIntValue("EFIS_HMI_GOLDEN_AGE_MS"));
    const bool hmiGoldenMode = QCoreApplication::arguments().contains(
        QStringLiteral("--hmi-golden"));
    const bool skyViewGoldenMode = QCoreApplication::arguments().contains(
        QStringLiteral("--skyview-golden"));
    engine.loadFromModule("EfisNext", skyViewGoldenMode ? "SkyViewGoldenMain"
                                         : hmiGoldenMode ? "HmiGoldenMain" : "Main");
    if (engine.rootObjects().isEmpty()) return 1;

    qInfo().noquote() << "event=startup product=efis-next component=pfd version=0.1.0"
                      << "qt=" << qVersion() << "platform=" << QGuiApplication::platformName();

    if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().front())) {
        QSize requestedCaptureSize;
        const auto captureSize = qEnvironmentVariable("EFIS_CAPTURE_SIZE");
        const auto captureParts = captureSize.split(QLatin1Char('x'));
        if (captureParts.size() == 2) {
            bool okWidth = false, okHeight = false;
            const int width = captureParts.at(0).toInt(&okWidth);
            const int height = captureParts.at(1).toInt(&okHeight);
            if (okWidth && okHeight && width > 0 && height > 0) {
                requestedCaptureSize = QSize{width, height};
                window->resize(width, height);
            }
        }
        auto frames = std::make_shared<quint64>(0);
        auto snapshotEvents = std::make_shared<quint64>(0);
        auto elapsed = std::make_shared<QElapsedTimer>();
        auto previousAccepted = std::make_shared<quint64>(controller.ddpAccepted());
        elapsed->start();
        QObject::connect(window, &QQuickWindow::frameSwapped, window,
                         [frames] { ++(*frames); });
        QObject::connect(&controller, &PfdController::snapshotChanged, window,
                         [snapshotEvents] { ++(*snapshotEvents); });
        auto* metrics = new QTimer(window);
        metrics->setInterval(5000);
        QObject::connect(metrics, &QTimer::timeout, window,
                         [frames, snapshotEvents, elapsed, previousAccepted, &controller] {
            const auto milliseconds = elapsed->restart();
            const double presentationHz = milliseconds > 0
                ? static_cast<double>(*frames) * 1000.0 / static_cast<double>(milliseconds)
                : 0.0;
            const quint64 sequence = controller.sequence();
            const quint64 accepted = controller.ddpAccepted();
            const quint64 acceptedDelta = accepted >= *previousAccepted
                ? accepted - *previousAccepted : 0;
            const double snapshotHz = milliseconds > 0
                ? static_cast<double>(*snapshotEvents) * 1000.0
                    / static_cast<double>(milliseconds)
                : 0.0;
            const double ddpHz = milliseconds > 0
                ? static_cast<double>(acceptedDelta) * 1000.0 / static_cast<double>(milliseconds)
                : 0.0;
            qInfo().noquote() << "event=render_health presentation_hz="
                              << QString::number(presentationHz, 'f', 1)
                              << "presentation_interval_ms="
                              << QString::number(presentationHz > 0.0
                                      ? 1000.0 / presentationHz : 0.0, 'f', 2)
                              << "snapshot_hz=" << QString::number(snapshotHz, 'f', 1)
                              << "sequence=" << sequence
                              << "ddp_state=" << controller.ddpLinkState()
                              << "ddp_hz=" << QString::number(ddpHz, 'f', 1)
                              << "ddp_accepted=" << accepted
                              << "ddp_rejected=" << controller.ddpRejected()
                              << "ddp_gaps=" << controller.ddpSequenceGaps()
                              << "ddp_socket_errors=" << controller.ddpSocketErrors();
            *frames = 0;
            *snapshotEvents = 0;
            *previousAccepted = accepted;
        });
        metrics->start();

        auto eventLoopElapsed = std::make_shared<QElapsedTimer>();
        eventLoopElapsed->start();
        auto* eventLoopWatchdog = new QTimer(window);
        eventLoopWatchdog->setTimerType(Qt::PreciseTimer);
        eventLoopWatchdog->setInterval(100);
        QObject::connect(eventLoopWatchdog, &QTimer::timeout, window,
                         [eventLoopElapsed] {
            const auto milliseconds = eventLoopElapsed->restart();
            if (milliseconds >= 250) {
                qWarning().noquote()
                    << "event=event_loop_stall duration_ms=" << milliseconds;
            }
        });
        eventLoopWatchdog->start();

        const QString capturePath = qEnvironmentVariable("EFIS_CAPTURE");
        if (!capturePath.isEmpty()) {
            const int configuredDelay = qEnvironmentVariableIntValue("EFIS_CAPTURE_DELAY_MS");
            const int delay = configuredDelay > 0 ? configuredDelay : 2000;
            const bool exitAfterCapture = qEnvironmentVariableIntValue("EFIS_CAPTURE_EXIT") > 0;
            QTimer::singleShot(delay, window, [window, capturePath, exitAfterCapture,
                                               requestedCaptureSize] {
                QDir{}.mkpath(QFileInfo{capturePath}.absolutePath());
                QImage image = window->grabWindow();
                if (!image.isNull() && requestedCaptureSize.isValid()
                    && image.size() != requestedCaptureSize) {
                    image = image.scaled(requestedCaptureSize, Qt::IgnoreAspectRatio,
                                         Qt::SmoothTransformation);
                }
                const bool saved = !image.isNull() && image.save(capturePath);
                qInfo().noquote() << "event=pfd_capture path=" << capturePath
                                  << "saved=" << saved << "size=" << image.size()
                                  << "exit_after_capture=" << exitAfterCapture;
                if (exitAfterCapture)
                    QGuiApplication::exit(saved ? 0 : 2);
            });
        }
    }

    if (kioskMode) {
        if (auto* window = qobject_cast<QWindow*>(engine.rootObjects().front())) {
            window->setFlags(window->flags() | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
            window->showFullScreen();
            window->requestActivate();
            QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
            QObject::connect(window, &QWindow::visibilityChanged, window,
                             [window](QWindow::Visibility visibility) {
                if (visibility != QWindow::FullScreen) {
                    QTimer::singleShot(0, window, [window] {
                        window->showFullScreen();
                        window->requestActivate();
                    });
                }
            });
            QObject::connect(window, &QWindow::activeChanged, window, [window] {
                if (!window->isActive()) {
                    QTimer::singleShot(0, window, [window] { window->requestActivate(); });
                }
            });
        }
    }
    return app.exec();
}
