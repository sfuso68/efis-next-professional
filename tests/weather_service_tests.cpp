#include "weather_service.hpp"
#include "weather_position_binding.hpp"

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
#include <QBuffer>
#include <QImage>
#include <QFile>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

QByteArray radarPayload(char marker) {
    QImage image(256, 256, QImage::Format_ARGB32);
    image.fill(qRgba(static_cast<unsigned char>(marker), 60, 90, 255));
    QByteArray payload;
    QBuffer buffer(&payload);
    require(buffer.open(QIODevice::WriteOnly), "PNG fixture buffer must open");
    require(image.save(&buffer, "PNG"), "PNG fixture must encode");
    return payload;
}

class ControlledRadarServer final : public QObject {
public:
    explicit ControlledRadarServer(bool automatic = true) : automatic_(automatic) {
        QObject::connect(&server_, &QTcpServer::newConnection, this, [this] {
            while (auto* socket = server_.nextPendingConnection()) {
                buffers_.insert(socket, {});
                QObject::connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    auto& buffer = buffers_[socket];
                    buffer += socket->readAll();
                    if (!buffer.contains("\r\n\r\n")) return;
                    if (buffer.startsWith("HANDLED")) return;
                    const auto firstLine = buffer.left(buffer.indexOf("\r\n"));
                    buffer = "HANDLED";
                    requests_.push_back(socket);
                    if (firstLine.contains("layers=radar%3Avmi") || firstLine.contains("layers=radar:vmi")) vmi_ = socket;
                    if (firstLine.contains("layers=radar%3Asri") || firstLine.contains("layers=radar:sri")) sri_ = socket;
                    if (automatic_ && vmi_ && sri_ && !responses_scheduled_) {
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
    int requestsReceived() const { return static_cast<int>(requests_.size()); }
    void respondTo(int index, char marker) { respond(requests_.at(index), radarPayload(marker)); }

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
    QList<QPointer<QTcpSocket>> requests_;
    bool automatic_{};
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

struct PositionController {
    bool valid{};
    bool simulated{};
    mutable int reads{};
    bool positionValid() const { return valid; }
    bool positionSimulated() const { ++reads; return simulated; }
    double latitude() const { ++reads; return 45.648; }
    double longitude() const { ++reads; return 12.194; }
};
struct PositionSink {
    int invalidations{};
    int updates{};
    bool simulated{};
    void invalidatePosition() { ++invalidations; }
    void updatePosition(double lat, double lon, bool valid, bool sim) {
        require(lat == 45.648 && lon == 12.194 && valid, "binding must forward valid coordinates");
        ++updates;
        simulated = sim;
    }
};
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
        {
            PositionController controller;
            PositionSink sink;
            updateWeatherPosition(controller, sink);
            require(controller.reads == 0 && sink.invalidations == 1 && sink.updates == 0,
                    "invalid fix must not read coordinates or provenance");
            controller.valid = true;
            controller.simulated = true;
            updateWeatherPosition(controller, sink);
            require(controller.reads == 3 && sink.updates == 1 && sink.simulated,
                    "binding must forward position-specific simulation provenance");
            std::cout << "position_binding: PASS\n";
        }
        const auto png = radarPayload('T');
        require(WeatherService::validRadarPayload(png), "real 256x256 PNG must validate");
        require(!WeatherService::validRadarPayload(QByteArray("\x89PNG\r\n\x1a\n", 8) + QByteArray(600, 'x')),
                "signature-only payload must fail closed");
        require(!WeatherService::validRadarPayload(png.left(40)), "truncated PNG must fail closed");
        require(!WeatherService::validRadarPayload(QByteArray(4 * 1024 * 1024 + 1, 'x')),
                "oversized payload must fail closed");
        QImage wrongSize(512, 256, QImage::Format_ARGB32);
        wrongSize.fill(Qt::transparent);
        QByteArray wrongData;
        QBuffer wrongBuffer(&wrongData);
        require(wrongBuffer.open(QIODevice::WriteOnly) && wrongSize.save(&wrongBuffer, "PNG"),
                "wrong-dimension fixture must encode");
        require(!WeatherService::validRadarPayload(wrongData), "wrong dimensions must fail closed");
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
            QDir().mkpath(weatherCacheRoot);
            QFile invalidCache(weatherCacheRoot + QLatin1Char('/')
                + WeatherService::radarCacheFileName(QStringLiteral("VMI"), 6, 34, 22));
            require(invalidCache.open(QIODevice::WriteOnly), "invalid cache fixture must open");
            invalidCache.write("not a PNG");
            invalidCache.close();
            weather.updatePosition(45.648, 12.194, true, false);
            require(weather.radarSource().isEmpty(), "corrupt cached image must not become available");
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
            QDir(weatherCacheRoot).removeRecursively();
            QSettings().setValue(QStringLiteral("weather/product"), QStringLiteral("VMI"));
            ControlledRadarServer server(false);
            require(server.listen(), "position test server must listen");
            WeatherService weather(server.endpoint());
            weather.refresh();
            weather.cycleProduct();
            weather.cycleProduct();
            weather.updatePosition(45.648, 12.194, false, false);
            weather.updatePosition(45.648, 12.194, true, true);
            weather.updatePosition(std::numeric_limits<double>::quiet_NaN(), 12.194, true, false);
            weather.updatePosition(45.648, std::numeric_limits<double>::infinity(), true, false);
            weather.updatePosition(49, 12, true, false);
            weather.updatePosition(45, 22, true, false);
            waitUntil([] { return false; }, 100);
            require(server.requestsReceived() == 0 && weather.radarSource().isEmpty()
                        && weather.radarState() == QStringLiteral("NO DATA")
                        && weather.radarAgeMinutes() == -1
                        && weather.radarArea() == QStringLiteral("NO VALID POSITION"),
                    "startup, invalid, simulated and out-of-coverage positions must not request real radar");
            std::cout << "no_fix_no_fallback: PASS\n";

            weather.updatePosition(45.648, 12.194, true, false);
            require(waitUntil([&] { return server.requestsReceived() == 1; }, 1000),
                    "valid position must request radar");
            weather.invalidatePosition();
            server.respondTo(0, 'A');
            require(waitUntil([&] { return server.responsesSent() == 1; }, 1000), "lost-fix reply must complete");
            waitUntil([] { return false; }, 100);
            require(weather.radarSource().isEmpty() && QDir(weatherCacheRoot).entryList({"radar-*.png"}, QDir::Files).isEmpty(),
                    "reply arriving without fix must neither publish nor populate cache");
            std::cout << "late_reply_while_lost: PASS\n";

            weather.updatePosition(45.648, 12.194, true, false);
            require(waitUntil([&] { return server.requestsReceived() == 2; }, 1000), "first recovery must request");
            weather.invalidatePosition();
            weather.updatePosition(45.648, 12.194, true, false);
            require(waitUntil([&] { return server.requestsReceived() == 3; }, 1000), "same-tile recovery must request");
            server.respondTo(1, 'B');
            waitUntil([] { return false; }, 100);
            require(weather.radarSource().isEmpty() && QDir(weatherCacheRoot).entryList({"radar-*.png"}, QDir::Files).isEmpty(),
                    "old epoch reply after same-tile recovery must not publish or poison cache");
            server.respondTo(2, 'C');
            require(waitUntil([&] { return !weather.radarSource().isEmpty(); }, 1000), "new epoch must publish");
            require(weather.radarState() == QStringLiteral("CURRENT") && weather.radarArea().startsWith("GPS "),
                    "recovered real frame must carry real position area");
            std::cout << "same_tile_recovery_epoch: PASS\n";
            weather.updatePosition(45.648, 12.194, false, false);
            require(weather.radarSource().isEmpty() && weather.radarAgeMinutes() == -1
                        && weather.radarState() == QStringLiteral("NO DATA")
                        && weather.radarArea() == QStringLiteral("NO VALID POSITION"),
                    "fix loss must immediately clear visible frame, age and GPS area");
            weather.refresh();
            waitUntil([] { return false; }, 100);
            require(server.requestsReceived() == 3, "refresh without fix must not reuse old tile");
            std::cout << "loss_clears_visible_context: PASS\n";
        }
        {
            QSettings().setValue(QStringLiteral("weather/product"), QStringLiteral("VMI"));
            ControlledRadarServer server(false);
            require(server.listen(), "simulation test server must listen");
            WeatherService weather(server.endpoint());
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
            require(weather.radarArea() == QStringLiteral("SIMULATION AREA") && server.requestsReceived() == 0,
                    "explicit training must not claim a GPS area or request real radar");
            require(weather.simulationPhase() >= 1, "simulation phase did not advance");
            weather.cycleProduct();
            weather.cycleProduct();
            require(weather.radarProduct() == QStringLiteral("CAPPI 3000"),
                    "simulation must expose the CAPPI training product");
            weather.setSimulation(false);
            require(weather.radarProduct() == QStringLiteral("VMI"),
                    "leaving simulation must reject the simulation-only CAPPI product");
            waitUntil([] { return false; }, 100);
            require(weather.radarSource().isEmpty() && weather.radarState() == QStringLiteral("NO DATA")
                        && weather.radarArea() == QStringLiteral("NO VALID POSITION") && server.requestsReceived() == 0,
                    "leaving simulation without fix must not resurrect cached/default radar");
            std::cout << "explicit_training_without_fix: PASS\n";
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
