#include "weather_service.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

QByteArray radarPayload(char marker) {
    QByteArray payload("\x89PNG\r\n\x1a\n", 8);
    payload.append(600, marker);
    return payload;
}

class ControlledRadarServer final : public QObject {
public:
    explicit ControlledRadarServer(QObject* parent = nullptr) : QObject(parent) {
        QObject::connect(&server_, &QTcpServer::newConnection, this, [this] {
            while (auto* socket = server_.nextPendingConnection()) {
                buffers_.insert(socket, {});
                QObject::connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    auto& buffer = buffers_[socket];
                    buffer += socket->readAll();
                    if (!buffer.contains("\r\n\r\n")) return;
                    const auto firstLine = buffer.left(buffer.indexOf("\r\n"));
                    if (firstLine.contains("layers=radar%3Avmi") || firstLine.contains("layers=radar:vmi")) vmi_ = socket;
                    if (firstLine.contains("layers=radar%3Asri") || firstLine.contains("layers=radar:sri")) sri_ = socket;
                    if (vmi_ && sri_ && !responses_scheduled_) {
                        responses_scheduled_ = true;
                        QTimer::singleShot(0, this, [this] { respond(sri_, radarPayload('S')); });
                        QTimer::singleShot(150, this, [this] { respond(vmi_, radarPayload('V')); });
                    }
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }

    bool listen() { return server_.listen(QHostAddress::LocalHost, 0); }
    QUrl endpoint() const {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/service/wms").arg(server_.serverPort()));
    }
    int responsesSent() const { return responses_sent_; }

private:
    void respond(QTcpSocket* socket, const QByteArray& payload) {
        if (!socket || socket->state() == QAbstractSocket::UnconnectedState) return;
        const auto header = QByteArray("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: ")
            + QByteArray::number(payload.size()) + QByteArray("\r\nConnection: close\r\n\r\n");
        socket->write(header + payload);
        socket->disconnectFromHost();
        ++responses_sent_;
    }

    QTcpServer server_;
    QHash<QTcpSocket*, QByteArray> buffers_;
    QPointer<QTcpSocket> vmi_;
    QPointer<QTcpSocket> sri_;
    bool responses_scheduled_{};
    int responses_sent_{};
};

bool waitUntil(const std::function<bool()>& predicate, int timeoutMilliseconds) {
    QElapsedTimer elapsed;
    elapsed.start();
    while (!predicate() && elapsed.elapsed() < timeoutMilliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return predicate();
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("efis-weather-service-tests"));
    QStandardPaths::setTestModeEnabled(true);
    qputenv("EFIS_WEATHER_SIMULATION_IO_DELAY_MS", "400");
    const auto outputRoot = QDir::currentPath() + QStringLiteral("/weather-service-test-runtime");
    QDir(outputRoot).removeRecursively();
    qputenv("EFIS_WEATHER_SIMULATION_DIR", outputRoot.toUtf8());
    const auto weatherCacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + QStringLiteral("/weather");
    QDir(weatherCacheRoot).removeRecursively();

    try {
        require(WeatherService::classifyRadarAgeSeconds(-6) == QStringLiteral("CLOCK SKEW"),
                "future radar timestamp outside tolerance must fail closed");
        require(WeatherService::classifyRadarAgeSeconds(-5) == QStringLiteral("CURRENT"),
                "small clock jitter must remain inside tolerance");
        require(WeatherService::classifyRadarAgeSeconds(600) == QStringLiteral("CURRENT"),
                "600 seconds must remain CURRENT");
        require(WeatherService::classifyRadarAgeSeconds(601) == QStringLiteral("STALE"),
                "601 seconds must become STALE");
        require(WeatherService::classifyRadarAgeSeconds(1200) == QStringLiteral("STALE"),
                "1200 seconds must remain STALE");
        require(WeatherService::classifyRadarAgeSeconds(1201) == QStringLiteral("EXPIRED"),
                "1201 seconds must become EXPIRED");
        require(WeatherService::radarCacheFileName(QStringLiteral("VMI"), 6, 34, 22)
                    == QStringLiteral("radar-vmi-z6-34-22.png"),
                "VMI cache key must include product, zoom and tile");
        require(WeatherService::radarCacheFileName(QStringLiteral("CAPPI 3000"), 6, 34, 22)
                    == QStringLiteral("radar-cappi-3000-z6-34-22.png"),
                "cache product must be normalized without losing identity");
        require(WeatherService::radarCacheFileName(QStringLiteral("SRI"), 6, 34, 22)
                    != WeatherService::radarCacheFileName(QStringLiteral("VMI"), 6, 34, 22),
                "different products must never share a cache key");
        require(WeatherService::normalizedRadarProduct(QStringLiteral(" sri ")) == QStringLiteral("SRI"),
                "known products must normalize deterministically");
        require(WeatherService::radarCacheFileName(QStringLiteral("../VMI"), 6, 34, 22).isEmpty(),
                "unknown or path-like product names must fail closed");
        require(WeatherService::radarCacheFileName(QStringLiteral("VMI"), 6, -1, 22).isEmpty(),
                "negative tile coordinates must fail closed");
        {
            ControlledRadarServer server;
            require(server.listen(), "controlled radar server did not listen");
            QSettings().setValue(QStringLiteral("weather/product"), QStringLiteral("VMI"));
            WeatherService weather(server.endpoint());
            weather.updatePosition(45.648, 12.194);
            weather.cycleProduct();
            require(weather.radarProduct() == QStringLiteral("SRI"),
                    "controlled race did not select SRI");
            require(waitUntil([&] { return server.responsesSent() == 2; }, 3000),
                    "controlled radar server did not complete both responses");
            const auto vmiCache = weatherCacheRoot + QLatin1Char('/')
                + WeatherService::radarCacheFileName(QStringLiteral("VMI"), 6, 34, 22);
            const auto sriCache = weatherCacheRoot + QLatin1Char('/')
                + WeatherService::radarCacheFileName(QStringLiteral("SRI"), 6, 34, 22);
            require(waitUntil([&] { return QFileInfo::exists(vmiCache) && QFileInfo::exists(sriCache); }, 3000),
                    "controlled responses were not committed to their isolated cache entries");
            require(waitUntil([&] { return weather.radarState() == QStringLiteral("CURRENT"); }, 1000),
                    "current SRI response was not published");
            require(weather.radarSource().contains(QStringLiteral("radar-sri-z6-")),
                    "current SRI response did not own the visible frame");
            require(!weather.radarSource().contains(QStringLiteral("radar-vmi-z6-")),
                    "superseded VMI response replaced the visible SRI frame");
            require(QFileInfo::exists(vmiCache),
                    "superseded VMI response was not isolated in its own cache entry");
            require(QFileInfo::exists(sriCache),
                    "current SRI response was not cached");
        }
        {
            QSettings().setValue(QStringLiteral("weather/product"), QStringLiteral("VMI"));
            WeatherService weather;
            QElapsedTimer callback;
            callback.start();
            weather.setSimulation(true);
            require(callback.elapsed() < 150,
                    "simulation callback blocked on the injected worker I/O delay");

            QEventLoop loop;
            QTimer deadline;
            deadline.setSingleShot(true);
            QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(&weather, &WeatherService::changed, &loop, [&] {
                if (!weather.radarSource().isEmpty())
                    loop.quit();
            });
            deadline.start(3000);
            loop.exec();

            if (weather.radarSource().isEmpty()) {
                std::cerr << "weather state=" << weather.radarState().toStdString()
                          << " message=" << weather.radarMessage().toStdString()
                          << " phase=" << weather.simulationPhase() << '\n';
                throw std::runtime_error("asynchronous simulation frame was not published");
            }
            require(weather.radarSource().contains(QStringLiteral("radar-sim-vmi.png")),
                    "unexpected simulation frame path");
            require(weather.radarState() == QStringLiteral("SIMULATED"),
                    "simulation frame did not publish SIMULATED state");
            require(weather.simulationPhase() >= 1, "simulation phase did not advance");
            weather.cycleProduct();
            weather.cycleProduct();
            require(weather.radarProduct() == QStringLiteral("CAPPI 3000"),
                    "simulation must expose the CAPPI training product");
            weather.setSimulation(false);
            require(weather.radarProduct() == QStringLiteral("VMI"),
                    "leaving simulation must reject the simulation-only CAPPI product");
        }
        QDir(outputRoot).removeRecursively();
        QDir(weatherCacheRoot).removeRecursively();
        std::cout << "weather_service_tests: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "weather_service_tests: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
