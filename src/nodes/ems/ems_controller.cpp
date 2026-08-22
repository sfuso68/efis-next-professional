#include "ems_controller.hpp"

#include "efis/recording/recorder_status.hpp"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
    loadAircraftProfile();
    recorder_status_path_ = qEnvironmentVariable("EFIS_RECORDER_STATUS",
                                                 "run/recorder.status");
    timer_.setTimerType(Qt::PreciseTimer);
    timer_.setInterval(kUpdateIntervalMs);
    connect(&timer_, &QTimer::timeout, this, &EmsController::updateSnapshot);
    updateSnapshot();
    timer_.start();
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
    const auto systems = document.object().value("systems").toObject();
    const auto gear = systems.value("landing_gear").toObject();
    landing_gear_installed_ = gear.value("installed").toBool(false);
    retractable_gear_ = gear.value("retractable").toBool(false);
    const auto flaps = systems.value("flaps").toObject();
    flaps_installed_ = flaps.value("installed").toBool(false);
    for (const auto detent : flaps.value("detents_deg").toArray())
        flap_detents_ << detent.toDouble();
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
    updateRecorderStatus(now);
    emit snapshotChanged();
}
