#include "ems_controller.hpp"

#include "efis/recording/recorder_status.hpp"

#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStringList>
#include <QList>
#include <cmath>
#include <algorithm>
#include <vector>

using namespace std::chrono_literals;

namespace {
// Le grandezze del motore hanno costanti di tempo termiche e meccaniche: una
// temperatura di testa non cambia in venti millisecondi, e il PFD gira a 50 Hz
// perche' deve seguire l'assetto, non il motore. Venti aggiornamenti al secondo
// sono gia' oltre quello che l'equipaggio puo' leggere, e lasciano alla GPU del
// RPi4 un margine che il ritmo del PFD non le lascerebbe.
constexpr int kUpdateIntervalMs = 50;
constexpr auto kRecorderPollInterval = 500ms;
// Oltre questa eta' lo stato del registratore descrive un passato, non un
// presente: dichiararlo fermo e' l'unica lettura onesta.
constexpr std::uint64_t kRecorderStaleMs = 3000;
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

EmsController::EmsController(QObject* parent) : QObject(parent) {
    const auto edp_bind = qEnvironmentVariable("EFIS_EDP_BIND");
    if (!edp_bind.isEmpty()) {
        bool port_ok = false;
        bool node_ok = false;
        const auto port = qEnvironmentVariableIntValue("EFIS_EDP_PORT", &port_ok);
        const auto node = qEnvironmentVariableIntValue("EFIS_EDP_SOURCE_NODE", &node_ok);
        efis::EdpUdpSourceConfig config;
        config.local = {edp_bind.toStdString(), static_cast<std::uint16_t>(port_ok ? port : 49'003)};
        config.consumer.expected_source_node = static_cast<std::uint32_t>(node_ok ? node : 47);
        engine_network_source_ = std::make_unique<efis::EdpUdpEngineSource>(std::move(config));
        engine_data_state_ = QStringLiteral("WAITING");
        data_source_state_ = QStringLiteral("NETWORK");
        sensor_node_state_ = QStringLiteral("BLACKBOX WAITING");
    }
    loadUnitSystem();
    loadAircraftProfile();
    loadOperationalCatalog();
    loadMaintenance();
    loadFuelComputer();
    fuel_event_log_path_=qEnvironmentVariable("EFIS_BLACKBOX_FUEL_EVENTS","state/blackbox-fuel-events.jsonl");
    loadFuelEventHistory();
    central_blackbox_cache_path_=qEnvironmentVariable("EFIS_CENTRAL_BLACKBOX_CACHE",QDir::homePath()+QStringLiteral("/.local/state/efis/fuel-central-cache.json"));
    central_blackbox_request_path_=qEnvironmentVariable("EFIS_CENTRAL_BLACKBOX_REQUEST",QDir::homePath()+QStringLiteral("/.local/state/efis/fuel-maintenance-request.jsonl"));
    runtime_store_ = std::make_unique<efis::engine::RuntimeStore>(
        qEnvironmentVariable("EFIS_ENGINE_RUNTIME_STATE", "state/engine-runtime.state").toStdString());
    try {
        if (const auto saved = runtime_store_->load(); saved && saved->profile_id == profile_id_.toStdString()) {
            runtime_tracker_ = efis::engine::RuntimeTracker{{500.0, 5000.0}, saved->totals};
            runtime_revision_ = static_cast<qulonglong>(saved->revision);
            runtime_persistence_state_ = QStringLiteral("RECOVERED");
        } else {
            runtime_persistence_state_ = QStringLiteral("NEW RECORD");
        }
    } catch (const std::exception& error) {
        runtime_persistence_state_ = QStringLiteral("INTEGRITY FAULT");
        qWarning().noquote() << "event=engine_runtime_rejected reason=" << error.what();
    }
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &EmsController::persistRuntime);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            this, &EmsController::persistFuelComputer);
    recorder_status_path_ = qEnvironmentVariable("EFIS_RECORDER_STATUS",
                                                 "run/recorder.status");
    timer_.setTimerType(Qt::PreciseTimer);
    timer_.setInterval(kUpdateIntervalMs);
    connect(&timer_, &QTimer::timeout, this, &EmsController::updateSnapshot);
    updateSnapshot();
    timer_.start();
}

void EmsController::loadUnitSystem() {
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-units")};
    const auto value = settings.value(QStringLiteral("display/unitSystem"),
                                      QStringLiteral("AVIATION")).toString().toUpper();
    if (value == QStringLiteral("AVIATION") || value == QStringLiteral("METRIC") ||
        value == QStringLiteral("IMPERIAL")) unit_system_ = value;
}

bool EmsController::setUnitSystem(const QString& system) {
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

void EmsController::loadFuelComputer() {
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-fuel-computer")};
    fuel_used_gallons_ = qMax(0.0, settings.value(QStringLiteral("real/used_gallons"), 0.0).toDouble());
    fuel_reserve_gallons_ = qBound(0.0, settings.value(QStringLiteral("pilot/reserve_gallons"), 5.0).toDouble(), 100.0);
    fuel_on_board_initialized_ = settings.contains(QStringLiteral("pilot/on_board_gallons"));
    fuel_on_board_gallons_ = qBound(0.0, settings.value(QStringLiteral("pilot/on_board_gallons"), 0.0).toDouble(), 100.0);
    fuel_calibration_left_ = settings.value(QStringLiteral("calibration/left_points")).toList();
    fuel_calibration_right_ = settings.value(QStringLiteral("calibration/right_points")).toList();
}

void EmsController::persistFuelComputer() {
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-fuel-computer")};
    settings.setValue(QStringLiteral("real/used_gallons"), fuel_used_gallons_);
    settings.setValue(QStringLiteral("pilot/reserve_gallons"), fuel_reserve_gallons_);
    settings.setValue(QStringLiteral("pilot/on_board_gallons"), fuel_on_board_gallons_);
    settings.setValue(QStringLiteral("calibration/left_points"), fuel_calibration_left_);
    settings.setValue(QStringLiteral("calibration/right_points"), fuel_calibration_right_);
    settings.sync();
}

bool EmsController::setFuelReserveGallons(double gallons) {
    if (!std::isfinite(gallons)) return false;
    fuel_reserve_gallons_ = qBound(0.0, gallons, 100.0);
    persistFuelComputer(); emit fuelComputerChanged(); return true;
}

bool EmsController::setFuelOnBoardGallons(double gallons) {
    if (!std::isfinite(gallons)) return false;
    fuel_on_board_gallons_ = qBound(0.0, gallons, 100.0);
    fuel_on_board_initialized_ = true;
    fuel_used_gallons_ = 0.0;
    simulated_fuel_used_gallons_ = 0.0;
    persistFuelComputer(); emit fuelComputerChanged(); return true;
}

bool EmsController::captureFuelCalibrationPoint(const QString& tank) {
    const bool left=tank.trimmed().toUpper()==QStringLiteral("LEFT");
    const bool right=tank.trimmed().toUpper()==QStringLiteral("RIGHT"); if(!left&&!right)return false;
    auto& curve=left?fuel_calibration_left_:fuel_calibration_right_; if(curve.size()>=5)return false;
    const double raw=left?engine_snapshot_.fuel_left_gal.value:engine_snapshot_.fuel_right_gal.value;
    if(!curve.isEmpty()&&std::abs(raw-curve.constLast().toMap().value(QStringLiteral("raw")).toDouble())<0.01)return false;
    const double capacity=qMax(0.1,fuel_on_board_gallons_/2.0);
    QVariantMap point; point.insert(QStringLiteral("percent"),curve.size()*25); point.insert(QStringLiteral("raw"),raw); point.insert(QStringLiteral("gallons"),capacity*curve.size()/4.0); curve.append(point);
    persistFuelComputer(); emit fuelComputerChanged(); return true;
}

bool EmsController::clearFuelCalibration(const QString& tank) {
    const auto normalized=tank.trimmed().toUpper(); if(normalized==QStringLiteral("LEFT"))fuel_calibration_left_.clear();else if(normalized==QStringLiteral("RIGHT"))fuel_calibration_right_.clear();else return false;
    persistFuelComputer(); emit fuelComputerChanged(); return true;
}

void EmsController::loadFuelEventHistory() {
    fuel_event_history_.clear(); QFile file(fuel_event_log_path_); if(!file.open(QIODevice::ReadOnly|QIODevice::Text))return;
    while(!file.atEnd()){const auto doc=QJsonDocument::fromJson(file.readLine());if(doc.isObject())fuel_event_history_.append(doc.object().toVariantMap());if(fuel_event_history_.size()>50)fuel_event_history_.removeFirst();}
}

void EmsController::recordFuelEvent(const QString& state) {
    QVariantMap event{{QStringLiteral("utc"),QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{QStringLiteral("state"),state},{QStringLiteral("rpm"),engine_snapshot_.rpm.value},{QStringLiteral("left_gal"),fuelLeft()},{QStringLiteral("right_gal"),fuelRight()},{QStringLiteral("remaining_gal"),fuelRemainingGallons()},{QStringLiteral("flow_gph"),engine_snapshot_.fuel_flow_gph.value},{QStringLiteral("excess_gph"),fuel_leak_excess_gph_},{QStringLiteral("simulated"),engine_snapshot_.fuel_flow_gph.is_simulated()}};
    QDir().mkpath(QFileInfo(fuel_event_log_path_).absolutePath()); QFile file(fuel_event_log_path_); if(file.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)){file.write(QJsonDocument(QJsonObject::fromVariantMap(event)).toJson(QJsonDocument::Compact));file.write("\n");file.flush();}
    fuel_event_history_.append(event);if(fuel_event_history_.size()>50)fuel_event_history_.removeFirst();
}

void EmsController::loadCentralBlackBoxCache() {
    QFileInfo info(central_blackbox_cache_path_); if(!info.exists()){central_blackbox_state_=QStringLiteral("OFFLINE");return;}
    const qint64 age=info.lastModified().msecsTo(QDateTime::currentDateTime()); QFile file(central_blackbox_cache_path_); if(!file.open(QIODevice::ReadOnly)){central_blackbox_state_=QStringLiteral("OFFLINE");return;}
    const auto root=QJsonDocument::fromJson(file.readAll()).object(); central_blackbox_nodes_=root.value("health").toObject().value("nodes").toArray().toVariantList();central_blackbox_maintenance_=root.value("health").toObject().value("maintenance").toObject().toVariantMap();const auto archives=root.value("archives").toObject();central_blackbox_archives_=archives.value("archives").toArray().toVariantList();central_blackbox_archive_summary_=archives.value("summary").toObject().toVariantMap(); central_fuel_events_.clear();
    for(const auto& item:root.value("events").toObject().value("events").toArray()){auto outer=item.toObject();auto event=outer.value("event").toObject().toVariantMap();event.insert("source",outer.value("source").toString());event.insert("received_utc",outer.value("received_utc").toString());central_fuel_events_.append(event);}
    central_blackbox_state_=age<=10000?QStringLiteral("ONLINE"):QStringLiteral("LINK LOST");
}

bool EmsController::requestCentralBlackBoxAction(const QString& action) {
    const auto normalized=action.trimmed().toUpper();if(normalized!=QStringLiteral("CREATE_INCIDENT")&&normalized!=QStringLiteral("VERIFY_SIGNATURES")&&normalized!=QStringLiteral("EXPORT_USB"))return false;
    QDir().mkpath(QFileInfo(central_blackbox_request_path_).absolutePath());QFile file(central_blackbox_request_path_);if(!file.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text))return false;
    QJsonObject request{{"action",normalized},{"request_id",QString::number(QDateTime::currentMSecsSinceEpoch())}};file.write(QJsonDocument(request).toJson(QJsonDocument::Compact));file.write("\n");file.flush();return true;
}

void EmsController::updateFuelLeakMonitor(efis::MonoTime now) {
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

void EmsController::resetFuelUsed() {
    if (engine_snapshot_.fuel_flow_gph.is_simulated()) simulated_fuel_used_gallons_ = 0.0;
    else { fuel_used_gallons_ = 0.0; persistFuelComputer(); }
    emit fuelComputerChanged();
}

void EmsController::loadAircraftProfile() {
    const auto path = qEnvironmentVariable("EFIS_AIRCRAFT_PROFILE",
                                           "config/active/aircraft-profile.json");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning().noquote() << "event=aircraft_profile_unavailable path=" << path;
        return;
    }
    const auto document = QJsonDocument::fromJson(file.readAll());
    profile_id_ = document.object().value("profile_id").toString(QStringLiteral("unconfigured"));
    const auto engine = document.object().value("engine").toObject();
    engine_profile_name_ = engine.value("manufacturer").toString() +
        QStringLiteral(" ") + engine.value("model").toString();
    engine_serial_number_ = engine.value("serial_number").toString(QStringLiteral("TO BE ENTERED"));
    engine_limits_ = engine.value("limits").toObject().toVariantMap();
    engine_capabilities_ = engine.value("capabilities").toObject().toVariantMap();
    const auto runtime = engine.value("runtime_tracking").toObject();
    runtime_tracker_ = efis::engine::RuntimeTracker{
        {runtime.value("engine_run_threshold_rpm").toDouble(500.0),
         runtime.value("tach_reference_rpm").toDouble(5000.0)},
        {runtime.value("initial_master_hours").toDouble(),
         runtime.value("initial_engine_hours").toDouble(),
         runtime.value("initial_tach_hours").toDouble(),
         runtime.value("initial_flight_hours").toDouble()}};
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
    const auto systems = document.object().value("systems").toObject();
    const auto gear = systems.value("landing_gear").toObject();
    landing_gear_installed_ = gear.value("installed").toBool(false);
    retractable_gear_ = gear.value("retractable").toBool(false);
    const auto flaps = systems.value("flaps").toObject();
    flaps_installed_ = flaps.value("installed").toBool(false);
    for (const auto detent : flaps.value("detents_deg").toArray())
        flap_detents_ << detent.toDouble();
    const auto electrical = systems.value("electrical").toObject();
    for (const auto item : electrical.value("buses").toArray()) electrical_buses_ << item.toObject().toVariantMap();
    for (const auto item : electrical.value("sources").toArray()) electrical_sources_ << item.toObject().toVariantMap();
    for (const auto item : electrical.value("loads").toArray())
        electrical_loads_ << item.toObject().toVariantMap();
}

void EmsController::loadOperationalCatalog() {
    const QString configured = qEnvironmentVariable("EFIS_CATALOG_INDEX");
    const QDir app{QCoreApplication::applicationDirPath()};
    const QStringList candidates = configured.isEmpty()
        ? QStringList{QDir::current().filePath("data/catalog/efis-catalog-index.json"),
                      app.filePath("../../data/catalog/efis-catalog-index.json")}
        : QStringList{configured};
    const auto draftPath = qEnvironmentVariable("EFIS_OPERATIONAL_PROFILE_DRAFT",
        QDir::homePath() + QStringLiteral("/.local/state/efis/operational-profile-draft.json"));
    for (const auto& path : candidates) {
        if (!QFileInfo::exists(path)) continue;
        if (operational_profile_model_.load(path, draftPath)) {
            qInfo().noquote() << "event=operational_catalog_loaded profiles="
                              << operational_profile_model_.profiles().size()
                              << "fields=" << operational_profile_model_.summary().value("total").toInt();
            emit operationalProfileChanged();
            return;
        }
    }
    qWarning().noquote() << "event=operational_catalog_unavailable status="
                         << operational_profile_model_.status();
}

bool EmsController::selectOperationalProfile(const QString& profileKey) {
    const bool result = operational_profile_model_.selectProfile(profileKey);
    emit operationalProfileChanged();
    return result;
}

bool EmsController::selectOperationalDomain(const QString& domain) {
    const bool result = operational_profile_model_.selectDomain(domain);
    emit operationalProfileChanged();
    return result;
}

bool EmsController::selectOperationalField(const QString& parameterKey) {
    const bool result = operational_profile_model_.selectField(parameterKey);
    emit operationalProfileChanged();
    return result;
}

void EmsController::filterOperationalFields(const QString& filter) {
    operational_profile_model_.setFilter(filter);
    emit operationalProfileChanged();
}

bool EmsController::stageOperationalField(const QString& disposition, const QString& valueText,
    const QString& rationale, const QString& sourceDocument,
    const QString& sourceRevision, const QString& sourcePage) {
    const bool result = operational_profile_model_.stageSelectedField(disposition, valueText,
        rationale, sourceDocument, sourceRevision, sourcePage);
    emit operationalProfileChanged();
    return result;
}

bool EmsController::selectNextOperationalIssue() {
    const bool result = operational_profile_model_.selectNextIssue();
    emit operationalProfileChanged();
    return result;
}

bool EmsController::preloadOperationalSimulation(const QString& profileKey) {
    const bool result = operational_profile_model_.preloadSimulationProfile(profileKey);
    emit operationalProfileChanged();
    return result;
}

bool EmsController::importOperationalDraft(const QString& path) {
    const bool result = operational_profile_model_.importDraft(path);
    emit operationalProfileChanged();
    return result;
}

bool EmsController::validateOperationalDraft() {
    const bool result = operational_profile_model_.validateDraft();
    emit operationalProfileChanged();
    return result;
}

bool EmsController::discardOperationalDraft() {
    const bool result = operational_profile_model_.discardDraft();
    emit operationalProfileChanged();
    return result;
}

bool EmsController::sensorCalibrationReady() const {
    for (const auto& item : engine_sensor_configuration_) {
        const auto sensor = item.toMap();
        if (sensor.value(QStringLiteral("required")).toBool() &&
            sensor.value(QStringLiteral("calibration_state")).toString() != QStringLiteral("VERIFIED")) return false;
    }
    return !engine_sensor_configuration_.isEmpty();
}

bool EmsController::cycleSensorCalibration(int index) {
    if (index < 0 || index >= engine_sensor_configuration_.size()) return false;
    auto sensor = engine_sensor_configuration_[index].toMap();
    const auto current = sensor.value(QStringLiteral("calibration_state")).toString();
    const bool liveCalibration = sensor.value(QStringLiteral("calibration_source")).toString() == QStringLiteral("LIVE") &&
        sensor.value(QStringLiteral("zero_raw")).isValid() && sensor.value(QStringLiteral("span_raw")).isValid();
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

bool EmsController::cycleSensorType(int index) {
    if (index < 0 || index >= engine_sensor_configuration_.size()) return false;
    static const QStringList types{QStringLiteral("UNSELECTED"), QStringLiteral("FREQUENCY"),
        QStringLiteral("THERMOCOUPLE K"), QStringLiteral("PT1000"), QStringLiteral("VDO RESISTIVE"),
        QStringLiteral("0-5 V"), QStringLiteral("CAN DIGITAL")};
    auto sensor = engine_sensor_configuration_[index].toMap();
    const auto current = sensor.value(QStringLiteral("configured_type")).toString();
    const int position = types.indexOf(current);
    const auto next = types.at((position + 1 + types.size()) % types.size());
    sensor.insert(QStringLiteral("configured_type"), next);
    sensor.insert(QStringLiteral("calibration_state"), QStringLiteral("NOT CONFIGURED"));
    engine_sensor_configuration_[index] = sensor;
    ++calibration_revision_;
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-calibration")};
    const auto signal = sensor.value(QStringLiteral("signal")).toString();
    settings.setValue(QStringLiteral("sensors/%1/type").arg(signal), next);
    settings.setValue(QStringLiteral("sensors/%1/state").arg(signal), QStringLiteral("NOT CONFIGURED"));
    settings.setValue(QStringLiteral("revision"), calibration_revision_); settings.sync();
    emit sensorConfigurationChanged(); return settings.status() == QSettings::NoError;
}

bool EmsController::cycleSensorInput(int index) {
    if (index < 0 || index >= engine_sensor_configuration_.size()) return false;
    static const QStringList inputs{QStringLiteral("UNASSIGNED"), QStringLiteral("CAN A"),
        QStringLiteral("CAN B"), QStringLiteral("ADC 1"), QStringLiteral("ADC 2"),
        QStringLiteral("FREQ 1"), QStringLiteral("FREQ 2")};
    auto sensor = engine_sensor_configuration_[index].toMap();
    const auto current = sensor.value(QStringLiteral("input")).toString();
    const auto next = inputs.at((inputs.indexOf(current) + 1 + inputs.size()) % inputs.size());
    sensor.insert(QStringLiteral("input"), next);
    sensor.insert(QStringLiteral("calibration_state"), QStringLiteral("NOT CONFIGURED"));
    engine_sensor_configuration_[index] = sensor;
    ++calibration_revision_;
    QSettings settings{QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("EFIS-NEXT"), QStringLiteral("ems-calibration")};
    const auto signal = sensor.value(QStringLiteral("signal")).toString();
    settings.setValue(QStringLiteral("sensors/%1/input").arg(signal), next);
    settings.setValue(QStringLiteral("sensors/%1/state").arg(signal), QStringLiteral("NOT CONFIGURED"));
    settings.setValue(QStringLiteral("revision"), calibration_revision_); settings.sync();
    emit sensorConfigurationChanged(); return settings.status() == QSettings::NoError;
}

bool EmsController::captureSensorPoint(int index, const QString& point) {
    if (index < 0 || index >= engine_sensor_configuration_.size()) return false;
    auto sensor = engine_sensor_configuration_[index].toMap();
    const auto signal = sensor.value(QStringLiteral("signal")).toString();
    double raw = 0.0; bool simulated = true; double spanReference = 100.0;
    if (signal == QStringLiteral("RPM")) { raw=engine_snapshot_.rpm.value; simulated=engine_snapshot_.rpm.is_simulated(); spanReference=5000; }
    else if (signal == QStringLiteral("OIL_PRESSURE")) { raw=engine_snapshot_.oil_pressure_psi.value; simulated=engine_snapshot_.oil_pressure_psi.is_simulated(); spanReference=100; }
    else if (signal == QStringLiteral("OIL_TEMPERATURE") || signal == QStringLiteral("COOLANT_TEMPERATURE")) { raw=engine_snapshot_.oil_temperature_c.value; simulated=engine_snapshot_.oil_temperature_c.is_simulated(); spanReference=120; }
    else if (signal == QStringLiteral("CHT")) { raw=engine_snapshot_.cht_c.front().value; simulated=engine_snapshot_.cht_c.front().is_simulated(); spanReference=200; }
    else if (signal == QStringLiteral("EGT")) { raw=engine_snapshot_.egt_c.front().value; simulated=engine_snapshot_.egt_c.front().is_simulated(); spanReference=900; }
    else if (signal == QStringLiteral("FUEL_PRESSURE")) { raw=engine_snapshot_.fuel_pressure_psi.value; simulated=engine_snapshot_.fuel_pressure_psi.is_simulated(); spanReference=10; }
    else if (signal == QStringLiteral("FUEL_FLOW")) { raw=engine_snapshot_.fuel_flow_gph.value; simulated=engine_snapshot_.fuel_flow_gph.is_simulated(); spanReference=20; }
    else if (signal == QStringLiteral("MANIFOLD_PRESSURE")) { raw=engine_snapshot_.manifold_pressure_inhg.value; simulated=engine_snapshot_.manifold_pressure_inhg.is_simulated(); spanReference=40; }
    else return false;
    const auto normalized = point.trimmed().toUpper(); if (normalized != QStringLiteral("ZERO") && normalized != QStringLiteral("SPAN")) return false;
    sensor.insert(normalized == QStringLiteral("ZERO") ? QStringLiteral("zero_raw") : QStringLiteral("span_raw"), raw);
    sensor.insert(QStringLiteral("calibration_source"), simulated ? QStringLiteral("BENCH") : QStringLiteral("LIVE"));
    const auto zero = sensor.value(QStringLiteral("zero_raw")); const auto span = sensor.value(QStringLiteral("span_raw"));
    if (zero.isValid() && span.isValid() && std::abs(span.toDouble()-zero.toDouble()) > 1e-9) {
        const double gain = spanReference/(span.toDouble()-zero.toDouble()); sensor.insert(QStringLiteral("gain"),gain); sensor.insert(QStringLiteral("offset"),-zero.toDouble()*gain);
        sensor.insert(QStringLiteral("calibration_state"), QStringLiteral("CONFIGURED"));
    }
    engine_sensor_configuration_[index]=sensor; ++calibration_revision_;
    QSettings settings{QSettings::IniFormat,QSettings::UserScope,QStringLiteral("EFIS-NEXT"),QStringLiteral("ems-calibration")};
    const auto base=QStringLiteral("sensors/%1/").arg(signal); settings.setValue(base+(normalized==QStringLiteral("ZERO")?"zero_raw":"span_raw"),raw);
    settings.setValue(base+"source",sensor.value("calibration_source")); settings.setValue(base+"gain",sensor.value("gain")); settings.setValue(base+"offset",sensor.value("offset")); settings.setValue(base+"state",sensor.value("calibration_state")); settings.setValue("revision",calibration_revision_); settings.sync();
    emit sensorConfigurationChanged(); return settings.status()==QSettings::NoError;
}

bool EmsController::startEngineSensorTest(const QString& signal, bool benchSimulation) {
    bool known=false; for(const auto& item:engine_sensor_configuration_) if(item.toMap().value("signal").toString()==signal){known=true;break;}
    if(!known)return false; sensor_test_signal_=signal; sensor_test_bench_=benchSimulation; sensor_test_ticks_=0; sensor_test_diagnostic_=QStringLiteral("NORMAL");
    sensor_test_status_=benchSimulation?QStringLiteral("BENCH SIMULATION"):QStringLiteral("WAITING FOR LIVE PROVIDER");
    sensor_test_raw_=sensor_test_value_=sensor_test_rate_hz_=0; sensor_test_age_ms_=0; emit sensorTestChanged(); return true;
}

void EmsController::stopEngineSensorTest(){sensor_test_bench_=false;sensor_test_status_=QStringLiteral("IDLE");emit sensorTestChanged();}

void EmsController::cycleSensorTestDiagnostic(){
    if(!sensor_test_bench_)return;static const QStringList modes{QStringLiteral("NORMAL"),QStringLiteral("TIMEOUT"),QStringLiteral("OPEN CIRCUIT"),QStringLiteral("SHORT CIRCUIT"),QStringLiteral("UNSTABLE")};
    sensor_test_diagnostic_=modes.at((modes.indexOf(sensor_test_diagnostic_)+1)%modes.size());emit sensorTestChanged();
}

void EmsController::cycleElectricalTestMode(){
    static const QStringList modes{QStringLiteral("NORMAL"),QStringLiteral("ALTERNATOR OFFLINE"),QStringLiteral("MAIN BUS LOW"),QStringLiteral("ESSENTIAL BUS ISOLATED"),QStringLiteral("BATTERY LOW")};
    electrical_test_mode_=modes.at((modes.indexOf(electrical_test_mode_)+1)%modes.size()); emit electricalTestChanged();
}

void EmsController::loadMaintenance(){
    const QStringList ids{"OIL_FILTER","ENGINE_INSPECTION","SPARK_PLUGS","COOLING_SYSTEM","GEARBOX_CLUTCH","ANNUAL_INSPECTION"};
    const QStringList labels{"OIL / FILTER SERVICE","ENGINE INSPECTION","SPARK PLUG INSPECTION","COOLING SYSTEM","GEARBOX / CLUTCH","ANNUAL INSPECTION"};
    QSettings settings{QSettings::IniFormat,QSettings::UserScope,"EFIS-NEXT","ems-maintenance"};
    for(int i=0;i<ids.size();++i){QVariantMap task{{"id",ids[i]},{"label",labels[i]}};const auto base=QStringLiteral("tasks/%1/").arg(ids[i]);task["interval_hours"]=settings.value(base+"interval_hours",0).toInt();task["last_hours"]=settings.value(base+"last_hours",0.0).toDouble();task["last_utc"]=settings.value(base+"last_utc",QStringLiteral("NEVER")).toString();maintenance_tasks_<<task;}
}

QString EmsController::maintenanceStatus() const {for(const auto& item:maintenance_tasks_){const auto t=item.toMap();const int interval=t.value("interval_hours").toInt();if(interval<=0)return QStringLiteral("PROGRAM REQUIRED");if(engineHours()>=t.value("last_hours").toDouble()+interval)return QStringLiteral("SERVICE DUE");}return QStringLiteral("CURRENT");}

void EmsController::cycleMaintenanceInterval(int index){if(index<0||index>=maintenance_tasks_.size())return;static const QList<int> values{0,25,50,100,200,600};auto task=maintenance_tasks_[index].toMap();const int current=task.value("interval_hours").toInt();task["interval_hours"]=values[(values.indexOf(current)+1)%values.size()];maintenance_tasks_[index]=task;QSettings s{QSettings::IniFormat,QSettings::UserScope,"EFIS-NEXT","ems-maintenance"};s.setValue(QStringLiteral("tasks/%1/interval_hours").arg(task.value("id").toString()),task.value("interval_hours"));s.sync();emit maintenanceChanged();}

bool EmsController::completeMaintenanceTask(int index){if(index<0||index>=maintenance_tasks_.size())return false;auto task=maintenance_tasks_[index].toMap();if(task.value("interval_hours").toInt()<=0)return false;task["last_hours"]=engineHours();task["last_utc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);maintenance_tasks_[index]=task;QSettings s{QSettings::IniFormat,QSettings::UserScope,"EFIS-NEXT","ems-maintenance"};const auto base=QStringLiteral("tasks/%1/").arg(task.value("id").toString());s.setValue(base+"last_hours",task.value("last_hours"));s.setValue(base+"last_utc",task.value("last_utc"));s.sync();emit maintenanceChanged();return s.status()==QSettings::NoError;}

void EmsController::persistRuntime() {
    if (!runtime_store_ || runtime_persistence_state_ == QStringLiteral("INTEGRITY FAULT")) return;
    try {
        const auto wall_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        ++runtime_revision_;
        runtime_store_->save({profile_id_.toStdString(), engine_serial_number_.toStdString(),
                              runtime_tracker_.totals(), static_cast<std::uint64_t>(runtime_revision_), wall_ms});
        runtime_persistence_state_ = engine_snapshot_.rpm.is_simulated()
            ? QStringLiteral("SAVED • SIM RPM EXCLUDED")
            : QStringLiteral("SAVED");
    } catch (const std::exception& error) {
        runtime_persistence_state_ = QStringLiteral("WRITE FAULT");
        qWarning().noquote() << "event=engine_runtime_save_failed reason=" << error.what();
    }
}

QString EmsController::gearState() const {
    if (!landing_gear_installed_) return QStringLiteral("NOT INSTALLED");
    return gear_nose_locked_ && gear_left_locked_ && gear_right_locked_
        ? QStringLiteral("DOWN & LOCKED") : QStringLiteral("GEAR UNSAFE");
}

double EmsController::engineRpm() const { return engine_snapshot_.rpm.value; }
double EmsController::manifoldPressure() const { return engine_snapshot_.manifold_pressure_inhg.value; }
double EmsController::oilPressure() const { return engine_snapshot_.oil_pressure_psi.value; }
double EmsController::oilTemperature() const { return engine_snapshot_.oil_temperature_c.value; }
double EmsController::fuelPressure() const { return engine_snapshot_.fuel_pressure_psi.value; }
double EmsController::fuelFlow() const { return engine_snapshot_.fuel_flow_gph.value; }
double EmsController::fuelLeft() const { return calibratedFuel(engine_snapshot_.fuel_left_gal.value,fuel_calibration_left_); }
double EmsController::fuelRight() const { return calibratedFuel(engine_snapshot_.fuel_right_gal.value,fuel_calibration_right_); }
double EmsController::busVoltage() const { return engine_snapshot_.bus_voltage_v.value; }
double EmsController::alternatorCurrent() const { return engine_snapshot_.alternator_current_a.value; }

QVariantList EmsController::chtValues() const {
    QVariantList values;
    for (const auto& sample : engine_snapshot_.cht_c) values << sample.value;
    return values;
}

QVariantList EmsController::egtValues() const {
    QVariantList values;
    for (const auto& sample : engine_snapshot_.egt_c) values << sample.value;
    return values;
}

int EmsController::engineHealth() const {
    return static_cast<int>(engine_snapshot_.health);
}

QString EmsController::engineFaultMode() const {
    using efis::engine::EngineFault;
    switch (engine_snapshot_.injected_fault) {
        case EngineFault::none: return QStringLiteral("NONE");
        case EngineFault::low_oil_pressure: return QStringLiteral("LOW OIL PRESS");
        case EngineFault::high_cht: return QStringLiteral("HIGH CHT 3");
        case EngineFault::alternator_failure: return QStringLiteral("ALTERNATOR");
        case EngineFault::low_fuel_pressure: return QStringLiteral("LOW FUEL PRESS");
    }
    return QStringLiteral("NONE");
}

void EmsController::cycleEngineFault() {
    using efis::engine::EngineFault;
    switch (engine_provider_.fault()) {
        case EngineFault::none: engine_provider_.set_fault(EngineFault::low_oil_pressure); break;
        case EngineFault::low_oil_pressure: engine_provider_.set_fault(EngineFault::high_cht); break;
        case EngineFault::high_cht: engine_provider_.set_fault(EngineFault::alternator_failure); break;
        case EngineFault::alternator_failure: engine_provider_.set_fault(EngineFault::low_fuel_pressure); break;
        case EngineFault::low_fuel_pressure: engine_provider_.set_fault(EngineFault::none); break;
    }
    qInfo().noquote() << "event=engine_fault_injection node=ems mode="
                      << engineFaultMode();
}

void EmsController::updateRecorderStatus(efis::MonoTime now) {
    if (last_recorder_check_ != efis::MonoTime{} &&
        now - last_recorder_check_ < kRecorderPollInterval) {
        return;
    }
    last_recorder_check_ = now;
    try {
        const auto status = efis::recording::read_recorder_status(
            recorder_status_path_.toStdString());
        const auto wall_now = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        const auto age = wall_now >= status.updated_wall_ms
            ? wall_now - status.updated_wall_ms : 0;
        black_box_state_ = age > kRecorderStaleMs
            ? QStringLiteral("STALE")
            : QString::fromStdString(status.state);
        black_box_records_ = static_cast<qulonglong>(status.records);
    } catch (...) {
        // Nessun registratore su questo nodo e' una condizione normale, non un
        // errore: il nodo EMS deve continuare a mostrare il motore comunque.
        black_box_state_ = QStringLiteral("OFFLINE");
        black_box_records_ = 0;
    }
}

void EmsController::updateSnapshot() {
    const auto now = efis::MonoClock::now();
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - started_at_);
    bool use_simulation = true;
    if (engine_network_source_) {
        const auto network = engine_network_source_->poll(now);
        switch (engine_network_source_->link_state(now)) {
            case efis::protocol::edp::LinkState::live:
                engine_snapshot_ = network;
                engine_data_state_ = network.rpm.is_simulated()
                    ? QStringLiteral("LIVE • SIM") : QStringLiteral("LIVE");
                data_source_state_ = network.rpm.is_simulated()
                    ? QStringLiteral("NETWORK • SIM") : QStringLiteral("NETWORK");
                sensor_node_state_ = network.rpm.is_simulated()
                    ? QStringLiteral("BLACKBOX 47 • SIM") : QStringLiteral("BLACKBOX 47");
                use_simulation = false;
                break;
            case efis::protocol::edp::LinkState::stale:
                engine_snapshot_ = network;
                engine_data_state_ = network.rpm.is_simulated()
                    ? QStringLiteral("STALE • SIM") : QStringLiteral("STALE");
                data_source_state_ = network.rpm.is_simulated()
                    ? QStringLiteral("NETWORK • SIM") : QStringLiteral("NETWORK");
                sensor_node_state_ = network.rpm.is_simulated()
                    ? QStringLiteral("BLACKBOX STALE • SIM") : QStringLiteral("BLACKBOX STALE");
                use_simulation = false;
                break;
            case efis::protocol::edp::LinkState::waiting:
                engine_data_state_ = QStringLiteral("SIM FALLBACK • WAITING");
                sensor_node_state_ = QStringLiteral("BLACKBOX WAITING");
                break;
            case efis::protocol::edp::LinkState::lost:
                engine_data_state_ = QStringLiteral("SIM FALLBACK • LINK LOST");
                sensor_node_state_ = QStringLiteral("BLACKBOX LINK LOST");
                break;
        }
    }
    if (use_simulation) {
        engine_snapshot_ = engine_provider_.sample(now, elapsed);
        data_source_state_ = QStringLiteral("SIMULATION");
        if (!engine_network_source_) sensor_node_state_ = QStringLiteral("SIMULATED");
    }
    if (engine_data_state_ != last_logged_engine_data_state_) {
        last_logged_engine_data_state_ = engine_data_state_;
        qInfo().noquote() << "event=ems_engine_data_state state=" << engine_data_state_
                          << "source=" << data_source_state_
                          << "node=" << sensor_node_state_;
    }
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
    if (last_runtime_update_ != efis::MonoTime{}) {
        const auto seconds = std::chrono::duration<double>(now - last_runtime_update_).count();
        const double accountable_rpm = engine_snapshot_.rpm.is_simulated()
            ? 0.0 : engine_snapshot_.rpm.value;
        runtime_tracker_.update(accountable_rpm, seconds, false);
    }
    last_runtime_update_ = now;
    if (last_runtime_save_ == efis::MonoTime{} || now - last_runtime_save_ >= 10s) {
        last_runtime_save_ = now;
        persistRuntime();
    }
    updateRecorderStatus(now);
    if(sensor_test_bench_){++sensor_test_ticks_;const double phase=double(sensor_test_ticks_)*0.06;double base=100;
        if(sensor_test_signal_=="RPM")base=5000;else if(sensor_test_signal_=="OIL_PRESSURE")base=58;else if(sensor_test_signal_=="OIL_TEMPERATURE"||sensor_test_signal_=="COOLANT_TEMPERATURE")base=96;else if(sensor_test_signal_=="CHT")base=105;else if(sensor_test_signal_=="EGT")base=780;else if(sensor_test_signal_=="FUEL_PRESSURE")base=4.6;else if(sensor_test_signal_=="FUEL_FLOW")base=4.8;else if(sensor_test_signal_=="MANIFOLD_PRESSURE")base=24.5;
        if(sensor_test_diagnostic_=="TIMEOUT"){sensor_test_rate_hz_=0;sensor_test_age_ms_+=50;sensor_test_status_=QStringLiteral("BENCH • TIMEOUT");}
        else {sensor_test_age_ms_=0;sensor_test_rate_hz_=20;
            if(sensor_test_diagnostic_=="OPEN CIRCUIT"){sensor_test_raw_=4095;sensor_test_status_=QStringLiteral("BENCH • OPEN CIRCUIT");}
            else if(sensor_test_diagnostic_=="SHORT CIRCUIT"){sensor_test_raw_=0;sensor_test_status_=QStringLiteral("BENCH • SHORT CIRCUIT");}
            else if(sensor_test_diagnostic_=="UNSTABLE"){sensor_test_raw_=base+std::sin(phase*11)*base*.28;sensor_test_status_=QStringLiteral("BENCH • UNSTABLE SIGNAL");}
            else {sensor_test_raw_=base+std::sin(phase)*qMax(0.1,base*.012);sensor_test_status_=QStringLiteral("BENCH • DATA VALID");}
            sensor_test_value_=sensor_test_raw_;}
        if(sensor_test_ticks_%4==0)emit sensorTestChanged();}
    emit snapshotChanged();
}
