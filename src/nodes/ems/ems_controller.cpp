#include "ems_controller.hpp"

#include "efis/recording/recorder_status.hpp"

#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStringList>
#include <QList>
#include <cmath>

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
}

EmsController::EmsController(QObject* parent) : QObject(parent) {
    loadUnitSystem();
    loadAircraftProfile();
    loadMaintenance();
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
double EmsController::fuelLeft() const { return engine_snapshot_.fuel_left_gal.value; }
double EmsController::fuelRight() const { return engine_snapshot_.fuel_right_gal.value; }
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
    switch (engine_provider_.fault()) {
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
    engine_snapshot_ = engine_provider_.sample(now, elapsed);
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
        if(sensor_test_signal_=="RPM")base=2450;else if(sensor_test_signal_=="OIL_PRESSURE")base=72;else if(sensor_test_signal_=="OIL_TEMPERATURE"||sensor_test_signal_=="COOLANT_TEMPERATURE")base=95;else if(sensor_test_signal_=="CHT")base=180;else if(sensor_test_signal_=="EGT")base=740;else if(sensor_test_signal_=="FUEL_PRESSURE")base=4.8;else if(sensor_test_signal_=="FUEL_FLOW")base=9.6;else if(sensor_test_signal_=="MANIFOLD_PRESSURE")base=24;
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
