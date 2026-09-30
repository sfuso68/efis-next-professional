#include "weather_service.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QPointer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSettings>
#include <QThread>
#include <QUrl>
#include <QtMath>

namespace {
QNetworkRequest weatherRequest(const QUrl& url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EFIS-Next/0.1 weather-service"));
    request.setTransferTimeout(12000);
    return request;
}

QString simulationPath(const QString& name) {
    auto root = qEnvironmentVariable("EFIS_WEATHER_SIMULATION_DIR");
    if (root.isEmpty()) root = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (root.isEmpty()) root = QDir::tempPath();
    root += QStringLiteral("/efis-next/weather-simulation");
    QDir().mkpath(root);
    return root + QLatin1Char('/') + name;
}
}

WeatherService::WeatherService(QObject* parent) : QObject(parent) {
    QSettings settings;
    radar_product_ = normalizedRadarProduct(
        settings.value(QStringLiteral("weather/product"), QStringLiteral("VMI")).toString());
    if (radar_product_ != QStringLiteral("VMI") && radar_product_ != QStringLiteral("SRI"))
        radar_product_ = QStringLiteral("VMI");
    audio_enabled_ = settings.value(QStringLiteral("weather/audioEnabled"),true).toBool();
    refresh_timer_.setInterval(5*60*1000);
    connect(&refresh_timer_, &QTimer::timeout, this, &WeatherService::refresh);
    refresh_timer_.start();
    age_timer_.setInterval(60*1000);
    connect(&age_timer_, &QTimer::timeout, this, &WeatherService::updateAgeState);
    age_timer_.start();
    simulation_timer_.setInterval(3000);
    connect(&simulation_timer_, &QTimer::timeout, this, &WeatherService::generateSimulation);
    simulation_worker_thread_.setObjectName(QStringLiteral("weather-simulation-writer"));
    simulation_worker_context_ = new QObject;
    simulation_worker_context_->moveToThread(&simulation_worker_thread_);
    connect(&simulation_worker_thread_, &QThread::finished, simulation_worker_context_, &QObject::deleteLater);
    simulation_worker_thread_.start();
    QTimer::singleShot(0, this, &WeatherService::refresh);
}

WeatherService::~WeatherService() {
    simulation_worker_thread_.quit();
    simulation_worker_thread_.wait();
}

QString WeatherService::cachePath(const QString& name) const {
    auto root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (root.isEmpty()) root = QDir::homePath()+QStringLiteral("/.cache/efis-next");
    QDir().mkpath(root+QStringLiteral("/weather"));
    return root+QStringLiteral("/weather/")+name;
}

int WeatherService::radarAgeMinutes() const {
    if (!radar_time_.isValid()) return -1;
    const auto ageSeconds = radar_time_.secsTo(QDateTime::currentDateTimeUtc());
    if (ageSeconds < -5) return -1;
    return static_cast<int>(qMax<qint64>(0, ageSeconds) / 60);
}

QString WeatherService::classifyRadarAgeSeconds(qint64 age_seconds) {
    if (age_seconds < -5) return QStringLiteral("CLOCK SKEW");
    const auto normalizedAge = qMax<qint64>(0, age_seconds);
    if (normalizedAge <= 600) return QStringLiteral("CURRENT");
    if (normalizedAge <= 1200) return QStringLiteral("STALE");
    return QStringLiteral("EXPIRED");
}

QString WeatherService::normalizedRadarProduct(const QString& product) {
    const auto candidate = product.trimmed().toUpper();
    if (candidate == QStringLiteral("VMI") || candidate == QStringLiteral("SRI")
        || candidate == QStringLiteral("CAPPI 3000"))
        return candidate;
    return {};
}

QString WeatherService::radarCacheFileName(const QString& product, int zoom, int x, int y) {
    auto normalizedProduct = normalizedRadarProduct(product).toLower();
    if (normalizedProduct.isEmpty() || zoom < 0 || x < 0 || y < 0) return {};
    normalizedProduct.replace(QLatin1Char(' '), QLatin1Char('-'));
    return QStringLiteral("radar-%1-z%2-%3-%4.png")
        .arg(normalizedProduct)
        .arg(zoom)
        .arg(x)
        .arg(y);
}

void WeatherService::clearRadarFrame(const QString& message) {
    radar_source_.clear();
    radar_time_ = {};
    radar_state_ = QStringLiteral("NO DATA");
    radar_message_ = message;
}

bool WeatherService::loadCurrentRadarCache() {
    constexpr int zoom = 6;
    if (tile_x_ < 0 || tile_y_ < 0) {
        clearRadarFrame(QStringLiteral("Radar unavailable • position unknown"));
        return false;
    }
    const auto filename = radarCacheFileName(radar_product_, zoom, tile_x_, tile_y_);
    if (filename.isEmpty()) {
        clearRadarFrame(QStringLiteral("Radar unavailable • invalid cache identity"));
        return false;
    }
    const QFileInfo cached(cachePath(filename));
    if (!cached.isFile() || cached.size() <= 0) {
        clearRadarFrame(QStringLiteral("Radar unavailable • no matching cache"));
        return false;
    }
    radar_time_ = cached.lastModified().toUTC();
    radar_source_ = QUrl::fromLocalFile(cached.absoluteFilePath()).toString()
        + QStringLiteral("?v=") + QString::number(cached.lastModified().toMSecsSinceEpoch());
    radar_message_ = QStringLiteral("Loaded matching Radar-DPC cache");
    updateAgeState();
    return true;
}

void WeatherService::updateAgeState() {
    if (simulation_) { radar_state_ = QStringLiteral("SIMULATED"); emit changed(); return; }
    radar_state_ = radar_time_.isValid()
        ? classifyRadarAgeSeconds(radar_time_.secsTo(QDateTime::currentDateTimeUtc()))
        : QStringLiteral("NO DATA");
    emit changed();
}

void WeatherService::refresh() {
    if (simulation_) { generateSimulation(); return; }
    requestRadar();
    requestMetar();
    requestTaf();
}

void WeatherService::setSimulation(bool enabled) {
    if (simulation_ == enabled) return;
    simulation_ = enabled;
    if (simulation_) {
        simulation_timer_.start();
        generateSimulation();
    } else {
        simulation_timer_.stop();
        simulation_frame_refresh_pending_ = false;
        if (radar_product_ != QStringLiteral("VMI") && radar_product_ != QStringLiteral("SRI")) {
            radar_product_ = QStringLiteral("VMI");
            QSettings().setValue(QStringLiteral("weather/product"), radar_product_);
        }
        radar_source_.clear();
        radar_time_ = {};
        airport_weather_.clear();
        refresh();
    }
    emit changed();
}

void WeatherService::toggleAnimation() {
    if (!simulation_) return;
    if (simulation_timer_.isActive()) simulation_timer_.stop(); else simulation_timer_.start();
    recordEvent(QStringLiteral("ANIMATION"), simulation_timer_.isActive()?QStringLiteral("PLAY"):QStringLiteral("PAUSE"));
    emit changed();
}

void WeatherService::acknowledgeAlert() {
    alert_acknowledged_ = true;
    recordEvent(QStringLiteral("ALERT_ACK"),scenario_);
    emit changed();
}

void WeatherService::toggleAudio() {
    audio_enabled_ = !audio_enabled_;
    QSettings().setValue(QStringLiteral("weather/audioEnabled"),audio_enabled_);
    recordEvent(QStringLiteral("AUDIO"),audio_enabled_?QStringLiteral("ENABLED"):QStringLiteral("MUTED"));
    emit changed();
}

void WeatherService::nextScenario() {
    if (!simulation_) return;
    forced_scenario_ = (forced_scenario_+1)%4;
    updateScenario(forced_scenario_);
    generateSimulation();
}

void WeatherService::recordEvent(const QString& type, const QString& detail) {
    QFile file(cachePath(QStringLiteral("weather-blackbox.jsonl")));
    if (!file.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)) return;
    const QJsonObject event{{QStringLiteral("utc"),QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("type"),type},{QStringLiteral("detail"),detail},{QStringLiteral("simulated"),simulation_}};
    file.write(QJsonDocument(event).toJson(QJsonDocument::Compact)); file.write("\n");
    ++weather_event_count_;
}

void WeatherService::updateScenario(int stage) {
    const QString next = stage==0?QStringLiteral("VFR"):stage==1?QStringLiteral("MVFR"):stage==2?QStringLiteral("IFR"):QStringLiteral("TSRA");
    if (next == scenario_) return;
    scenario_ = next;
    route_risk_ = stage<1?QStringLiteral("NONE"):stage==1?QStringLiteral("CAUTION"):QStringLiteral("HIGH");
    advisory_ = stage<1?QStringLiteral("ROUTE CLEAR"):stage==1?QStringLiteral("MONITOR • LIGHT PRECIP AHEAD"):stage==2?QStringLiteral("ADVISORY • DEVIATE RIGHT 15°"):QStringLiteral("WARNING • DEVIATE RIGHT 30°");
    suggested_heading_ = stage<2?254:stage==2?269:284;
    alternates_ = QVariantList{
        QVariantMap{{QStringLiteral("icao"),QStringLiteral("LIPZ")},{QStringLiteral("distance"),QStringLiteral("12 NM")},{QStringLiteral("category"),stage>=3?QStringLiteral("MVFR"):QStringLiteral("VFR")}},
        QVariantMap{{QStringLiteral("icao"),QStringLiteral("LIPA")},{QStringLiteral("distance"),QStringLiteral("29 NM")},{QStringLiteral("category"),QStringLiteral("VFR")}}
    };
    voice_alert_ = stage<2?QString():stage==2?QStringLiteral("Weather ahead. Consider right deviation."):QStringLiteral("Severe weather ahead. Avoid cell. Turn right thirty degrees.");
    alert_acknowledged_ = stage<2;
    sigmets_.clear();
    if (stage>=2) sigmets_.push_back(QVariantMap{{QStringLiteral("id"),QStringLiteral("SIM-SIGMET-01")},
        {QStringLiteral("phenomenon"),stage==3?QStringLiteral("SEV TS/CB"):QStringLiteral("EMBD TS")},
        {QStringLiteral("area"),QStringLiteral("LIPH–LIPZ CORRIDOR")},{QStringLiteral("valid"),QStringLiteral("TRAINING +30 MIN")}});
    recordEvent(QStringLiteral("SCENARIO"),scenario_+QStringLiteral(" ROUTE_RISK=")+route_risk_);
}

void WeatherService::generateSimulation() {
    if (!simulation_) return;
    QElapsedTimer callbackElapsed;
    callbackElapsed.start();
    ++simulation_phase_;
    const int stage = forced_scenario_>=0 ? forced_scenario_ : (simulation_phase_/4)%4;
    updateScenario(stage);
    const QString category = stage==0 ? QStringLiteral("VFR") : stage==1 ? QStringLiteral("MVFR") : QStringLiteral("IFR");
    const QString conditions = stage==0 ? QStringLiteral("18008KT 9999 SCT035 24/17 Q1014") : stage==1 ? QStringLiteral("16012G20KT 6000 SHRA BKN018 21/18 Q1011") : stage==2 ? QStringLiteral("14018G28KT 2500 SHRA BKN007 19/18 Q1009") : QStringLiteral("14025G38KT 1500 +TSRA BKN005CB 18/17 Q1006");
    airport_weather_ = QVariantList{
        QVariantMap{{QStringLiteral("icao"),QStringLiteral("LIPH")},{QStringLiteral("metar"),QStringLiteral("SIM METAR LIPH 181950Z ")+conditions},{QStringLiteral("category"),category},{QStringLiteral("observed"),QStringLiteral("SIM")},{QStringLiteral("taf"),QStringLiteral("SIM TAF LIPH 181700Z 1818/1918 17010KT 9999 SCT030 TEMPO 1820/1902 4000 TSRA BKN012CB")},{QStringLiteral("simulated"),true}},
        QVariantMap{{QStringLiteral("icao"),QStringLiteral("LIPZ")},{QStringLiteral("metar"),QStringLiteral("SIM METAR LIPZ 181950Z 15011KT 8000 SCT020 22/18 Q1011")},{QStringLiteral("category"),QStringLiteral("MVFR")},{QStringLiteral("observed"),QStringLiteral("SIM")},{QStringLiteral("taf"),QStringLiteral("SIM TAF LIPZ 181700Z 1818/1918 16012KT 7000 SHRA SCT018 TEMPO 1821/1903 TSRA BKN010CB")},{QStringLiteral("simulated"),true}}
    };
    emit changed();

    if (simulation_frame_in_flight_) {
        simulation_frame_refresh_pending_ = true;
        qWarning().noquote() << "event=weather_simulation_frame_deferred reason=encoder_busy phase=" << simulation_phase_;
    } else {
        QImage image(256, 256, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        const int drift = simulation_phase_ % 48;
        const auto drawCell = [&painter](const QPoint& center, int radius, const QColor& outer, const QColor& core) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(outer);
            painter.drawEllipse(center, radius, radius*3/5);
            painter.setBrush(core);
            painter.drawEllipse(center, radius/2, radius/3);
        };
        const bool cappi = radar_product_.startsWith(QStringLiteral("CAPPI"));
        drawCell(QPoint(52+drift,72),42,cappi?QColor(40,120,230,130):QColor(20,210,70,125),cappi?QColor(190,70,240,205):QColor(255,220,30,190));
        drawCell(QPoint(142+drift/2,126),34,cappi?QColor(40,150,230,135):QColor(20,190,60,130),QColor(245,55,35,215));
        drawCell(QPoint(205-drift/3,196),28,cappi?QColor(80,100,220,120):QColor(30,200,80,115),QColor(255,170,20,185));
        painter.end();

        const auto product = radar_product_;
        const auto phase = static_cast<quint64>(simulation_phase_);
        const auto framePath = simulationPath(QStringLiteral("radar-sim-%1.png").arg(product.toLower()));
        const int diagnosticDelay = qBound(0, qEnvironmentVariableIntValue("EFIS_WEATHER_SIMULATION_IO_DELAY_MS"), 5000);
        simulation_frame_in_flight_ = true;
        QPointer<WeatherService> service(this);
        const bool queued = QMetaObject::invokeMethod(simulation_worker_context_,
            [service, image = std::move(image), framePath, product, phase, diagnosticDelay]() mutable {
                QElapsedTimer workerElapsed;
                workerElapsed.start();
                if (diagnosticDelay > 0) QThread::msleep(static_cast<unsigned long>(diagnosticDelay));
                QSaveFile file(framePath);
                bool saved = false;
                QString error;
                if (!file.open(QIODevice::WriteOnly)) {
                    error = file.errorString();
                } else {
                    QImageWriter writer(&file, "PNG");
                    if (!writer.write(image)) {
                        error = writer.errorString();
                        file.cancelWriting();
                    } else if (!file.commit()) {
                        error = file.errorString();
                    } else {
                        saved = true;
                    }
                }
                const SimulationFrameResult result{framePath, product, phase, workerElapsed.elapsed(), saved, error};
                if (service) QMetaObject::invokeMethod(service,
                    [service, result] { if (service) service->finishSimulationFrame(result); },
                    Qt::QueuedConnection);
            }, Qt::QueuedConnection);
        if (!queued) {
            simulation_frame_in_flight_ = false;
            radar_message_ = QStringLiteral("Simulation frame worker unavailable");
            qWarning().noquote() << "event=weather_simulation_frame_queue_failed phase=" << phase;
            emit changed();
        }
    }
    const auto callbackMilliseconds = callbackElapsed.elapsed();
    if (callbackMilliseconds >= 100) {
        qWarning().noquote()
            << "event=callback_stall callback=weather_simulation duration_ms="
            << callbackMilliseconds;
    }
}

void WeatherService::finishSimulationFrame(const SimulationFrameResult& result) {
    simulation_frame_in_flight_ = false;
    if (result.worker_milliseconds >= 100)
        qWarning().noquote() << "event=weather_simulation_worker_slow duration_ms=" << result.worker_milliseconds << "phase=" << result.phase;
    if (simulation_ && result.saved && result.product == radar_product_) {
        radar_time_ = QDateTime::currentDateTimeUtc();
        radar_source_ = QUrl::fromLocalFile(result.path).toString()+QStringLiteral("?v=")+QString::number(result.phase);
        radar_state_ = QStringLiteral("SIMULATED");
        radar_message_ = QStringLiteral("Animated training weather • not observed data");
        emit changed();
    } else if (!result.saved) {
        radar_message_ = QStringLiteral("Simulation frame write failed: %1").arg(result.error);
        qWarning().noquote() << "event=weather_simulation_frame_write_failed phase=" << result.phase << "error=" << result.error;
        emit changed();
    }
    if (simulation_ && simulation_frame_refresh_pending_) {
        simulation_frame_refresh_pending_ = false;
        QTimer::singleShot(0, this, &WeatherService::generateSimulation);
    }
}

void WeatherService::updatePosition(double latitude, double longitude) {
    if (!qIsFinite(latitude) || !qIsFinite(longitude) || latitude < 35.0 || latitude > 48.0 || longitude < 4.0 || longitude > 21.0) return;
    latitude_ = latitude;
    longitude_ = longitude;
    constexpr int zoom = 6;
    constexpr int count = 1 << zoom;
    const int x = qBound(0, static_cast<int>(qFloor((longitude+180.0)/360.0*count)), count-1);
    const double latitudeRad = qDegreesToRadians(qBound(-85.05112878, latitude, 85.05112878));
    const int y = qBound(0, static_cast<int>(qFloor((1.0-qLn(qTan(latitudeRad)+1.0/qCos(latitudeRad))/M_PI)/2.0*count)), count-1);
    const auto area = QStringLiteral("GPS %1° %2°").arg(latitude, 0, 'f', 1).arg(longitude, 0, 'f', 1);
    const bool tileChanged = x != tile_x_ || y != tile_y_;
    const bool labelChanged = area != radar_area_;
    radar_area_ = area;
    if (tileChanged) {
        tile_x_ = x;
        tile_y_ = y;
        if (!simulation_) {
            loadCurrentRadarCache();
            requestRadar();
        }
    }
    if (tileChanged || labelChanged) emit changed();
}

void WeatherService::cycleProduct() {
    if (simulation_)
        radar_product_ = radar_product_ == QStringLiteral("VMI") ? QStringLiteral("SRI") : radar_product_ == QStringLiteral("SRI") ? QStringLiteral("CAPPI 3000") : QStringLiteral("VMI");
    else
        radar_product_ = radar_product_ == QStringLiteral("VMI") ? QStringLiteral("SRI") : QStringLiteral("VMI");
    if (simulation_) {
        generateSimulation();
    } else {
        loadCurrentRadarCache();
        requestRadar();
    }
    QSettings().setValue(QStringLiteral("weather/product"),radar_product_);
    emit changed();
}

void WeatherService::requestRadar() {
    constexpr int zoom = 6;
    constexpr int count = 1 << zoom;
    constexpr double extent = 20037508.342789244;
    if (tile_x_ < 0 || tile_y_ < 0) { updatePosition(latitude_, longitude_); return; }
    const int requestX = tile_x_;
    const int requestY = tile_y_;
    const auto requestProduct = radar_product_;
    const double minX = (static_cast<double>(tile_x_)/count*2.0-1.0)*extent;
    const double maxX = (static_cast<double>(tile_x_+1)/count*2.0-1.0)*extent;
    const double maxY = (1.0-static_cast<double>(tile_y_)/count*2.0)*extent;
    const double minY = (1.0-static_cast<double>(tile_y_+1)/count*2.0)*extent;
    const auto urlText = QStringLiteral("https://radar-geowebcache.protezionecivile.it/service/wms?service=WMS&version=1.1.1&request=GetMap&layers=radar:%1&styles=&format=image/png&transparent=true&tiled=true&srs=EPSG:3857&bbox=%2,%3,%4,%5&width=256&height=256")
        .arg(requestProduct.toLower()).arg(minX,0,'f',7).arg(minY,0,'f',7).arg(maxX,0,'f',7).arg(maxY,0,'f',7);
    const QUrl url(urlText);
    auto* reply = network_.get(weatherRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestProduct, requestX, requestY] {
        const auto data = reply->readAll();
        const bool requestIsCurrent = requestProduct == radar_product_
            && requestX == tile_x_ && requestY == tile_y_ && !simulation_;
        if (reply->error() == QNetworkReply::NoError && data.startsWith("\x89PNG") && data.size() > 512) {
            const auto filename = radarCacheFileName(requestProduct, 6, requestX, requestY);
            QSaveFile file(cachePath(filename));
            if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit()) {
                if (requestIsCurrent) {
                    radar_time_ = QDateTime::currentDateTimeUtc();
                    radar_source_ = QUrl::fromLocalFile(cachePath(filename)).toString()+QStringLiteral("?v=")+QString::number(radar_time_.toMSecsSinceEpoch());
                    radar_message_ = QStringLiteral("Radar-DPC %1 received").arg(requestProduct);
                }
            }
        } else if (requestIsCurrent) {
            radar_message_ = radar_source_.isEmpty() ? QStringLiteral("Radar unavailable • no cache") : QStringLiteral("Radar offline • cached frame retained");
        }
        if (requestIsCurrent) updateAgeState();
        reply->deleteLater();
    });
}

void WeatherService::requestMetar() {
    auto* reply = network_.get(weatherRequest(QUrl(QStringLiteral("https://aviationweather.gov/api/data/metar?ids=LIPH,LIPZ&format=json"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply->error() == QNetworkReply::NoError) {
            const auto document = QJsonDocument::fromJson(reply->readAll());
            if (document.isArray()) {
                QVariantList result;
                for (const auto& value : document.array()) {
                    const auto item = value.toObject();
                    result.push_back(QVariantMap{{QStringLiteral("icao"), item.value(QStringLiteral("icaoId")).toString()},
                        {QStringLiteral("metar"), item.value(QStringLiteral("rawOb")).toString()},
                        {QStringLiteral("category"), item.value(QStringLiteral("fltCat")).toString()},
                        {QStringLiteral("observed"), item.value(QStringLiteral("reportTime")).toString()},
                        {QStringLiteral("taf"), QString()}});
                }
                if (!result.isEmpty()) airport_weather_ = result;
            }
        }
        emit changed(); reply->deleteLater();
    });
}

void WeatherService::requestTaf() {
    auto* reply = network_.get(weatherRequest(QUrl(QStringLiteral("https://aviationweather.gov/api/data/taf?ids=LIPH,LIPZ&format=json"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        if (reply->error() == QNetworkReply::NoError) {
            const auto document = QJsonDocument::fromJson(reply->readAll());
            if (document.isArray()) {
                for (const auto& value : document.array()) {
                    const auto item = value.toObject();
                    const auto icao = item.value(QStringLiteral("icaoId")).toString();
                    bool merged = false;
                    for (int i=0; i<airport_weather_.size(); ++i) {
                        auto row = airport_weather_.at(i).toMap();
                        if (row.value(QStringLiteral("icao")).toString() == icao) {
                            row[QStringLiteral("taf")] = item.value(QStringLiteral("rawTAF")).toString();
                            airport_weather_[i] = row;
                            merged = true;
                        }
                    }
                    if (!merged) airport_weather_.push_back(QVariantMap{{QStringLiteral("icao"), icao},
                        {QStringLiteral("metar"), QString()}, {QStringLiteral("category"), QString()},
                        {QStringLiteral("observed"), QString()}, {QStringLiteral("taf"), item.value(QStringLiteral("rawTAF")).toString()}});
                }
            }
        }
        emit changed(); reply->deleteLater();
    });
}
