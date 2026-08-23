#include "ems_controller.hpp"

#include "efis/recording/recorder_status.hpp"

#include <QDebug>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStringList>

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
    const auto next = current == QStringLiteral("NOT CONFIGURED") ? QStringLiteral("CONFIGURED")
                    : current == QStringLiteral("CONFIGURED") ? QStringLiteral("VERIFIED")
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
    emit snapshotChanged();
}
