#pragma once

// Controllore del nodo EMS.
//
// Espone alla pagina motore esattamente le grandezze del motore, e nulla piu'.
// Non e' una riduzione di PfdController: e' il confine di processo previsto
// dalla ADR-002 reso reale. Un nodo che non conosce assetto, navigazione,
// terreno o meteo non puo' guastarsi per una loro ragione, e la superficie che
// pubblica e' la stessa che la pagina EMS consuma dentro il PFD completo.

#include "efis/providers/engine_simulation_provider.hpp"
#include "efis/core/engine_runtime.hpp"
#include "efis/core/engine_runtime_store.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <chrono>
#include <memory>

class EmsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(double engineRpm READ engineRpm NOTIFY snapshotChanged)
    Q_PROPERTY(double manifoldPressure READ manifoldPressure NOTIFY snapshotChanged)
    Q_PROPERTY(double oilPressure READ oilPressure NOTIFY snapshotChanged)
    Q_PROPERTY(double oilTemperature READ oilTemperature NOTIFY snapshotChanged)
    Q_PROPERTY(double fuelPressure READ fuelPressure NOTIFY snapshotChanged)
    Q_PROPERTY(double fuelFlow READ fuelFlow NOTIFY snapshotChanged)
    Q_PROPERTY(double fuelLeft READ fuelLeft NOTIFY snapshotChanged)
    Q_PROPERTY(double fuelRight READ fuelRight NOTIFY snapshotChanged)
    Q_PROPERTY(double busVoltage READ busVoltage NOTIFY snapshotChanged)
    Q_PROPERTY(double alternatorCurrent READ alternatorCurrent NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList chtValues READ chtValues NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList egtValues READ egtValues NOTIFY snapshotChanged)
    Q_PROPERTY(int engineHealth READ engineHealth NOTIFY snapshotChanged)
    Q_PROPERTY(QString engineFaultMode READ engineFaultMode NOTIFY snapshotChanged)
    Q_PROPERTY(QString engineDataState READ engineDataState CONSTANT)
    Q_PROPERTY(QString dataSourceState READ dataSourceState CONSTANT)
    Q_PROPERTY(QString sensorNodeState READ sensorNodeState CONSTANT)
    Q_PROPERTY(QString blackBoxState READ blackBoxState NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong blackBoxRecords READ blackBoxRecords NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong sequence READ sequence NOTIFY snapshotChanged)
    Q_PROPERTY(bool landingGearInstalled READ landingGearInstalled CONSTANT)
    Q_PROPERTY(bool retractableGear READ retractableGear CONSTANT)
    Q_PROPERTY(bool gearNoseLocked READ gearNoseLocked NOTIFY snapshotChanged)
    Q_PROPERTY(bool gearLeftLocked READ gearLeftLocked NOTIFY snapshotChanged)
    Q_PROPERTY(bool gearRightLocked READ gearRightLocked NOTIFY snapshotChanged)
    Q_PROPERTY(QString gearState READ gearState NOTIFY snapshotChanged)
    Q_PROPERTY(bool flapsInstalled READ flapsInstalled CONSTANT)
    Q_PROPERTY(QVariantList flapDetents READ flapDetents CONSTANT)
    Q_PROPERTY(double flapAngle READ flapAngle NOTIFY snapshotChanged)
    Q_PROPERTY(double flapCommandedAngle READ flapCommandedAngle NOTIFY snapshotChanged)
    Q_PROPERTY(QString engineProfileName READ engineProfileName CONSTANT)
    Q_PROPERTY(QString engineSerialNumber READ engineSerialNumber CONSTANT)
    Q_PROPERTY(double masterHours READ masterHours NOTIFY snapshotChanged)
    Q_PROPERTY(double engineHours READ engineHours NOTIFY snapshotChanged)
    Q_PROPERTY(double tachHours READ tachHours NOTIFY snapshotChanged)
    Q_PROPERTY(double flightHours READ flightHours NOTIFY snapshotChanged)
    Q_PROPERTY(bool engineRunning READ engineRunning NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList engineSensorConfiguration READ engineSensorConfiguration CONSTANT)
    Q_PROPERTY(QVariantList electricalLoads READ electricalLoads CONSTANT)
    Q_PROPERTY(QString runtimePersistenceState READ runtimePersistenceState NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong runtimeRevision READ runtimeRevision NOTIFY snapshotChanged)
    Q_PROPERTY(QString runtimeAuthority READ runtimeAuthority CONSTANT)
    Q_PROPERTY(QString unitSystem READ unitSystem NOTIFY unitsChanged)

public:
    explicit EmsController(QObject* parent = nullptr);

    [[nodiscard]] double engineRpm() const;
    [[nodiscard]] double manifoldPressure() const;
    [[nodiscard]] double oilPressure() const;
    [[nodiscard]] double oilTemperature() const;
    [[nodiscard]] double fuelPressure() const;
    [[nodiscard]] double fuelFlow() const;
    [[nodiscard]] double fuelLeft() const;
    [[nodiscard]] double fuelRight() const;
    [[nodiscard]] double busVoltage() const;
    [[nodiscard]] double alternatorCurrent() const;
    [[nodiscard]] QVariantList chtValues() const;
    [[nodiscard]] QVariantList egtValues() const;
    [[nodiscard]] int engineHealth() const;
    [[nodiscard]] QString engineFaultMode() const;
    [[nodiscard]] QString engineDataState() const { return QStringLiteral("SIM"); }
    // Finche' questo processo usa EngineSimulationProvider deve dichiararlo
    // alla barra di stato. Una proprieta' assente diventa `undefined` in QML e
    // lascia il pilota senza indicazione sull'origine dei numeri mostrati.
    [[nodiscard]] QString dataSourceState() const { return QStringLiteral("SIM"); }
    [[nodiscard]] QString sensorNodeState() const {
        return QStringLiteral("SIMULATED");
    }
    [[nodiscard]] QString blackBoxState() const { return black_box_state_; }
    [[nodiscard]] qulonglong blackBoxRecords() const { return black_box_records_; }
    [[nodiscard]] qulonglong sequence() const {
        return static_cast<qulonglong>(engine_snapshot_.sequence);
    }
    [[nodiscard]] bool landingGearInstalled() const { return landing_gear_installed_; }
    [[nodiscard]] bool retractableGear() const { return retractable_gear_; }
    [[nodiscard]] bool gearNoseLocked() const { return gear_nose_locked_; }
    [[nodiscard]] bool gearLeftLocked() const { return gear_left_locked_; }
    [[nodiscard]] bool gearRightLocked() const { return gear_right_locked_; }
    [[nodiscard]] QString gearState() const;
    [[nodiscard]] bool flapsInstalled() const { return flaps_installed_; }
    [[nodiscard]] QVariantList flapDetents() const { return flap_detents_; }
    [[nodiscard]] double flapAngle() const { return flap_angle_; }
    [[nodiscard]] double flapCommandedAngle() const { return flap_commanded_angle_; }
    [[nodiscard]] QString engineProfileName() const { return engine_profile_name_; }
    [[nodiscard]] QString engineSerialNumber() const { return engine_serial_number_; }
    [[nodiscard]] double masterHours() const { return runtime_tracker_.totals().master_hours; }
    [[nodiscard]] double engineHours() const { return runtime_tracker_.totals().engine_hours; }
    [[nodiscard]] double tachHours() const { return runtime_tracker_.totals().tach_hours; }
    [[nodiscard]] double flightHours() const { return runtime_tracker_.totals().flight_hours; }
    [[nodiscard]] bool engineRunning() const { return runtime_tracker_.engine_running(); }
    [[nodiscard]] QVariantList engineSensorConfiguration() const { return engine_sensor_configuration_; }
    [[nodiscard]] QVariantList electricalLoads() const { return electrical_loads_; }
    [[nodiscard]] QString runtimePersistenceState() const { return runtime_persistence_state_; }
    [[nodiscard]] qulonglong runtimeRevision() const { return runtime_revision_; }
    [[nodiscard]] QString runtimeAuthority() const { return QStringLiteral("EMS NODE • AUTHORITATIVE"); }
    [[nodiscard]] QString unitSystem() const { return unit_system_; }

    Q_INVOKABLE void cycleEngineFault();
    Q_INVOKABLE bool setUnitSystem(const QString& system);

signals:
    void snapshotChanged();
    void unitsChanged();

private:
    void updateSnapshot();
    void updateRecorderStatus(efis::MonoTime now);
    void loadAircraftProfile();
    void persistRuntime();
    void loadUnitSystem();

    QTimer timer_;
    efis::EngineSimulationProvider engine_provider_{84};
    efis::engine::EngineSnapshot engine_snapshot_{};
    efis::MonoTime started_at_{efis::MonoClock::now()};
    efis::MonoTime last_recorder_check_{};
    QString recorder_status_path_;
    QString black_box_state_{QStringLiteral("OFFLINE")};
    qulonglong black_box_records_{};
    bool landing_gear_installed_{};
    bool retractable_gear_{};
    bool gear_nose_locked_{true};
    bool gear_left_locked_{true};
    bool gear_right_locked_{true};
    bool flaps_installed_{};
    QVariantList flap_detents_;
    double flap_angle_{15.0};
    double flap_commanded_angle_{15.0};
    QString engine_profile_name_{QStringLiteral("UNCONFIGURED")};
    QString engine_serial_number_{QStringLiteral("TO BE ENTERED")};
    QVariantList engine_sensor_configuration_;
    QVariantList electrical_loads_;
    efis::engine::RuntimeTracker runtime_tracker_{};
    efis::MonoTime last_runtime_update_{};
    efis::MonoTime last_runtime_save_{};
    std::unique_ptr<efis::engine::RuntimeStore> runtime_store_;
    QString profile_id_{QStringLiteral("unconfigured")};
    QString runtime_persistence_state_{QStringLiteral("INITIALIZING")};
    qulonglong runtime_revision_{};
    QString unit_system_{QStringLiteral("AVIATION")};
};
