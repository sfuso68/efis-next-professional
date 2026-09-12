#include "weather_service.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImage>
#include <QPainter>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSettings>
#include <QUrl>
#include <QtMath>

namespace {
QNetworkRequest weatherRequest(const QUrl& url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("EFIS-Next/0.1 weather-service"));
    request.setTransferTimeout(12000);
    return request;
}
}

WeatherService::WeatherService(QObject* parent) : QObject(parent) {
    QSettings settings;
    radar_product_ = settings.value(QStringLiteral("weather/product"),QStringLiteral("VMI")).toString();
    audio_enabled_ = settings.value(QStringLiteral("weather/audioEnabled"),true).toBool();
    const QFileInfo cached(cachePath(QStringLiteral("radar-vmi-veneto.png")));
    if (cached.exists() && cached.size() > 0) {
        radar_time_ = cached.lastModified().toUTC();
        radar_source_ = QUrl::fromLocalFile(cached.absoluteFilePath()).toString();
        radar_message_ = QStringLiteral("Loaded persistent Radar-DPC cache");
        updateAgeState();
    }
    refresh_timer_.setInterval(5*60*1000);
    connect(&refresh_timer_, &QTimer::timeout, this, &WeatherService::refresh);
    refresh_timer_.start();
    age_timer_.setInterval(60*1000);
    connect(&age_timer_, &QTimer::timeout, this, &WeatherService::updateAgeState);
    age_timer_.start();
    simulation_timer_.setInterval(3000);
    connect(&simulation_timer_, &QTimer::timeout, this, &WeatherService::generateSimulation);
    QTimer::singleShot(0, this, &WeatherService::refresh);
}

QString WeatherService::cachePath(const QString& name) const {
    auto root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (root.isEmpty()) root = QDir::homePath()+QStringLiteral("/.cache/efis-next");
    QDir().mkpath(root+QStringLiteral("/weather"));
    return root+QStringLiteral("/weather/")+name;
}

int WeatherService::radarAgeMinutes() const {
    return radar_time_.isValid() ? static_cast<int>(radar_time_.secsTo(QDateTime::currentDateTimeUtc())/60) : -1;
}

void WeatherService::updateAgeState() {
    if (simulation_) { radar_state_ = QStringLiteral("SIMULATED"); emit changed(); return; }
    const int age = radarAgeMinutes();
    radar_state_ = age < 0 ? QStringLiteral("NO DATA") : age <= 10 ? QStringLiteral("CURRENT") : age <= 20 ? QStringLiteral("STALE") : QStringLiteral("EXPIRED");
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
    const auto filename = QStringLiteral("radar-sim-%1.png").arg(radar_product_.toLower());
    QSaveFile file(cachePath(filename));
    if (file.open(QIODevice::WriteOnly) && image.save(&file,"PNG") && file.commit()) {
        radar_time_ = QDateTime::currentDateTimeUtc();
        radar_source_ = QUrl::fromLocalFile(cachePath(filename)).toString()+QStringLiteral("?v=")+QString::number(++simulation_phase_);
        radar_state_ = QStringLiteral("SIMULATED");
        radar_message_ = QStringLiteral("Animated training weather • not observed data");
    }
    const int stage = forced_scenario_>=0 ? forced_scenario_ : (simulation_phase_/4)%4;
    updateScenario(stage);
    const QString category = stage==0 ? QStringLiteral("VFR") : stage==1 ? QStringLiteral("MVFR") : QStringLiteral("IFR");
    const QString conditions = stage==0 ? QStringLiteral("18008KT 9999 SCT035 24/17 Q1014") : stage==1 ? QStringLiteral("16012G20KT 6000 SHRA BKN018 21/18 Q1011") : stage==2 ? QStringLiteral("14018G28KT 2500 SHRA BKN007 19/18 Q1009") : QStringLiteral("14025G38KT 1500 +TSRA BKN005CB 18/17 Q1006");
    airport_weather_ = QVariantList{
        QVariantMap{{QStringLiteral("icao"),QStringLiteral("LIPH")},{QStringLiteral("metar"),QStringLiteral("SIM METAR LIPH 181950Z ")+conditions},{QStringLiteral("category"),category},{QStringLiteral("observed"),QStringLiteral("SIM")},{QStringLiteral("taf"),QStringLiteral("SIM TAF LIPH 181700Z 1818/1918 17010KT 9999 SCT030 TEMPO 1820/1902 4000 TSRA BKN012CB")},{QStringLiteral("simulated"),true}},
        QVariantMap{{QStringLiteral("icao"),QStringLiteral("LIPZ")},{QStringLiteral("metar"),QStringLiteral("SIM METAR LIPZ 181950Z 15011KT 8000 SCT020 22/18 Q1011")},{QStringLiteral("category"),QStringLiteral("MVFR")},{QStringLiteral("observed"),QStringLiteral("SIM")},{QStringLiteral("taf"),QStringLiteral("SIM TAF LIPZ 181700Z 1818/1918 16012KT 7000 SHRA SCT018 TEMPO 1821/1903 TSRA BKN010CB")},{QStringLiteral("simulated"),true}}
    };
    emit changed();
    const auto callbackMilliseconds = callbackElapsed.elapsed();
    if (callbackMilliseconds >= 100) {
        qWarning().noquote()
            << "event=callback_stall callback=weather_simulation duration_ms="
            << callbackMilliseconds;
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
    if (tileChanged) { tile_x_ = x; tile_y_ = y; if (!simulation_) requestRadar(); }
    if (tileChanged || labelChanged) emit changed();
}

void WeatherService::cycleProduct() {
    if (simulation_)
        radar_product_ = radar_product_ == QStringLiteral("VMI") ? QStringLiteral("SRI") : radar_product_ == QStringLiteral("SRI") ? QStringLiteral("CAPPI 3000") : QStringLiteral("VMI");
    else
        radar_product_ = radar_product_ == QStringLiteral("VMI") ? QStringLiteral("SRI") : QStringLiteral("VMI");
    if (simulation_) generateSimulation(); else requestRadar();
    QSettings().setValue(QStringLiteral("weather/product"),radar_product_);
    emit changed();
}

void WeatherService::requestRadar() {
    constexpr int zoom = 6;
    constexpr int count = 1 << zoom;
    constexpr double extent = 20037508.342789244;
    if (tile_x_ < 0 || tile_y_ < 0) { updatePosition(latitude_, longitude_); return; }
    const double minX = (static_cast<double>(tile_x_)/count*2.0-1.0)*extent;
    const double maxX = (static_cast<double>(tile_x_+1)/count*2.0-1.0)*extent;
    const double maxY = (1.0-static_cast<double>(tile_y_)/count*2.0)*extent;
    const double minY = (1.0-static_cast<double>(tile_y_+1)/count*2.0)*extent;
    const auto urlText = QStringLiteral("https://radar-geowebcache.protezionecivile.it/service/wms?service=WMS&version=1.1.1&request=GetMap&layers=radar:%1&styles=&format=image/png&transparent=true&tiled=true&srs=EPSG:3857&bbox=%2,%3,%4,%5&width=256&height=256")
        .arg(radar_product_.toLower()).arg(minX,0,'f',7).arg(minY,0,'f',7).arg(maxX,0,'f',7).arg(maxY,0,'f',7);
    const QUrl url(urlText);
    auto* reply = network_.get(weatherRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const auto data = reply->readAll();
        if (reply->error() == QNetworkReply::NoError && data.startsWith("\x89PNG") && data.size() > 512) {
            const auto filename = QStringLiteral("radar-%1-z6-%2-%3.png").arg(radar_product_.toLower()).arg(tile_x_).arg(tile_y_);
            QSaveFile file(cachePath(filename));
            if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit()) {
                radar_time_ = QDateTime::currentDateTimeUtc();
                radar_source_ = QUrl::fromLocalFile(cachePath(filename)).toString()+QStringLiteral("?v=")+QString::number(radar_time_.toMSecsSinceEpoch());
                radar_message_ = QStringLiteral("Radar-DPC %1 received").arg(radar_product_);
            }
        } else {
            radar_message_ = radar_source_.isEmpty() ? QStringLiteral("Radar unavailable • no cache") : QStringLiteral("Radar offline • cached frame retained");
        }
        updateAgeState();
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
