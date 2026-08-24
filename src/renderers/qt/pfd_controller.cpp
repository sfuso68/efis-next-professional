#include "pfd_controller.hpp"
#include "efis/recording/recorder_status.hpp"

#include <QString>
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QDateTime>
#include <QTextStream>
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <QList>
#include <QUrl>

#include <utility>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <vector>

using namespace std::chrono_literals;

namespace {
double calibratedFuel(double raw, const QVariantList& curve) {
    if (curve.size() < 5) return qMax(0.0, raw);
    std::vector<std::pair<double,double>> points;
    for (const auto& entry : curve) { const auto p=entry.toMap(); points.emplace_back(p.value("raw").toDouble(),p.value("gallons").toDouble()); }
    std::sort(points.begin(),points.end());
    if (raw<=points.front().first) return qMax(0.0,points.front().second);
    if (raw>=points.back().first) return qMax(0.0,points.back().second);
    for (std::size_t i=1;i<points.size();++i) if(raw<=points[i].first){const auto& a=points[i-1];const auto& b=points[i];const double d=b.first-a.first;if(std::abs(d)<1e-9)return qMax(0.0,b.second);return qMax(0.0,a.second+(raw-a.first)*(b.second-a.second)/d);}
    return qMax(0.0,raw);
}
}

PfdController::PfdController(QObject* parent) : QObject(parent) {
    loadUnitSystem();
    // La simulazione resta registrata sempre: senza sensori collegati e' cio'
    // che permette di continuare a sviluppare, e con i sensori collegati copre
    // le grandezze che non forniscono. Il ruolo predefinito e' il ripiego, non
    // la precedenza: il dato reale, quando c'e', vince.
    router_.set_simulation(simulation_);
    engine_router_.set_simulation(engine_simulation_);

    // Il ruolo iniziale e' configurabile dall'ambiente, cosi' il banco puo'
    // partire senza dato simulato e vedere cosa il quadro dichiara quando non
    // ha sorgenti: e' la prova che le celle di stato sono calcolate e non
    // scritte a mano. Il valore predefinito resta il ripiego.
    const QString role = qEnvironmentVariable("EFIS_SIMULATION_ROLE").toUpper();
    const auto chosen =
        role == QStringLiteral("DISABLED") ? efis::SimulationRole::disabled
        : role == QStringLiteral("PRIMARY") ? efis::SimulationRole::primary
                                            : efis::SimulationRole::fallback;
    applySimulationRole(chosen);
    loadAircraftProfile();
    loadMaintenance();
    loadFuelComputer();
    fuel_event_log_path_=qEnvironmentVariable("EFIS_BLACKBOX_FUEL_EVENTS","state/blackbox-fuel-events.jsonl");
    loadFuelEventHistory();
    central_blackbox_cache_path_=qEnvironmentVariable("EFIS_CENTRAL_BLACKBOX_CACHE",QStringLiteral("/var/lib/efis/fuel-central-cache.json"));
    central_blackbox_request_path_=qEnvironmentVariable("EFIS_CENTRAL_BLACKBOX_REQUEST",QStringLiteral("/var/lib/efis/fuel-maintenance-request.jsonl"));
    loadCatalogIndex();
    loadNavigationIndex();
    loadRadionavigationIndex();
    loadImageryStatus();
    recorder_status_path_ = qEnvironmentVariable("EFIS_RECORDER_STATUS",
                                                 "run/recorder.status");
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &PfdController::persistFuelComputer);
    timer_.setTimerType(Qt::PreciseTimer);
    timer_.setInterval(20);
    connect(&timer_, &QTimer::timeout, this, &PfdController::updateSnapshot);
    updateSnapshot();
    timer_.start();
}

void PfdController::loadUnitSystem() {
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-units")};
    const auto value = settings.value(QStringLiteral("display/unitSystem"),
                                      QStringLiteral("AVIATION")).toString().toUpper();
    if (value == QStringLiteral("AVIATION") || value == QStringLiteral("METRIC") ||
        value == QStringLiteral("IMPERIAL")) unit_system_ = value;
}

bool PfdController::setUnitSystem(const QString& system) {
    const auto value = system.trimmed().toUpper();
    if (value != QStringLiteral("AVIATION") && value != QStringLiteral("METRIC") &&
        value != QStringLiteral("IMPERIAL")) return false;
    if (unit_system_ == value) return true;
    unit_system_ = value;
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-units")};
    settings.setValue(QStringLiteral("display/unitSystem"), unit_system_);
    settings.sync();
    qInfo().noquote() << "event=ems_unit_system system=" << unit_system_;
    emit unitsChanged();
    emit snapshotChanged();
    return settings.status() == QSettings::NoError;
}

void PfdController::loadFuelComputer() {
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-fuel-computer")};
    fuel_used_gallons_ = qMax(0.0, settings.value(QStringLiteral("real/used_gallons"), 0.0).toDouble());
    fuel_reserve_gallons_ = qBound(0.0, settings.value(QStringLiteral("pilot/reserve_gallons"), 5.0).toDouble(), 100.0);
    fuel_on_board_initialized_ = settings.contains(QStringLiteral("pilot/on_board_gallons"));
    fuel_on_board_gallons_ = qBound(0.0, settings.value(QStringLiteral("pilot/on_board_gallons"), 0.0).toDouble(), 100.0);
    fuel_calibration_left_ = settings.value(QStringLiteral("calibration/left_points")).toList();
    fuel_calibration_right_ = settings.value(QStringLiteral("calibration/right_points")).toList();
}

void PfdController::persistFuelComputer() {
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-fuel-computer")};
    settings.setValue(QStringLiteral("real/used_gallons"), fuel_used_gallons_);
    settings.setValue(QStringLiteral("pilot/reserve_gallons"), fuel_reserve_gallons_);
    settings.setValue(QStringLiteral("pilot/on_board_gallons"), fuel_on_board_gallons_);
    settings.setValue(QStringLiteral("calibration/left_points"), fuel_calibration_left_);
    settings.setValue(QStringLiteral("calibration/right_points"), fuel_calibration_right_);
    settings.sync();
}

bool PfdController::setFuelReserveGallons(double gallons) {
    if (!std::isfinite(gallons)) return false;
    fuel_reserve_gallons_ = qBound(0.0, gallons, 100.0);
    persistFuelComputer(); emit fuelComputerChanged(); return true;
}

bool PfdController::setFuelOnBoardGallons(double gallons) {
    if (!std::isfinite(gallons)) return false;
    fuel_on_board_gallons_ = qBound(0.0, gallons, 100.0);
    fuel_on_board_initialized_ = true;
    fuel_used_gallons_ = 0.0;
    simulated_fuel_used_gallons_ = 0.0;
    persistFuelComputer(); emit fuelComputerChanged(); return true;
}

bool PfdController::captureFuelCalibrationPoint(const QString& tank) {
    const bool left=tank.trimmed().toUpper()==QStringLiteral("LEFT");
    const bool right=tank.trimmed().toUpper()==QStringLiteral("RIGHT"); if(!left&&!right)return false;
    auto& curve=left?fuel_calibration_left_:fuel_calibration_right_; if(curve.size()>=5)return false;
    const double raw=left?engine_snapshot_.fuel_left_gal.value:engine_snapshot_.fuel_right_gal.value;
    if(!curve.isEmpty()&&std::abs(raw-curve.constLast().toMap().value(QStringLiteral("raw")).toDouble())<0.01)return false;
    const double capacity=qMax(0.1,fuel_on_board_gallons_/2.0);
    QVariantMap point; point.insert(QStringLiteral("percent"),curve.size()*25); point.insert(QStringLiteral("raw"),raw); point.insert(QStringLiteral("gallons"),capacity*curve.size()/4.0); curve.append(point);
    persistFuelComputer(); emit fuelComputerChanged(); return true;
}

bool PfdController::clearFuelCalibration(const QString& tank) {
    const auto normalized=tank.trimmed().toUpper(); if(normalized==QStringLiteral("LEFT"))fuel_calibration_left_.clear();else if(normalized==QStringLiteral("RIGHT"))fuel_calibration_right_.clear();else return false;
    persistFuelComputer(); emit fuelComputerChanged(); return true;
}

void PfdController::loadFuelEventHistory() {
    fuel_event_history_.clear(); QFile file(fuel_event_log_path_); if(!file.open(QIODevice::ReadOnly|QIODevice::Text))return;
    while(!file.atEnd()){const auto doc=QJsonDocument::fromJson(file.readLine());if(doc.isObject())fuel_event_history_.append(doc.object().toVariantMap());if(fuel_event_history_.size()>50)fuel_event_history_.removeFirst();}
}

void PfdController::recordFuelEvent(const QString& state) {
    QVariantMap event{{QStringLiteral("utc"),QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{QStringLiteral("state"),state},{QStringLiteral("rpm"),engine_snapshot_.rpm.value},{QStringLiteral("left_gal"),fuelLeft()},{QStringLiteral("right_gal"),fuelRight()},{QStringLiteral("remaining_gal"),fuelRemainingGallons()},{QStringLiteral("flow_gph"),engine_snapshot_.fuel_flow_gph.value},{QStringLiteral("excess_gph"),fuel_leak_excess_gph_},{QStringLiteral("simulated"),engine_snapshot_.fuel_flow_gph.is_simulated()}};
    QDir().mkpath(QFileInfo(fuel_event_log_path_).absolutePath()); QFile file(fuel_event_log_path_); if(file.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)){file.write(QJsonDocument(QJsonObject::fromVariantMap(event)).toJson(QJsonDocument::Compact));file.write("\n");file.flush();}
    fuel_event_history_.append(event);if(fuel_event_history_.size()>50)fuel_event_history_.removeFirst();
}

void PfdController::loadCentralBlackBoxCache() {
    QFileInfo info(central_blackbox_cache_path_); if(!info.exists()){central_blackbox_state_=QStringLiteral("OFFLINE");return;}
    const qint64 age=info.lastModified().msecsTo(QDateTime::currentDateTime()); QFile file(central_blackbox_cache_path_); if(!file.open(QIODevice::ReadOnly)){central_blackbox_state_=QStringLiteral("OFFLINE");return;}
    const auto root=QJsonDocument::fromJson(file.readAll()).object(); central_blackbox_nodes_=root.value("health").toObject().value("nodes").toArray().toVariantList();central_blackbox_maintenance_=root.value("health").toObject().value("maintenance").toObject().toVariantMap();central_blackbox_archives_=root.value("archives").toObject().value("archives").toArray().toVariantList(); central_fuel_events_.clear();
    for(const auto& item:root.value("events").toObject().value("events").toArray()){auto outer=item.toObject();auto event=outer.value("event").toObject().toVariantMap();event.insert("source",outer.value("source").toString());event.insert("received_utc",outer.value("received_utc").toString());central_fuel_events_.append(event);}
    central_blackbox_state_=age<=10000?QStringLiteral("ONLINE"):QStringLiteral("LINK LOST");
}

bool PfdController::requestCentralBlackBoxAction(const QString& action) {
    const auto normalized=action.trimmed().toUpper();if(normalized!=QStringLiteral("CREATE_INCIDENT")&&normalized!=QStringLiteral("VERIFY_SIGNATURES")&&normalized!=QStringLiteral("EXPORT_USB"))return false;
    QDir().mkpath(QFileInfo(central_blackbox_request_path_).absolutePath());QFile file(central_blackbox_request_path_);if(!file.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text))return false;
    QJsonObject request{{"action",normalized},{"request_id",QString::number(QDateTime::currentMSecsSinceEpoch())}};file.write(QJsonDocument(request).toJson(QJsonDocument::Compact));file.write("\n");file.flush();return true;
}

void PfdController::updateFuelLeakMonitor(efis::MonoTime now) {
    const double measured=fuelLeft()+fuelRight();
    auto setState=[this](const QString& state){if(fuel_leak_state_==state)return;fuel_leak_state_=state;recordFuelEvent(state);qInfo().noquote()<<"event=fuel_leak_monitor state="<<state<<"excess_gph="<<fuel_leak_excess_gph_;emit fuelComputerChanged();};
    if(fuel_leak_last_update_==efis::MonoTime{}){fuel_leak_last_update_=now;fuel_leak_last_measured_=measured;setState(QStringLiteral("INITIALIZING"));return;}
    const double seconds=std::chrono::duration<double>(now-fuel_leak_last_update_).count(); if(seconds<1.0)return;
    const double actualDrop=fuel_leak_last_measured_-measured;
    fuel_leak_last_update_=now;fuel_leak_last_measured_=measured;
    const bool calibrated=fuel_calibration_left_.size()==5&&fuel_calibration_right_.size()==5;
    if(!calibrated&&!engine_snapshot_.fuel_left_gal.is_simulated()){fuel_leak_excess_gph_=0;fuel_leak_alarm_since_={};setState(QStringLiteral("CALIBRATION REQUIRED"));return;}
    if(actualDrop < -0.15){fuel_leak_suppressed_until_=now+120s;fuel_leak_excess_gph_=0;fuel_leak_alarm_since_={};setState(QStringLiteral("REFUEL DETECTED"));return;}
    if(std::abs(actualDrop)>0.20){fuel_leak_suppressed_until_=now+20s;fuel_leak_excess_gph_=0;fuel_leak_alarm_since_={};setState(QStringLiteral("MOTION SUPPRESSED"));return;}
    if(now<fuel_leak_suppressed_until_){setState(QStringLiteral("SUPPRESSED"));return;}
    if(engine_snapshot_.rpm.value<400.0){fuel_leak_excess_gph_=0;fuel_leak_alarm_since_={};setState(QStringLiteral("STANDBY"));return;}
    const double expected=engine_snapshot_.fuel_flow_gph.value*seconds/3600.0;
    const double sample=qBound(-20.0,(actualDrop-expected)*3600.0/seconds,20.0);
    const double alpha=1.0-std::exp(-seconds/45.0); fuel_leak_excess_gph_+=(sample-fuel_leak_excess_gph_)*alpha;
    const double threshold=fuel_leak_excess_gph_>5.0?5.0:2.0;
    if(fuel_leak_excess_gph_<=2.0){fuel_leak_alarm_since_={};setState(QStringLiteral("MONITORING"));return;}
    if(fuel_leak_alarm_since_==efis::MonoTime{})fuel_leak_alarm_since_=now;
    const double confirmed=std::chrono::duration<double>(now-fuel_leak_alarm_since_).count();
    setState(confirmed>=(threshold>2.0?10.0:30.0)?(threshold>2.0?QStringLiteral("FUEL LEAK WARNING"):QStringLiteral("POSSIBLE LEAK")):QStringLiteral("VERIFYING"));
}

void PfdController::resetFuelUsed() {
    if (engine_snapshot_.fuel_flow_gph.is_simulated()) simulated_fuel_used_gallons_ = 0.0;
    else { fuel_used_gallons_ = 0.0; persistFuelComputer(); }
    emit fuelComputerChanged();
}

void PfdController::loadImageryStatus() {
    const QString configured = qEnvironmentVariable(
        "EFIS_IMAGERY_ACTIVE_PACKAGE", "output/imagery/active-package.json");
    const QDir app{QCoreApplication::applicationDirPath()};
    const QStringList candidates{configured, QDir::current().filePath(configured),
                                 app.filePath(QStringLiteral("../../") + configured)};
    for (const auto& path : candidates) {
        QFile file{path};
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) continue;
        const auto root = document.object();
        imagery_state_ = root.value("state").toString(QStringLiteral("INVALID"));
        imagery_offline_ready_ = root.value("offline_ready").toBool(false);
        imagery_tile_count_ = root.value("verified_tile_count").toInt();
        imagery_coverage_gaps_ = root.value("coverage_gap_tile_count").toInt();
        const QString atlas = QDir::current().filePath(
            QStringLiteral("output/imagery/atlases/treviso-belluno-z12.png"));
        if (QFileInfo::exists(atlas)) imagery_atlas_source_ = QUrl::fromLocalFile(atlas).toString();
        qInfo().noquote() << "event=imagery_package_loaded state=" << imagery_state_
                          << "tiles=" << imagery_tile_count_
                          << "coverage_gaps=" << imagery_coverage_gaps_ << "path=" << path;
        emit imageryChanged();
        return;
    }
    qWarning().noquote() << "event=imagery_package_unavailable";
    emit imageryChanged();
}

void PfdController::loadRadionavigationIndex() {
    const QString configured = qEnvironmentVariable("EFIS_RADIONAVIGATION_INDEX");
    const QDir app{QCoreApplication::applicationDirPath()};
    const QStringList candidates = configured.isEmpty()
        ? QStringList{QDir::current().filePath("data/navigation/radionavigation-index.json"),
                      app.filePath("../../data/navigation/radionavigation-index.json")}
        : QStringList{configured};
    for (const auto& path : candidates) {
        QFile file{path};
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) continue;
        const auto root = document.object();
        radionavigation_database_version_ = root.value("database_version").toString(QStringLiteral("UNKNOWN"));
        radionavigation_facilities_ = root.value("facilities").toArray().toVariantList();
        radionavigation_database_status_ = root.value("source").toObject().value("status").toString(QStringLiteral("UNKNOWN"));
        if (radionavigation_facilities_.isEmpty()) radionavigation_database_status_ = QStringLiteral("NO_DATA");
        qInfo().noquote() << "event=radionavigation_index_loaded facilities=" << radionavigation_facilities_.size()
                          << "version=" << radionavigation_database_version_
                          << "status=" << radionavigation_database_status_ << "path=" << path;
        return;
    }
    qWarning().noquote() << "event=radionavigation_index_unavailable";
}

void PfdController::loadNavigationIndex() {
    const QString configured = qEnvironmentVariable("EFIS_NAVIGATION_INDEX");
    const QDir app{QCoreApplication::applicationDirPath()};
    const QStringList candidates = configured.isEmpty()
        ? QStringList{QDir::current().filePath("data/navigation/navigation-index.json"),
                      app.filePath("../../data/navigation/navigation-index.json")}
        : QStringList{configured};
    for (const auto& path : candidates) {
        QFile file{path};
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) continue;
        const auto root = document.object();
        airport_catalog_ = root.value("airports").toArray().toVariantList();
        navigation_database_version_ = root.value("database_version").toString(QStringLiteral("UNKNOWN"));
        if (!airport_catalog_.isEmpty()) {
            selected_airport_ = airport_catalog_.first().toMap();
            const QString defaultDestination = qEnvironmentVariable("EFIS_SIM_DEST_ICAO", QStringLiteral("LIDB"));
            for (const auto& entry : airport_catalog_)
                if (entry.toMap().value("icao").toString() == defaultDestination) { selected_airport_ = entry.toMap(); break; }
        }
        qInfo().noquote() << "event=navigation_index_loaded airports=" << airport_catalog_.size()
                          << "version=" << navigation_database_version_ << "path=" << path;
        return;
    }
    qWarning().noquote() << "event=navigation_index_unavailable";
}

bool PfdController::selectAirport(const QString& icao) {
    for (const auto& entry : airport_catalog_) {
        const auto airport = entry.toMap();
        if (airport.value("icao").toString().compare(icao.trimmed(),Qt::CaseInsensitive) != 0) continue;
        selected_airport_ = airport;
        setup_message_ = QStringLiteral("Selected %1 • %2 • %3")
            .arg(airport.value("icao").toString(),airport.value("name").toString(),airport.value("status").toString());
        appendSetupAudit(QStringLiteral("AIRPORT_SELECT"), airport.value("status").toString(), icao);
        emit navigationSetupChanged(); emit setupChanged(); return true;
    }
    setup_message_ = QStringLiteral("Airport not found in the loaded navigation package");
    emit setupChanged(); return false;
}

void PfdController::updateNearestAirports() {
    struct Candidate { double distance; QVariantMap airport; };
    std::vector<Candidate> candidates;
    candidates.reserve(static_cast<std::size_t>(airport_catalog_.size()));
    constexpr double earthRadiusNm = 3440.065;
    constexpr double toRadians = 3.14159265358979323846 / 180.0;
    const double lat1 = latitude()*toRadians;
    for (const auto& entry : airport_catalog_) {
        auto airport = entry.toMap();
        const double lat2 = airport.value("latitude_deg").toDouble()*toRadians;
        const double dLat = lat2-lat1;
        const double dLon = (airport.value("longitude_deg").toDouble()-longitude())*toRadians;
        const double a = std::sin(dLat/2.0)*std::sin(dLat/2.0)+std::cos(lat1)*std::cos(lat2)*std::sin(dLon/2.0)*std::sin(dLon/2.0);
        const double distance = earthRadiusNm*2.0*std::atan2(std::sqrt(a),std::sqrt(qMax(0.0,1.0-a)));
        airport.insert("distance_nm",distance);
        candidates.push_back({distance,airport});
    }
    std::sort(candidates.begin(),candidates.end(),[](const Candidate& left,const Candidate& right){ return left.distance<right.distance; });
    nearest_airports_.clear();
    for (std::size_t index=0; index<std::min<std::size_t>(8,candidates.size()); ++index)
        nearest_airports_.append(candidates[index].airport);
}

void PfdController::loadCatalogIndex() {
    const QString configured = qEnvironmentVariable("EFIS_CATALOG_INDEX");
    const QDir app{QCoreApplication::applicationDirPath()};
    const QStringList candidates = configured.isEmpty()
        ? QStringList{QDir::current().filePath("data/catalog/efis-catalog-index.json"),
                      app.filePath("../../data/catalog/efis-catalog-index.json")}
        : QStringList{configured};
    for (const auto& path : candidates) {
        QFile file{path};
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) continue;
        const auto root = document.object();
        aircraft_catalog_ = root.value("aircraft").toArray().toVariantList();
        engine_catalog_ = root.value("engines").toArray().toVariantList();
        if (catalog_version_ == QStringLiteral("UNAVAILABLE"))
            catalog_version_ = root.value("catalog_version").toString(QStringLiteral("UNKNOWN"));
        qInfo().noquote() << "event=catalog_index_loaded aircraft=" << aircraft_catalog_.size()
                          << "engines=" << engine_catalog_.size() << "path=" << path;
        return;
    }
    qWarning().noquote() << "event=catalog_index_unavailable";
}

bool PfdController::stageAircraft(int catalogId) {
    QVariantMap selected;
    for (const auto& entry : aircraft_catalog_) {
        const auto candidate = entry.toMap();
        if (candidate.value("id").toInt() == catalogId) { selected = candidate; break; }
    }
    if (selected.isEmpty()) {
        staging_status_ = QStringLiteral("NOT_FOUND");
        setup_message_ = QStringLiteral("Aircraft not found in the current catalog");
        emit setupChanged();
        return false;
    }
    const auto speeds = selected.value("speeds_kt").toMap();
    const QStringList required{QStringLiteral("VSO"),QStringLiteral("VS1"),QStringLiteral("VFE"),
        QStringLiteral("VX"),QStringLiteral("VY"),QStringLiteral("VA"),QStringLiteral("VNO"),
        QStringLiteral("VNE"),QStringLiteral("BEST_GLIDE"),QStringLiteral("VREF")};
    QStringList missing;
    for (const auto& code : required) if (!speeds.contains(code) || speeds.value(code).toDouble() <= 0.0) missing << code;
    staging_status_ = !missing.isEmpty() ? QStringLiteral("INCOMPLETE")
        : selected.value("status").toString() == QStringLiteral("APPROVED") ? QStringLiteral("READY_FOR_REVIEW")
        : QStringLiteral("BLOCKED_INDICATIVE");
    selected.insert("missing_fields", missing);
    selected.insert("staging_status", staging_status_);
    selected.insert("catalog_version", catalog_version_);
    staged_aircraft_profile_ = selected;

    const QString path = stagingProfilePath();
    const QFileInfo info{path};
    QDir{}.mkpath(info.absolutePath());
    QSaveFile file{path};
    if (!file.open(QIODevice::WriteOnly)) {
        staging_status_ = QStringLiteral("WRITE_ERROR");
        setup_message_ = QStringLiteral("Unable to write the staging profile"); emit setupChanged(); return false;
    }
    const QJsonObject document{{"schema_version",1},{"state","DRAFT"},
        {"catalog_version",catalog_version_},{"aircraft",QJsonObject::fromVariantMap(selected)}};
    file.write(QJsonDocument{document}.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        staging_status_ = QStringLiteral("WRITE_ERROR");
        setup_message_ = QStringLiteral("Unable to commit the staging profile"); emit setupChanged(); return false;
    }
    setup_message_ = staging_status_ == QStringLiteral("INCOMPLETE")
        ? QStringLiteral("Complete all required V-speeds before verification")
        : QStringLiteral("Confirm the installed-aircraft data against its POH/AFM");
    appendSetupAudit(QStringLiteral("STAGE"), staging_status_, QString::number(catalogId));
    qInfo().noquote() << "event=aircraft_profile_staged id=" << catalogId
                      << "status=" << staging_status_ << "path=" << path;
    emit setupChanged();
    return true;
}

QString PfdController::activeProfilePath() const {
    const QString configured = qEnvironmentVariable("EFIS_AIRCRAFT_PROFILE");
    return configured.isEmpty() ? QDir::current().filePath("config/active/aircraft-profile.json") : configured;
}

QString PfdController::stagingProfilePath() const {
    const QString configured = qEnvironmentVariable("EFIS_STAGING_PROFILE");
    return configured.isEmpty() ? QDir::current().filePath("config/staging/aircraft-profile-draft.json") : configured;
}

QString PfdController::backupDirectoryPath() const {
    const QString configured = qEnvironmentVariable("EFIS_PROFILE_BACKUP_DIR");
    return configured.isEmpty() ? QDir::current().filePath("config/backups") : configured;
}

void PfdController::appendSetupAudit(const QString& action, const QString& result, const QString& detail) const {
    const QString configured = qEnvironmentVariable("EFIS_SETUP_AUDIT");
    const QString path = configured.isEmpty() ? QDir::current().filePath("config/audit/setup-audit.log") : configured;
    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    QTextStream stream{&file};
    stream << QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) << '\t'
           << action << '\t' << result << '\t' << detail.simplified() << '\n';
}

bool PfdController::verifyStagedAircraft(const QString& pohReference, bool speedsConfirmed,
                                         bool weightsConfirmed, bool engineConfirmed) {
    if (staged_aircraft_profile_.isEmpty() || staging_status_ == QStringLiteral("INCOMPLETE")) {
        setup_message_ = QStringLiteral("The staged profile is incomplete");
        emit setupChanged(); return false;
    }
    if (pohReference.trimmed().size() < 4 || !speedsConfirmed || !weightsConfirmed || !engineConfirmed) {
        staging_status_ = QStringLiteral("VERIFICATION_REQUIRED");
        setup_message_ = QStringLiteral("POH/AFM reference and all three confirmations are required");
        appendSetupAudit(QStringLiteral("VERIFY"), QStringLiteral("REJECTED"), setup_message_);
        emit setupChanged(); return false;
    }
    staged_aircraft_profile_.insert("verification_reference", pohReference.trimmed());
    staged_aircraft_profile_.insert("verified_at_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    staged_aircraft_profile_.insert("staging_status", QStringLiteral("READY_TO_APPLY"));
    staging_status_ = QStringLiteral("READY_TO_APPLY");
    setup_message_ = QStringLiteral("Verification complete. Profile can now be activated on ground.");

    QSaveFile file{stagingProfilePath()};
    if (!file.open(QIODevice::WriteOnly)) {
        staging_status_ = QStringLiteral("WRITE_ERROR"); setup_message_ = QStringLiteral("Cannot persist verification"); emit setupChanged(); return false;
    }
    const QJsonObject document{{"schema_version",1},{"state","VERIFIED"},
        {"catalog_version",catalog_version_},{"aircraft",QJsonObject::fromVariantMap(staged_aircraft_profile_)}};
    file.write(QJsonDocument{document}.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        staging_status_ = QStringLiteral("WRITE_ERROR"); setup_message_ = QStringLiteral("Cannot commit verification"); emit setupChanged(); return false;
    }
    appendSetupAudit(QStringLiteral("VERIFY"), QStringLiteral("ACCEPTED"), pohReference);
    emit setupChanged(); return true;
}

bool PfdController::applyStagedAircraft() {
    if (staging_status_ != QStringLiteral("READY_TO_APPLY")) {
        setup_message_ = QStringLiteral("Verification is required before activation"); emit setupChanged(); return false;
    }
    const QString active = activeProfilePath();
    const QString backupDir = backupDirectoryPath();
    QDir{}.mkpath(QFileInfo{active}.absolutePath());
    QDir{}.mkpath(backupDir);
    if (QFileInfo::exists(active)) {
        latest_backup_path_ = QDir{backupDir}.filePath(QStringLiteral("aircraft-profile-%1.json")
            .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))));
        if (!QFile::copy(active, latest_backup_path_)) {
            staging_status_ = QStringLiteral("BACKUP_ERROR"); setup_message_ = QStringLiteral("Activation aborted: backup failed");
            appendSetupAudit(QStringLiteral("APPLY"), QStringLiteral("REJECTED"), setup_message_); emit setupChanged(); return false;
        }
    }
    const QVariantMap speeds = staged_aircraft_profile_.value("speeds_kt").toMap();
    const QJsonObject aircraft{{"manufacturer", staged_aircraft_profile_.value("manufacturer").toString()},
        {"model", staged_aircraft_profile_.value("model").toString()}, {"variant", staged_aircraft_profile_.value("variant").toString()},
        {"registration", QString{}}, {"category", staged_aircraft_profile_.value("category").toString()},
        {"mtow_kg", staged_aircraft_profile_.value("mtow_kg").toDouble()},
        {"empty_weight_kg", staged_aircraft_profile_.value("empty_weight_kg").toDouble()},
        {"fuel_capacity_l", staged_aircraft_profile_.value("fuel_capacity_l").toDouble()}};
    const QJsonObject engine{{"manufacturer", staged_aircraft_profile_.value("engine_manufacturer").toString()},
        {"model", staged_aircraft_profile_.value("engine_model").toString()}, {"status",QStringLiteral("OPERATOR_VERIFIED")}};
    const QJsonObject evidence{{"status",QStringLiteral("OPERATOR_VERIFIED")},
        {"reference",staged_aircraft_profile_.value("verification_reference").toString()},
        {"verified_at_utc",staged_aircraft_profile_.value("verified_at_utc").toString()}};
    const QJsonObject root{{"schema_version",1}, {"profile_id",QStringLiteral("catalog-%1").arg(staged_aircraft_profile_.value("id").toInt())},
        {"revision",QDateTime::currentSecsSinceEpoch()}, {"state","ACTIVE"}, {"catalog_version",catalog_version_},
        {"aircraft",aircraft}, {"engine",engine}, {"speeds_kt",QJsonObject::fromVariantMap(speeds)},
        {"units",QJsonObject{{"airspeed","KT"},{"altitude","FT"},{"pressure","INHG"},{"temperature","C"},{"fuel","L"}}},
        {"evidence",evidence}};
    QSaveFile file{active};
    if (!file.open(QIODevice::WriteOnly)) {
        staging_status_ = QStringLiteral("WRITE_ERROR"); setup_message_ = QStringLiteral("Activation failed while opening active profile"); emit setupChanged(); return false;
    }
    file.write(QJsonDocument{root}.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        staging_status_ = QStringLiteral("WRITE_ERROR"); setup_message_ = QStringLiteral("Activation failed; previous profile remains available in backup"); emit setupChanged(); return false;
    }
    staging_status_ = QStringLiteral("APPLIED_RESTART_REQUIRED");
    setup_message_ = QStringLiteral("Profile activated. Restart the display to load the new limits.");
    appendSetupAudit(QStringLiteral("APPLY"), QStringLiteral("SUCCESS"), active);
    emit setupChanged(); return true;
}

bool PfdController::rollbackAircraftProfile() {
    if (latest_backup_path_.isEmpty() || !QFileInfo::exists(latest_backup_path_)) {
        setup_message_ = QStringLiteral("No backup from this session is available"); emit setupChanged(); return false;
    }
    QFile backup{latest_backup_path_};
    if (!backup.open(QIODevice::ReadOnly)) return false;
    QSaveFile active{activeProfilePath()};
    if (!active.open(QIODevice::WriteOnly)) return false;
    active.write(backup.readAll());
    if (!active.commit()) return false;
    staging_status_ = QStringLiteral("ROLLED_BACK_RESTART_REQUIRED");
    setup_message_ = QStringLiteral("Previous profile restored. Restart the display.");
    appendSetupAudit(QStringLiteral("ROLLBACK"), QStringLiteral("SUCCESS"), latest_backup_path_);
    emit setupChanged(); return true;
}

bool PfdController::stageEngine(int catalogId) {
    QVariantMap selected;
    for (const auto& entry : engine_catalog_) {
        const auto candidate = entry.toMap();
        if (candidate.value("id").toInt() == catalogId) { selected = candidate; break; }
    }
    if (selected.isEmpty()) {
        engine_staging_status_ = QStringLiteral("NOT_FOUND");
        setup_message_ = QStringLiteral("Engine not found in the current catalog");
        emit setupChanged(); return false;
    }
    selected.insert("catalog_version", catalog_version_);
    selected.insert("staging_status", QStringLiteral("LIMITS_REQUIRED"));
    staged_engine_profile_ = selected;
    engine_staging_status_ = QStringLiteral("LIMITS_REQUIRED");
    setup_message_ = QStringLiteral("Enter the installed-engine limits from its current manual");
    engine_sensor_mappings_.clear();
    const QStringList baseSignals{QStringLiteral("RPM"),QStringLiteral("MAP"),QStringLiteral("OIL_PRESS"),
        QStringLiteral("OIL_TEMP"),QStringLiteral("FUEL_PRESS"),QStringLiteral("FUEL_FLOW")};
    for (const auto& signal : baseSignals)
        engine_sensor_mappings_.append(QVariantMap{{"signal",signal},{"source","UNASSIGNED"},{"node",""},{"channel",-1},{"state","MISSING"}});
    const int cylinders = qBound(1, selected.value("cylinders",4).toInt(), 12);
    for (int cylinder=1; cylinder<=cylinders; ++cylinder) {
        engine_sensor_mappings_.append(QVariantMap{{"signal",QStringLiteral("CHT_%1").arg(cylinder)},{"source","UNASSIGNED"},{"node",""},{"channel",-1},{"state","MISSING"}});
        engine_sensor_mappings_.append(QVariantMap{{"signal",QStringLiteral("EGT_%1").arg(cylinder)},{"source","UNASSIGNED"},{"node",""},{"channel",-1},{"state","MISSING"}});
    }
    sensor_mapping_status_ = QStringLiteral("INCOMPLETE");
    appendSetupAudit(QStringLiteral("ENGINE_STAGE"), engine_staging_status_, QString::number(catalogId));
    emit setupChanged(); return true;
}

bool PfdController::verifyStagedEngine(const QString& manualReference, int cylinders,
        double rpmMaximum, double oilPressureMinimum, double oilPressureMaximum,
        double oilTemperatureMaximum, double chtMaximum, double egtMaximum,
        double fuelPressureMinimum, double fuelPressureMaximum) {
    if (staged_engine_profile_.isEmpty()) {
        engine_staging_status_ = QStringLiteral("NO_SELECTION"); emit setupChanged(); return false;
    }
    const bool valid = manualReference.trimmed().size() >= 4 && cylinders >= 1 && cylinders <= 12
        && rpmMaximum >= 1000.0 && rpmMaximum <= 10000.0
        && oilPressureMinimum > 0.0 && oilPressureMaximum > oilPressureMinimum
        && oilTemperatureMaximum > 40.0 && oilTemperatureMaximum <= 250.0
        && chtMaximum > 60.0 && chtMaximum <= 350.0
        && egtMaximum > chtMaximum && egtMaximum <= 1200.0
        && fuelPressureMinimum >= 0.0 && fuelPressureMaximum > fuelPressureMinimum;
    if (!valid) {
        engine_staging_status_ = QStringLiteral("INVALID_LIMITS");
        setup_message_ = QStringLiteral("Check units and ranges: maximum values must exceed minimum values");
        appendSetupAudit(QStringLiteral("ENGINE_VERIFY"), QStringLiteral("REJECTED"), setup_message_);
        emit setupChanged(); return false;
    }
    staged_engine_profile_.insert("cylinders", cylinders);
    staged_engine_profile_.insert("manual_reference", manualReference.trimmed());
    staged_engine_profile_.insert("verified_at_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    const QVariantMap limits{{"rpm_max",rpmMaximum},{"oil_pressure_min_psi",oilPressureMinimum},
        {"oil_pressure_max_psi",oilPressureMaximum},{"oil_temperature_max_c",oilTemperatureMaximum},
        {"cht_max_c",chtMaximum},{"egt_max_c",egtMaximum},
        {"fuel_pressure_min_psi",fuelPressureMinimum},{"fuel_pressure_max_psi",fuelPressureMaximum}};
    staged_engine_profile_.insert("limits", limits);
    staged_engine_profile_.insert("staging_status", QStringLiteral("READY_FOR_SENSOR_MAPPING"));
    engine_staging_status_ = QStringLiteral("READY_FOR_SENSOR_MAPPING");
    setup_message_ = QStringLiteral("Engine limits verified. Continue with sensor-channel mapping.");
    const QString configured = qEnvironmentVariable("EFIS_ENGINE_STAGING_PROFILE");
    const QString path = configured.isEmpty() ? QDir::current().filePath("config/staging/engine-profile-draft.json") : configured;
    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    QSaveFile file{path};
    if (!file.open(QIODevice::WriteOnly)) {
        engine_staging_status_ = QStringLiteral("WRITE_ERROR"); emit setupChanged(); return false;
    }
    const QJsonObject document{{"schema_version",1},{"state","VERIFIED_LIMITS"},
        {"catalog_version",catalog_version_},{"engine",QJsonObject::fromVariantMap(staged_engine_profile_)}};
    file.write(QJsonDocument{document}.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        engine_staging_status_ = QStringLiteral("WRITE_ERROR"); emit setupChanged(); return false;
    }
    appendSetupAudit(QStringLiteral("ENGINE_VERIFY"), QStringLiteral("ACCEPTED"), manualReference);
    emit setupChanged(); return true;
}

bool PfdController::setEngineSensorMapping(const QString& signal, const QString& source,
                                           const QString& node, int channel) {
    for (int index=0; index<engine_sensor_mappings_.size(); ++index) {
        auto mapping = engine_sensor_mappings_[index].toMap();
        if (mapping.value("signal").toString() != signal) continue;
        const bool assigned = source != QStringLiteral("UNASSIGNED") && !node.trimmed().isEmpty() && channel >= 0;
        mapping.insert("source", source);
        mapping.insert("node", node.trimmed());
        mapping.insert("channel", channel);
        mapping.insert("state", assigned ? QStringLiteral("ASSIGNED") : QStringLiteral("MISSING"));
        engine_sensor_mappings_[index] = mapping;
        sensor_mapping_status_ = QStringLiteral("CHANGED_NOT_VALIDATED");
        setup_message_ = QStringLiteral("Mapping changed. Validate all channels before activation.");
        emit setupChanged(); return assigned;
    }
    return false;
}

bool PfdController::validateEngineSensorMappings() {
    if (engine_sensor_mappings_.isEmpty()) {
        sensor_mapping_status_ = QStringLiteral("NOT_CONFIGURED"); emit setupChanged(); return false;
    }
    QStringList errors;
    QSet<QString> endpoints;
    for (const auto& item : engine_sensor_mappings_) {
        const auto mapping = item.toMap();
        const QString signal = mapping.value("signal").toString();
        const QString source = mapping.value("source").toString();
        const QString node = mapping.value("node").toString();
        const int channel = mapping.value("channel",-1).toInt();
        if (source == QStringLiteral("UNASSIGNED") || node.isEmpty() || channel < 0) {
            errors << signal; continue;
        }
        const QString endpoint = source+QStringLiteral(":")+node+QStringLiteral(":")+QString::number(channel);
        if (endpoints.contains(endpoint)) errors << QStringLiteral("DUPLICATE ")+endpoint;
        endpoints.insert(endpoint);
    }
    if (!errors.isEmpty()) {
        sensor_mapping_status_ = QStringLiteral("INVALID");
        setup_message_ = QStringLiteral("Missing or duplicate channels: ")+errors.mid(0,4).join(QStringLiteral(", "));
        appendSetupAudit(QStringLiteral("SENSOR_MAP_VERIFY"), QStringLiteral("REJECTED"), setup_message_);
        emit setupChanged(); return false;
    }
    const QString configured = qEnvironmentVariable("EFIS_ENGINE_SENSOR_MAP");
    const QString path = configured.isEmpty() ? QDir::current().filePath("config/staging/engine-sensor-map.json") : configured;
    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    QSaveFile file{path};
    if (!file.open(QIODevice::WriteOnly)) {
        sensor_mapping_status_ = QStringLiteral("WRITE_ERROR"); emit setupChanged(); return false;
    }
    const QJsonObject document{{"schema_version",1},{"state","VERIFIED"},
        {"engine_id",staged_engine_profile_.value("id").toInt()},
        {"verified_at_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"mappings",QJsonArray::fromVariantList(engine_sensor_mappings_)}};
    file.write(QJsonDocument{document}.toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        sensor_mapping_status_ = QStringLiteral("WRITE_ERROR"); emit setupChanged(); return false;
    }
    sensor_mapping_status_ = QStringLiteral("READY_FOR_CHANNEL_TEST");
    setup_message_ = QStringLiteral("Mapping verified. Run the live test for every sensor channel.");
    appendSetupAudit(QStringLiteral("SENSOR_MAP_VERIFY"), QStringLiteral("ACCEPTED"), path);
    emit setupChanged(); return true;
}

bool PfdController::startEngineSensorTest(const QString& signal, bool benchSimulation) {
    bool known = false;
    for (const auto& item : engine_sensor_mappings_)
        if (item.toMap().value("signal").toString() == signal) { known = true; break; }
    for (const auto& item : engine_sensor_configuration_)
        if (item.toMap().value("signal").toString() == signal) { known = true; break; }
    if (!known) {
        sensor_test_status_ = QStringLiteral("UNKNOWN_SIGNAL"); emit setupChanged(); return false;
    }
    sensor_test_signal_ = signal;
    sensor_test_bench_ = benchSimulation;
    sensor_test_fault_ = false;
    sensor_test_diagnostic_ = QStringLiteral("NORMAL");
    sensor_test_ticks_ = 0;
    sensor_test_raw_ = sensor_test_value_ = sensor_test_rate_hz_ = 0.0;
    sensor_test_age_ms_ = 0;
    sensor_test_status_ = benchSimulation ? QStringLiteral("BENCH_SIM_ACTIVE") : QStringLiteral("WAITING_FOR_PROVIDER");
    appendSetupAudit(QStringLiteral("SENSOR_TEST_START"), sensor_test_status_, signal);
    emit setupChanged(); return true;
}

void PfdController::stopEngineSensorTest() {
    if (!sensor_test_signal_.isEmpty())
        appendSetupAudit(QStringLiteral("SENSOR_TEST_STOP"), sensor_test_status_, sensor_test_signal_);
    sensor_test_status_ = QStringLiteral("IDLE");
    sensor_test_bench_ = false;
    sensor_test_fault_ = false;
    emit setupChanged();
}

void PfdController::setEngineSensorCalibration(double gain, double offset) {
    if (!std::isfinite(gain) || !std::isfinite(offset) || gain <= 0.0 || gain > 1000.0) {
        setup_message_ = QStringLiteral("Calibration rejected: gain must be positive and finite");
        emit setupChanged(); return;
    }
    sensor_test_gain_ = gain;
    sensor_test_offset_ = offset;
    setup_message_ = QStringLiteral("Calibration applied to bench preview only");
    appendSetupAudit(QStringLiteral("SENSOR_CALIBRATION_PREVIEW"), QStringLiteral("ACCEPTED"),
        QStringLiteral("gain=%1 offset=%2").arg(gain).arg(offset));
    emit setupChanged();
}

void PfdController::toggleEngineSensorTestFault() {
    if (!sensor_test_bench_) return;
    sensor_test_fault_ = !sensor_test_fault_;
    sensor_test_status_ = sensor_test_fault_ ? QStringLiteral("TIMEOUT_FAULT") : QStringLiteral("BENCH_SIM_ACTIVE");
    appendSetupAudit(QStringLiteral("SENSOR_TEST_FAULT"), sensor_test_fault_ ? QStringLiteral("INJECTED") : QStringLiteral("CLEARED"), sensor_test_signal_);
    emit setupChanged();
}

void PfdController::cycleSensorTestDiagnostic() {
    if (!sensor_test_bench_) return;
    static const QStringList modes{QStringLiteral("NORMAL"),QStringLiteral("TIMEOUT"),QStringLiteral("OPEN CIRCUIT"),QStringLiteral("SHORT CIRCUIT"),QStringLiteral("UNSTABLE")};
    sensor_test_diagnostic_=modes.at((modes.indexOf(sensor_test_diagnostic_)+1)%modes.size());
    sensor_test_fault_=sensor_test_diagnostic_==QStringLiteral("TIMEOUT"); emit setupChanged();
}

void PfdController::cycleElectricalTestMode(){
    static const QStringList modes{QStringLiteral("NORMAL"),QStringLiteral("ALTERNATOR OFFLINE"),QStringLiteral("MAIN BUS LOW"),QStringLiteral("ESSENTIAL BUS ISOLATED"),QStringLiteral("BATTERY LOW")};
    electrical_test_mode_=modes.at((modes.indexOf(electrical_test_mode_)+1)%modes.size());emit electricalTestChanged();
}

void PfdController::loadMaintenance(){const QStringList ids{"OIL_FILTER","ENGINE_INSPECTION","SPARK_PLUGS","COOLING_SYSTEM","GEARBOX_CLUTCH","ANNUAL_INSPECTION"};const QStringList labels{"OIL / FILTER SERVICE","ENGINE INSPECTION","SPARK PLUG INSPECTION","COOLING SYSTEM","GEARBOX / CLUTCH","ANNUAL INSPECTION"};QSettings s{QSettings::IniFormat,QSettings::UserScope,"EFIS-NEXT","ems-maintenance"};for(int i=0;i<ids.size();++i){QVariantMap t{{"id",ids[i]},{"label",labels[i]}};const auto b=QStringLiteral("tasks/%1/").arg(ids[i]);t["interval_hours"]=s.value(b+"interval_hours",0).toInt();t["last_hours"]=s.value(b+"last_hours",0.0).toDouble();t["last_utc"]=s.value(b+"last_utc",QStringLiteral("NEVER")).toString();maintenance_tasks_<<t;}}

QString PfdController::maintenanceStatus() const{for(const auto& item:maintenance_tasks_){const auto t=item.toMap();const int interval=t.value("interval_hours").toInt();if(interval<=0)return QStringLiteral("PROGRAM REQUIRED");if(engineHours()>=t.value("last_hours").toDouble()+interval)return QStringLiteral("SERVICE DUE");}return QStringLiteral("CURRENT");}

void PfdController::cycleMaintenanceInterval(int index){if(index<0||index>=maintenance_tasks_.size())return;static const QList<int> values{0,25,50,100,200,600};auto t=maintenance_tasks_[index].toMap();const int current=t.value("interval_hours").toInt();t["interval_hours"]=values[(values.indexOf(current)+1)%values.size()];maintenance_tasks_[index]=t;QSettings s{QSettings::IniFormat,QSettings::UserScope,"EFIS-NEXT","ems-maintenance"};s.setValue(QStringLiteral("tasks/%1/interval_hours").arg(t.value("id").toString()),t.value("interval_hours"));s.sync();emit maintenanceChanged();}

bool PfdController::completeMaintenanceTask(int index){if(index<0||index>=maintenance_tasks_.size())return false;auto t=maintenance_tasks_[index].toMap();if(t.value("interval_hours").toInt()<=0)return false;t["last_hours"]=engineHours();t["last_utc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);maintenance_tasks_[index]=t;QSettings s{QSettings::IniFormat,QSettings::UserScope,"EFIS-NEXT","ems-maintenance"};const auto b=QStringLiteral("tasks/%1/").arg(t.value("id").toString());s.setValue(b+"last_hours",t.value("last_hours"));s.setValue(b+"last_utc",t.value("last_utc"));s.sync();emit maintenanceChanged();return s.status()==QSettings::NoError;}

void PfdController::loadAircraftProfile() {
    aircraft_speed_profile_ = {{"vso",39.0},{"vs1",45.0},{"vfe",85.0},{"vx",62.0},
        {"vy",68.0},{"va",105.0},{"vno",120.0},{"vne",140.0},{"bestGlide",65.0},{"vref",55.0}};
    const QString configured = qEnvironmentVariable("EFIS_AIRCRAFT_PROFILE");
    const QDir app{QCoreApplication::applicationDirPath()};
    const QStringList candidates = configured.isEmpty()
        ? QStringList{QDir::current().filePath("config/active/aircraft-profile.json"),
                      app.filePath("../../config/active/aircraft-profile.json")}
        : QStringList{configured};
    for (const auto& path : candidates) {
        QFile file{path};
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) continue;
        const auto root = document.object();
        const auto aircraft = root.value("aircraft").toObject();
        const auto engine = root.value("engine").toObject();
        engine_profile_name_ = engine.value("manufacturer").toString() +
            QStringLiteral(" ") + engine.value("model").toString();
        engine_serial_number_ = engine.value("serial_number").toString(QStringLiteral("TO BE ENTERED"));
        engine_limits_ = engine.value("limits").toObject().toVariantMap();
        engine_capabilities_ = engine.value("capabilities").toObject().toVariantMap();
        const auto runtime = engine.value("runtime_tracking").toObject();
        runtime_tracker_ = efis::engine::RuntimeTracker{
            {runtime.value("engine_run_threshold_rpm").toDouble(500.0),
             runtime.value("tach_reference_rpm").toDouble(5000.0)},
            {runtime.value("initial_master_hours").toDouble(), runtime.value("initial_engine_hours").toDouble(),
             runtime.value("initial_tach_hours").toDouble(), runtime.value("initial_flight_hours").toDouble()}};
        engine_sensor_configuration_.clear();
        QSettings calibration{QSettings::IniFormat, QSettings::UserScope,
                              QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-calibration")};
        calibration_revision_ = calibration.value(QStringLiteral("revision"), 0).toInt();
        for (const auto item : engine.value("sensors").toArray()) {
            auto sensor = item.toObject().toVariantMap();
            const auto signal = sensor.value(QStringLiteral("signal")).toString();
            sensor.insert(QStringLiteral("calibration_state"),
                          calibration.value(QStringLiteral("sensors/%1/state").arg(signal),
                                            QStringLiteral("NOT CONFIGURED")).toString());
            sensor.insert(QStringLiteral("configured_type"),
                          calibration.value(QStringLiteral("sensors/%1/type").arg(signal),
                                            sensor.value(QStringLiteral("type"))).toString());
            sensor.insert(QStringLiteral("input"),
                          calibration.value(QStringLiteral("sensors/%1/input").arg(signal),
                                            QStringLiteral("UNASSIGNED")).toString());
            sensor.insert(QStringLiteral("zero_raw"), calibration.value(QStringLiteral("sensors/%1/zero_raw").arg(signal)));
            sensor.insert(QStringLiteral("span_raw"), calibration.value(QStringLiteral("sensors/%1/span_raw").arg(signal)));
            sensor.insert(QStringLiteral("gain"), calibration.value(QStringLiteral("sensors/%1/gain").arg(signal), 1.0));
            sensor.insert(QStringLiteral("offset"), calibration.value(QStringLiteral("sensors/%1/offset").arg(signal), 0.0));
            sensor.insert(QStringLiteral("calibration_source"), calibration.value(QStringLiteral("sensors/%1/source").arg(signal), QStringLiteral("NONE")));
            engine_sensor_configuration_ << sensor;
        }
        electrical_loads_.clear(); electrical_buses_.clear(); electrical_sources_.clear();
        const auto electrical=root.value("systems").toObject().value("electrical").toObject();
        for(const auto item:electrical.value("buses").toArray())electrical_buses_<<item.toObject().toVariantMap();
        for(const auto item:electrical.value("sources").toArray())electrical_sources_<<item.toObject().toVariantMap();
        for (const auto item : electrical.value("loads").toArray())
            electrical_loads_ << item.toObject().toVariantMap();
        const auto systems = root.value("systems").toObject();
        const auto gear = systems.value("landing_gear").toObject();
        landing_gear_installed_ = gear.value("installed").toBool(false);
        retractable_gear_ = gear.value("retractable").toBool(false);
        const auto flaps = systems.value("flaps").toObject();
        flaps_installed_ = flaps.value("installed").toBool(false);
        flap_detents_.clear();
        for (const auto detent : flaps.value("detents_deg").toArray()) flap_detents_ << detent.toDouble();
        const auto speeds = root.value("speeds_kt").toObject();
        if (speeds.isEmpty()) continue;
        const QMap<QString,QString> mapping{{"VSO","vso"},{"VS1","vs1"},{"VFE","vfe"},
            {"VX","vx"},{"VY","vy"},{"VA","va"},{"VNO","vno"},{"VNE","vne"},
            {"BEST_GLIDE","bestGlide"},{"VREF","vref"}};
        for (auto it=mapping.cbegin(); it!=mapping.cend(); ++it)
            if (speeds.contains(it.key())) aircraft_speed_profile_[it.value()] = speeds.value(it.key()).toDouble();
        aircraft_profile_name_ = aircraft.value("manufacturer").toString()+QStringLiteral(" • ")+aircraft.value("model").toString();
        aircraft_profile_state_ = root.value("state").toString(QStringLiteral("DRAFT"));
        catalog_version_ = root.value("catalog_version").toString(QStringLiteral("UNKNOWN"));
        qInfo().noquote() << "event=aircraft_profile_loaded path=" << path
                          << "profile=" << aircraft_profile_name_ << "state=" << aircraft_profile_state_;
        return;
    }
    qWarning().noquote() << "event=aircraft_profile_fallback reason=no_valid_profile";
}

bool PfdController::sensorCalibrationReady() const {
    for (const auto& item : engine_sensor_configuration_) {
        const auto sensor = item.toMap();
        if (sensor.value(QStringLiteral("required")).toBool() &&
            sensor.value(QStringLiteral("calibration_state")).toString() != QStringLiteral("VERIFIED")) return false;
    }
    return !engine_sensor_configuration_.isEmpty();
}

bool PfdController::cycleSensorCalibration(int index) {
    if (index < 0 || index >= engine_sensor_configuration_.size()) return false;
    auto sensor = engine_sensor_configuration_[index].toMap();
    const auto current = sensor.value(QStringLiteral("calibration_state")).toString();
    const bool liveCalibration = sensor.value(QStringLiteral("calibration_source")).toString() == QStringLiteral("LIVE") && sensor.value(QStringLiteral("zero_raw")).isValid() && sensor.value(QStringLiteral("span_raw")).isValid();
    const auto next = current == QStringLiteral("NOT CONFIGURED") ? QStringLiteral("CONFIGURED")
                    : current == QStringLiteral("CONFIGURED") && liveCalibration ? QStringLiteral("VERIFIED")
                    : current == QStringLiteral("CONFIGURED") ? QStringLiteral("CONFIGURED")
                                                              : QStringLiteral("NOT CONFIGURED");
    sensor.insert(QStringLiteral("calibration_state"), next);
    engine_sensor_configuration_[index] = sensor;
    ++calibration_revision_;
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-calibration")};
    settings.setValue(QStringLiteral("sensors/%1/state").arg(sensor.value(QStringLiteral("signal")).toString()), next);
    settings.setValue(QStringLiteral("revision"), calibration_revision_);
    settings.sync();
    emit sensorConfigurationChanged();
    qInfo().noquote() << "event=ems_sensor_calibration signal=" << sensor.value("signal").toString()
                      << "state=" << next << "revision=" << calibration_revision_;
    return settings.status() == QSettings::NoError;
}

bool PfdController::cycleSensorType(int index) {
    if (index < 0 || index >= engine_sensor_configuration_.size()) return false;
    static const QStringList types{QStringLiteral("UNSELECTED"), QStringLiteral("FREQUENCY"),
        QStringLiteral("THERMOCOUPLE K"), QStringLiteral("PT1000"), QStringLiteral("VDO RESISTIVE"),
        QStringLiteral("0-5 V"), QStringLiteral("CAN DIGITAL")};
    auto sensor = engine_sensor_configuration_[index].toMap();
    const auto current = sensor.value(QStringLiteral("configured_type")).toString();
    const auto next = types.at((types.indexOf(current) + 1 + types.size()) % types.size());
    sensor.insert(QStringLiteral("configured_type"), next);
    sensor.insert(QStringLiteral("calibration_state"), QStringLiteral("NOT CONFIGURED"));
    engine_sensor_configuration_[index] = sensor; ++calibration_revision_;
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-calibration")};
    const auto signal = sensor.value(QStringLiteral("signal")).toString();
    settings.setValue(QStringLiteral("sensors/%1/type").arg(signal), next);
    settings.setValue(QStringLiteral("sensors/%1/state").arg(signal), QStringLiteral("NOT CONFIGURED"));
    settings.setValue(QStringLiteral("revision"), calibration_revision_); settings.sync();
    emit sensorConfigurationChanged(); return settings.status() == QSettings::NoError;
}

bool PfdController::cycleSensorInput(int index) {
    if (index < 0 || index >= engine_sensor_configuration_.size()) return false;
    static const QStringList inputs{QStringLiteral("UNASSIGNED"), QStringLiteral("CAN A"),
        QStringLiteral("CAN B"), QStringLiteral("ADC 1"), QStringLiteral("ADC 2"),
        QStringLiteral("FREQ 1"), QStringLiteral("FREQ 2")};
    auto sensor = engine_sensor_configuration_[index].toMap();
    const auto current = sensor.value(QStringLiteral("input")).toString();
    const auto next = inputs.at((inputs.indexOf(current) + 1 + inputs.size()) % inputs.size());
    sensor.insert(QStringLiteral("input"), next);
    sensor.insert(QStringLiteral("calibration_state"), QStringLiteral("NOT CONFIGURED"));
    engine_sensor_configuration_[index] = sensor; ++calibration_revision_;
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-calibration")};
    const auto signal = sensor.value(QStringLiteral("signal")).toString();
    settings.setValue(QStringLiteral("sensors/%1/input").arg(signal), next);
    settings.setValue(QStringLiteral("sensors/%1/state").arg(signal), QStringLiteral("NOT CONFIGURED"));
    settings.setValue(QStringLiteral("revision"), calibration_revision_); settings.sync();
    emit sensorConfigurationChanged(); return settings.status() == QSettings::NoError;
}

bool PfdController::captureSensorPoint(int index, const QString& point) {
    if (index < 0 || index >= engine_sensor_configuration_.size()) return false;
    auto sensor=engine_sensor_configuration_[index].toMap(); const auto signal=sensor.value("signal").toString();
    double raw=0, spanReference=100; bool simulated=true;
    if(signal=="RPM"){raw=engine_snapshot_.rpm.value;simulated=engine_snapshot_.rpm.is_simulated();spanReference=5000;}
    else if(signal=="OIL_PRESSURE"){raw=engine_snapshot_.oil_pressure_psi.value;simulated=engine_snapshot_.oil_pressure_psi.is_simulated();spanReference=100;}
    else if(signal=="OIL_TEMPERATURE"||signal=="COOLANT_TEMPERATURE"){raw=engine_snapshot_.oil_temperature_c.value;simulated=engine_snapshot_.oil_temperature_c.is_simulated();spanReference=120;}
    else if(signal=="CHT"){raw=engine_snapshot_.cht_c.front().value;simulated=engine_snapshot_.cht_c.front().is_simulated();spanReference=200;}
    else if(signal=="EGT"){raw=engine_snapshot_.egt_c.front().value;simulated=engine_snapshot_.egt_c.front().is_simulated();spanReference=900;}
    else if(signal=="FUEL_PRESSURE"){raw=engine_snapshot_.fuel_pressure_psi.value;simulated=engine_snapshot_.fuel_pressure_psi.is_simulated();spanReference=10;}
    else if(signal=="FUEL_FLOW"){raw=engine_snapshot_.fuel_flow_gph.value;simulated=engine_snapshot_.fuel_flow_gph.is_simulated();spanReference=20;}
    else if(signal=="MANIFOLD_PRESSURE"){raw=engine_snapshot_.manifold_pressure_inhg.value;simulated=engine_snapshot_.manifold_pressure_inhg.is_simulated();spanReference=40;} else return false;
    const auto normalized=point.trimmed().toUpper(); if(normalized!="ZERO"&&normalized!="SPAN")return false;
    sensor.insert(normalized=="ZERO"?"zero_raw":"span_raw",raw); sensor.insert("calibration_source",simulated?"BENCH":"LIVE");
    const auto zero=sensor.value("zero_raw"),span=sensor.value("span_raw"); if(zero.isValid()&&span.isValid()&&std::abs(span.toDouble()-zero.toDouble())>1e-9){const double gain=spanReference/(span.toDouble()-zero.toDouble());sensor.insert("gain",gain);sensor.insert("offset",-zero.toDouble()*gain);sensor.insert("calibration_state","CONFIGURED");}
    engine_sensor_configuration_[index]=sensor;++calibration_revision_;QSettings settings{QSettings::IniFormat,QSettings::UserScope,"EFIS-NEXT","ems-calibration"};const auto base=QStringLiteral("sensors/%1/").arg(signal);settings.setValue(base+(normalized=="ZERO"?"zero_raw":"span_raw"),raw);settings.setValue(base+"source",sensor.value("calibration_source"));settings.setValue(base+"gain",sensor.value("gain"));settings.setValue(base+"offset",sensor.value("offset"));settings.setValue(base+"state",sensor.value("calibration_state"));settings.setValue("revision",calibration_revision_);settings.sync();emit sensorConfigurationChanged();return settings.status()==QSettings::NoError;
}

double PfdController::roll() const { return snapshot_.attitude.roll_deg.value; }
double PfdController::pitch() const { return snapshot_.attitude.pitch_deg.value; }
double PfdController::heading() const { return snapshot_.attitude.heading_deg.value; }
double PfdController::airspeed() const { return snapshot_.air_data.indicated_airspeed_kt.value; }
double PfdController::altitude() const { return snapshot_.air_data.pressure_altitude_ft.value; }
double PfdController::verticalSpeed() const { return snapshot_.air_data.vertical_speed_fpm.value; }
double PfdController::groundSpeed() const { return snapshot_.position.ground_speed_kt.value; }
double PfdController::latitude() const { return snapshot_.position.latitude_deg.value; }
double PfdController::longitude() const { return snapshot_.position.longitude_deg.value; }
double PfdController::commandedRoll() const { return snapshot_.guidance.commanded_roll_deg.value; }
double PfdController::commandedPitch() const { return snapshot_.guidance.commanded_pitch_deg.value; }
double PfdController::selectedAltitude() const { return snapshot_.guidance.selected_altitude_ft.value; }
double PfdController::selectedCourse() const { return snapshot_.guidance.selected_course_deg.value; }
double PfdController::selectedAirspeed() const { return snapshot_.guidance.selected_airspeed_kt.value; }
double PfdController::cdiDeviation() const { return snapshot_.navigation.cdi_deviation_dots.value; }
double PfdController::glideSlope() const { return snapshot_.navigation.glide_slope_dots.value; }
double PfdController::airspeedTrend() const { return airspeed_trend_; }
double PfdController::altitudeTrend() const { return verticalSpeed() / 10.0; }
bool PfdController::navTo() const {
    return snapshot_.navigation.to_flag.value && snapshot_.navigation.to_flag.usable();
}
bool PfdController::flightDirectorActive() const {
    return snapshot_.guidance.flight_director_active.value &&
           snapshot_.guidance.flight_director_active.usable();
}
QString PfdController::annunciation() const {
    return snapshot_.alerts.active.empty()
        ? QString{}
        : QString::fromStdString(snapshot_.alerts.active.front().message);
}
int PfdController::annunciationSeverity() const {
    return snapshot_.alerts.active.empty()
        ? -1
        : static_cast<int>(snapshot_.alerts.active.front().severity);
}
int PfdController::healthLevel() const {
    return static_cast<int>(snapshot_.health.level);
}
bool PfdController::airspeedValid() const {
    return snapshot_.air_data.indicated_airspeed_kt.usable();
}
bool PfdController::altitudeValid() const {
    return snapshot_.air_data.pressure_altitude_ft.usable();
}
bool PfdController::navigationValid() const {
    return snapshot_.navigation.cdi_deviation_dots.usable() &&
           snapshot_.attitude.heading_deg.usable();
}
QString PfdController::faultMode() const {
    return QString::fromLatin1(efis::injected_fault_name(simulation_->fault()));
}

QString PfdController::simulationRole() const {
    return QString::fromLatin1(efis::simulation_role_name(router_.simulation_role()));
}

QString PfdController::attitudeSource() const {
    switch (router_.arbitrator().selected_for(efis::SignalId::roll)) {
        case efis::SourceId::simulation: return QStringLiteral("SIM");
        case efis::SourceId::pixhawk_primary: return QStringLiteral("PIXHAWK");
        case efis::SourceId::esp32_legacy: return QStringLiteral("ESP32");
        case efis::SourceId::dlvr_legacy: return QStringLiteral("DLVR");
        case efis::SourceId::nav_rf: return QStringLiteral("NAV RF");
        case efis::SourceId::nav_synthetic: return QStringLiteral("NAV SYN");
        case efis::SourceId::unknown: break;
    }
    return QStringLiteral("NONE");
}

// Vero quando almeno una grandezza mostrata sul PFD non viene dal mondo reale.
// Il quadro deve poterlo dichiarare anche in un punto solo, oltre alle etichette
// dei singoli campi: il paragrafo 19 dell'handoff non ammette che un dato
// simulato possa passare per reale.
bool PfdController::showingSimulatedData() const {
    return snapshot_.attitude.roll_deg.is_simulated() ||
           snapshot_.air_data.indicated_airspeed_kt.is_simulated() ||
           snapshot_.air_data.pressure_altitude_ft.is_simulated() ||
           snapshot_.position.latitude_deg.is_simulated();
}

// Stato del dato mostrato, nel vocabolario obbligatorio della specifica
// PFD_FOCUS_1024: LIVE, SIM, DEGRADED, STALE, FAIL, NOT INSTALLED, UNVERIFIED.
//
// Era una stringa fissa "SIM" scritta nel QML. Una dichiarazione che non puo'
// cambiare non e' una dichiarazione: avrebbe continuato a dire SIM con i dati
// del Pixhawk sotto gli occhi, ed e' il valore decorativo che la specifica
// vieta al posto di un dato assente.
QString PfdController::dataSourceState() const {
    // Nessuna grandezza primaria utilizzabile. Il paragrafo 19 dell'handoff non
    // ammette che questo stato passi inosservato.
    if (!snapshot_.attitude.roll_deg.usable() &&
        !snapshot_.air_data.indicated_airspeed_kt.usable() &&
        !snapshot_.air_data.pressure_altitude_ft.usable()) {
        return QStringLiteral("FAIL");
    }
    if (!showingSimulatedData()) return QStringLiteral("LIVE");

    // La simulazione che ha la precedenza su sensori reali funzionanti e' una
    // condizione voluta ma non ordinaria, e va distinta dal semplice ripiego:
    // chi guarda deve sapere che il dato vero c'e' e non lo sta vedendo.
    return router_.simulation_role() == efis::SimulationRole::primary
        ? QStringLiteral("SIM FORCED")
        : QStringLiteral("SIM");
}

// Stato del nodo sensori reale. Anche questo era fisso a "NOT INSTALLED".
// Oggi il testo coincide, perche' sul nodo display non e' registrata alcuna
// sorgente reale, ma ora e' una conseguenza dello stato e non una didascalia:
// cambiera' da solo quando il collegamento verso il nodo di ingestion esistera'.
QString PfdController::sensorNodeState() const {
    if (router_.source_count() == 0) return QStringLiteral("NOT INSTALLED");
    return attitudeSource() == QStringLiteral("PIXHAWK")
        ? QStringLiteral("LIVE")
        : QStringLiteral("READY FOR DATA");
}

// Il ruolo vale per volo e motore insieme: e' una sola scelta dell'operatore, e
// due catene che potessero divergere darebbero un quadro meta' simulato e meta'
// no senza che nessuno l'abbia deciso.
// Stato del quadro motore, nello stesso vocabolario della riga di comando.
//
// Serve perche' il banco motore ha diciotto canali su sorgenti diverse: dire
// "SIM" a prescindere, come faceva l'etichetta scritta a mano, nasconde sia il
// caso in cui i canali sono veri sia quello in cui non c'e' alcun dato.
QString PfdController::engineDataState() const {
    if (!engine_snapshot_.rpm.usable() &&
        !engine_snapshot_.oil_pressure_psi.usable() &&
        !engine_snapshot_.cht_c[0].usable()) {
        return QStringLiteral("FAIL");
    }
    const bool simulated = engine_snapshot_.rpm.is_simulated() ||
                           engine_snapshot_.oil_pressure_psi.is_simulated() ||
                           engine_snapshot_.fuel_left_gal.is_simulated();
    if (!simulated) return QStringLiteral("LIVE");
    return engine_router_.simulation_role() == efis::SimulationRole::primary
        ? QStringLiteral("SIM FORCED")
        : QStringLiteral("SIM");
}

void PfdController::applySimulationRole(efis::SimulationRole role) {
    router_.set_simulation_role(role);
    engine_router_.set_simulation_role(role);
    qInfo().noquote() << "event=simulation_role role=" << simulationRole();
}

void PfdController::cycleSimulationRole() {
    switch (router_.simulation_role()) {
        case efis::SimulationRole::fallback:
            applySimulationRole(efis::SimulationRole::primary);
            break;
        case efis::SimulationRole::primary:
            applySimulationRole(efis::SimulationRole::disabled);
            break;
        case efis::SimulationRole::disabled:
            applySimulationRole(efis::SimulationRole::fallback);
            break;
    }
    emit snapshotChanged();
}

void PfdController::cycleFault() {
    simulation_->cycle_fault();
    qInfo().noquote() << "event=fault_injection mode=" << faultMode();
}
qulonglong PfdController::sequence() const { return snapshot_.sequence; }
QString PfdController::blackBoxState() const { return black_box_state_; }
qulonglong PfdController::blackBoxRecords() const { return black_box_records_; }
double PfdController::engineRpm() const { return engine_snapshot_.rpm.value; }
double PfdController::manifoldPressure() const { return engine_snapshot_.manifold_pressure_inhg.value; }
double PfdController::oilPressure() const { return engine_snapshot_.oil_pressure_psi.value; }
double PfdController::oilTemperature() const { return engine_snapshot_.oil_temperature_c.value; }
double PfdController::fuelPressure() const { return engine_snapshot_.fuel_pressure_psi.value; }
double PfdController::fuelFlow() const { return engine_snapshot_.fuel_flow_gph.value; }
double PfdController::fuelLeft() const { return calibratedFuel(engine_snapshot_.fuel_left_gal.value,fuel_calibration_left_); }
double PfdController::fuelRight() const { return calibratedFuel(engine_snapshot_.fuel_right_gal.value,fuel_calibration_right_); }
double PfdController::busVoltage() const { return engine_snapshot_.bus_voltage_v.value; }
double PfdController::alternatorCurrent() const { return engine_snapshot_.alternator_current_a.value; }
QVariantList PfdController::chtValues() const {
    QVariantList values; for (const auto& sample : engine_snapshot_.cht_c) values << sample.value; return values;
}
QVariantList PfdController::egtValues() const {
    QVariantList values; for (const auto& sample : engine_snapshot_.egt_c) values << sample.value; return values;
}
int PfdController::engineHealth() const { return static_cast<int>(engine_snapshot_.health); }
QString PfdController::engineFaultMode() const {
    using efis::engine::EngineFault;
    const auto fault = system_simulation_active_
        ? system_frame_.engine.injected_fault : engine_simulation_->fault();
    switch (fault) {
        case EngineFault::none: return QStringLiteral("NONE");
        case EngineFault::low_oil_pressure: return QStringLiteral("LOW OIL PRESS");
        case EngineFault::high_cht: return QStringLiteral("HIGH CHT 3");
        case EngineFault::alternator_failure: return QStringLiteral("ALTERNATOR");
        case EngineFault::low_fuel_pressure: return QStringLiteral("LOW FUEL PRESS");
    }
    return QStringLiteral("NONE");
}
void PfdController::cycleEngineFault() {
    using efis::engine::EngineFault;
    switch (engine_simulation_->fault()) {
        case EngineFault::none: engine_simulation_->set_fault(EngineFault::low_oil_pressure); break;
        case EngineFault::low_oil_pressure: engine_simulation_->set_fault(EngineFault::high_cht); break;
        case EngineFault::high_cht: engine_simulation_->set_fault(EngineFault::alternator_failure); break;
        case EngineFault::alternator_failure: engine_simulation_->set_fault(EngineFault::low_fuel_pressure); break;
        case EngineFault::low_fuel_pressure: engine_simulation_->set_fault(EngineFault::none); break;
    }
    qInfo().noquote() << "event=engine_fault_injection mode=" << engineFaultMode();
}

QString PfdController::systemSimulationScenario() const {
    return QString::fromLatin1(efis::simulation::scenario_name(system_simulator_.scenario()));
}

QString PfdController::systemSimulationPhase() const {
    return QString::fromLatin1(efis::simulation::phase_name(system_frame_.phase));
}

bool PfdController::startSystemSimulation(const QString& scenario) {
    try {
        system_simulator_.reset(efis::simulation::parse_scenario(
            scenario.toStdString()));
    } catch (const std::exception&) {
        return false;
    }
    system_simulation_active_ = true;
    system_simulation_running_ = true;
    system_simulation_elapsed_ms_ = 0.0;
    system_simulation_last_tick_ = efis::MonoClock::now();
    system_simulation_last_event_ = QStringLiteral("SCENARIO STARTED");
    qInfo().noquote() << "event=system_simulation_start scenario=" << scenario;
    emit simulationChanged();
    return true;
}

void PfdController::toggleSystemSimulation() {
    if (!system_simulation_active_) {
        startSystemSimulation(QStringLiteral("full-flight"));
        return;
    }
    system_simulation_running_ = !system_simulation_running_;
    system_simulation_last_tick_ = efis::MonoClock::now();
    system_simulation_last_event_ = system_simulation_running_
        ? QStringLiteral("RESUMED") : QStringLiteral("PAUSED");
    emit simulationChanged();
}

void PfdController::stopSystemSimulation() {
    system_simulation_active_ = false;
    system_simulation_running_ = false;
    system_simulation_elapsed_ms_ = 0.0;
    system_simulation_last_event_ = QStringLiteral("STOPPED");
    qInfo().noquote() << "event=system_simulation_stop";
    emit simulationChanged();
}

void PfdController::seekSystemSimulation(double progress) {
    if (!system_simulation_active_) return;
    const double bounded = std::clamp(progress, 0.0, 1.0);
    system_simulation_elapsed_ms_ = bounded *
        static_cast<double>(system_simulator_.duration().count());
    system_simulation_last_tick_ = efis::MonoClock::now();
    system_simulation_last_event_ = QStringLiteral("TIMELINE SEEK");
    emit simulationChanged();
}

void PfdController::setSystemSimulationSpeed(double speed) {
    system_simulation_speed_ = std::clamp(speed, 0.25, 20.0);
    system_simulation_last_event_ = QStringLiteral("SPEED CHANGED");
    emit simulationChanged();
}

void PfdController::updateSnapshot() {
    const auto now = efis::MonoClock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - started_at_);
    // Il display non sceglie piu' la sorgente: consuma cio' che il router
    // arbitra. Oggi e' registrata la sola simulazione, perche' su un nodo
    // display non deve arrivare alcun frame di bus: lo stato normalizzato viene
    // dal nodo di ingestion, come stabilito dalla ADR-003. Quando quel
    // collegamento esistera' sara' una sorgente in piu', e qui non cambiera'
    // nulla.
    if (system_simulation_active_) {
        if (system_simulation_last_tick_ == efis::MonoTime{})
            system_simulation_last_tick_ = now;
        if (system_simulation_running_) {
            const auto delta = std::chrono::duration<double, std::milli>(
                now - system_simulation_last_tick_).count();
            system_simulation_elapsed_ms_ += delta * system_simulation_speed_;
        }
        system_simulation_last_tick_ = now;
        const auto duration_ms = static_cast<double>(system_simulator_.duration().count());
        system_simulation_elapsed_ms_ = std::clamp(system_simulation_elapsed_ms_, 0.0, duration_ms);
        system_frame_ = system_simulator_.sample(
            now, std::chrono::milliseconds{static_cast<long long>(system_simulation_elapsed_ms_)});
        snapshot_ = model_.normalize(system_frame_.flight, now);
        engine_snapshot_ = system_frame_.engine;
        if (!system_frame_.events.empty())
            system_simulation_last_event_ = QString::fromStdString(system_frame_.events.back());
        if (system_frame_.complete) system_simulation_running_ = false;
        emit simulationChanged();
    } else {
        auto input = router_.poll(now, elapsed);
        snapshot_ = model_.normalize(std::move(input), now);
        engine_snapshot_ = engine_router_.poll(now, elapsed);
    }
    if (last_runtime_update_ != efis::MonoTime{}) {
        const auto seconds = std::chrono::duration<double>(now - last_runtime_update_).count();
        const bool airborne = snapshot_.position.ground_speed_kt.usable() &&
                              snapshot_.position.ground_speed_kt.value > 35.0;
        const double accountable_rpm = engine_snapshot_.rpm.is_simulated()
            ? 0.0 : engine_snapshot_.rpm.value;
        runtime_tracker_.update(accountable_rpm, seconds, airborne && !showingSimulatedData());
    }
    last_runtime_update_ = now;
    if (!fuel_on_board_initialized_) {
        fuel_on_board_gallons_ = fuelLeft() + fuelRight();
        fuel_on_board_initialized_ = true;
        persistFuelComputer(); emit fuelComputerChanged();
    }
    if (last_fuel_update_ != efis::MonoTime{}) {
        const auto seconds = std::chrono::duration<double>(now - last_fuel_update_).count();
        if (engine_snapshot_.rpm.value > 400.0 && engine_snapshot_.fuel_flow_gph.value > 0.0) {
            const double consumed = engine_snapshot_.fuel_flow_gph.value * seconds / 3600.0;
            if (engine_snapshot_.fuel_flow_gph.is_simulated()) simulated_fuel_used_gallons_ += consumed;
            else fuel_used_gallons_ += consumed;
            if (last_fuel_notify_ == efis::MonoTime{} || now - last_fuel_notify_ >= 1s) {
                last_fuel_notify_ = now; emit fuelComputerChanged();
            }
        }
    }
    last_fuel_update_ = now;
    updateFuelLeakMonitor(now);
    if(last_central_blackbox_check_==efis::MonoTime{}||now-last_central_blackbox_check_>=1s){last_central_blackbox_check_=now;loadCentralBlackBoxCache();}
    if (last_fuel_save_ == efis::MonoTime{} || now - last_fuel_save_ >= 10s) {
        last_fuel_save_ = now; persistFuelComputer();
    }
    if (trend_initialized_) {
        const auto six_second_projection = (snapshot_.air_data.indicated_airspeed_kt.value -
                                            previous_airspeed_) * 300.0;
        airspeed_trend_ = airspeed_trend_ * 0.9 + six_second_projection * 0.1;
    } else {
        trend_initialized_ = true;
    }
    previous_airspeed_ = snapshot_.air_data.indicated_airspeed_kt.value;
    if (last_recorder_check_ == efis::MonoTime{} || now - last_recorder_check_ >= 500ms) {
        last_recorder_check_ = now;
        try {
            const auto status = efis::recording::read_recorder_status(
                recorder_status_path_.toStdString());
            const auto wall_now = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            const auto age = wall_now >= status.updated_wall_ms
                ? wall_now - status.updated_wall_ms : 0;
            black_box_state_ = age > 3000
                ? QStringLiteral("STALE")
                : QString::fromStdString(status.state);
            black_box_records_ = status.records;
        } catch (...) {
            black_box_state_ = QStringLiteral("OFFLINE");
            black_box_records_ = 0;
        }
    }
    if (sensor_test_bench_) {
        ++sensor_test_ticks_;
        if (sensor_test_diagnostic_ == QStringLiteral("TIMEOUT")) {
            sensor_test_age_ms_ += 20;
            sensor_test_rate_hz_ = 0.0;
        } else {
            const double phase = static_cast<double>(sensor_test_ticks_) * 0.04;
            double base = 1000.0;
            if (sensor_test_signal_ == QStringLiteral("RPM")) base = 5000.0;
            else if (sensor_test_signal_ == QStringLiteral("MAP")) base = 24.5;
            else if (sensor_test_signal_ == QStringLiteral("OIL_PRESS")) base = 58.0;
            else if (sensor_test_signal_ == QStringLiteral("OIL_PRESSURE")) base = 58.0;
            else if (sensor_test_signal_ == QStringLiteral("OIL_TEMP")) base = 96.0;
            else if (sensor_test_signal_ == QStringLiteral("OIL_TEMPERATURE") || sensor_test_signal_ == QStringLiteral("COOLANT_TEMPERATURE")) base = 96.0;
            else if (sensor_test_signal_ == QStringLiteral("FUEL_PRESS")) base = 4.6;
            else if (sensor_test_signal_ == QStringLiteral("FUEL_PRESSURE")) base = 4.6;
            else if (sensor_test_signal_ == QStringLiteral("FUEL_FLOW")) base = 4.8;
            else if (sensor_test_signal_.startsWith(QStringLiteral("CHT_"))) base = 108.0;
            else if (sensor_test_signal_ == QStringLiteral("CHT")) base = 105.0;
            else if (sensor_test_signal_.startsWith(QStringLiteral("EGT_"))) base = 780.0;
            else if (sensor_test_signal_ == QStringLiteral("EGT")) base = 780.0;
            else if (sensor_test_signal_ == QStringLiteral("MANIFOLD_PRESSURE")) base = 24.5;
            if(sensor_test_diagnostic_==QStringLiteral("OPEN CIRCUIT")){sensor_test_raw_=4095.0;sensor_test_status_=QStringLiteral("BENCH_OPEN_CIRCUIT");}
            else if(sensor_test_diagnostic_==QStringLiteral("SHORT CIRCUIT")){sensor_test_raw_=0.0;sensor_test_status_=QStringLiteral("BENCH_SHORT_CIRCUIT");}
            else if(sensor_test_diagnostic_==QStringLiteral("UNSTABLE")){sensor_test_raw_=base+std::sin(phase*11.0)*base*.28;sensor_test_status_=QStringLiteral("BENCH_UNSTABLE_SIGNAL");}
            else {sensor_test_raw_ = base + std::sin(phase) * qMax(1.0, base * 0.015);sensor_test_status_=QStringLiteral("BENCH_SIM_ACTIVE");}
            sensor_test_value_ = sensor_test_raw_ * sensor_test_gain_ + sensor_test_offset_;
            sensor_test_rate_hz_ = 50.0;
            sensor_test_age_ms_ = 0;
        }
        if (sensor_test_ticks_ % 5 == 0) emit setupChanged();
    }
    if (++navigation_update_ticks_ >= 50) {
        navigation_update_ticks_ = 0;
        updateNearestAirports();
    }
    emit snapshotChanged();
}
