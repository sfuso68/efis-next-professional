#pragma once

// Controllore del nodo EMS.
//
// Espone alla pagina motore esattamente le grandezze del motore, e nulla piu'.
// Non e' una riduzione di PfdController: e' il confine di processo previsto
// dalla ADR-002 reso reale. Un nodo che non conosce assetto, navigazione,
// terreno o meteo non puo' guastarsi per una loro ragione, e la superficie che
// pubblica e' la stessa che la pagina EMS consuma dentro il PFD completo.

#include "efis/providers/engine_simulation_provider.hpp"
#include "efis/providers/edp_udp.hpp"
#include "efis/core/engine_runtime.hpp"
#include "efis/core/engine_runtime_store.hpp"
#include "../../setup/operational_profile_model.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

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
    Q_PROPERTY(QString engineDataState READ engineDataState NOTIFY snapshotChanged)
    Q_PROPERTY(QString dataSourceState READ dataSourceState NOTIFY snapshotChanged)
    Q_PROPERTY(QString sensorNodeState READ sensorNodeState NOTIFY snapshotChanged)
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
    Q_PROPERTY(QVariantMap engineLimits READ engineLimits CONSTANT)
    Q_PROPERTY(QVariantMap engineCapabilities READ engineCapabilities CONSTANT)
    Q_PROPERTY(double masterHours READ masterHours NOTIFY snapshotChanged)
    Q_PROPERTY(double engineHours READ engineHours NOTIFY snapshotChanged)
    Q_PROPERTY(double tachHours READ tachHours NOTIFY snapshotChanged)
    Q_PROPERTY(double flightHours READ flightHours NOTIFY snapshotChanged)
    Q_PROPERTY(bool engineRunning READ engineRunning NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList engineSensorConfiguration READ engineSensorConfiguration NOTIFY sensorConfigurationChanged)
    Q_PROPERTY(int calibrationRevision READ calibrationRevision NOTIFY sensorConfigurationChanged)
    Q_PROPERTY(bool sensorCalibrationReady READ sensorCalibrationReady NOTIFY sensorConfigurationChanged)
    Q_PROPERTY(QString sensorTestSignal READ sensorTestSignal NOTIFY sensorTestChanged)
    Q_PROPERTY(QString sensorTestStatus READ sensorTestStatus NOTIFY sensorTestChanged)
    Q_PROPERTY(double sensorTestRaw READ sensorTestRaw NOTIFY sensorTestChanged)
    Q_PROPERTY(double sensorTestValue READ sensorTestValue NOTIFY sensorTestChanged)
    Q_PROPERTY(double sensorTestRateHz READ sensorTestRateHz NOTIFY sensorTestChanged)
    Q_PROPERTY(int sensorTestAgeMs READ sensorTestAgeMs NOTIFY sensorTestChanged)
    Q_PROPERTY(QString sensorTestDiagnostic READ sensorTestDiagnostic NOTIFY sensorTestChanged)
    Q_PROPERTY(QVariantList electricalLoads READ electricalLoads CONSTANT)
    Q_PROPERTY(QVariantList electricalBuses READ electricalBuses CONSTANT)
    Q_PROPERTY(QVariantList electricalSources READ electricalSources CONSTANT)
    Q_PROPERTY(QString electricalTestMode READ electricalTestMode NOTIFY electricalTestChanged)
    Q_PROPERTY(QVariantList maintenanceTasks READ maintenanceTasks NOTIFY maintenanceChanged)
    Q_PROPERTY(QString maintenanceStatus READ maintenanceStatus NOTIFY maintenanceChanged)
    Q_PROPERTY(QString runtimePersistenceState READ runtimePersistenceState NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong runtimeRevision READ runtimeRevision NOTIFY snapshotChanged)
    Q_PROPERTY(QString runtimeAuthority READ runtimeAuthority CONSTANT)
    Q_PROPERTY(QString unitSystem READ unitSystem NOTIFY unitsChanged)
    Q_PROPERTY(double fuelUsedGallons READ fuelUsedGallons NOTIFY fuelComputerChanged)
    Q_PROPERTY(double fuelReserveGallons READ fuelReserveGallons NOTIFY fuelComputerChanged)
    Q_PROPERTY(double fuelOnBoardGallons READ fuelOnBoardGallons NOTIFY fuelComputerChanged)
    Q_PROPERTY(double fuelRemainingGallons READ fuelRemainingGallons NOTIFY fuelComputerChanged)
    Q_PROPERTY(QVariantList fuelCalibrationLeft READ fuelCalibrationLeft NOTIFY fuelComputerChanged)
    Q_PROPERTY(QVariantList fuelCalibrationRight READ fuelCalibrationRight NOTIFY fuelComputerChanged)
    Q_PROPERTY(QString fuelLeakState READ fuelLeakState NOTIFY fuelComputerChanged)
    Q_PROPERTY(double fuelLeakExcessGph READ fuelLeakExcessGph NOTIFY fuelComputerChanged)
    Q_PROPERTY(QVariantList fuelEventHistory READ fuelEventHistory NOTIFY fuelComputerChanged)
    Q_PROPERTY(QString centralBlackBoxState READ centralBlackBoxState NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList centralBlackBoxNodes READ centralBlackBoxNodes NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList centralFuelEvents READ centralFuelEvents NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantMap centralBlackBoxMaintenance READ centralBlackBoxMaintenance NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList centralBlackBoxArchives READ centralBlackBoxArchives NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantMap centralBlackBoxArchiveSummary READ centralBlackBoxArchiveSummary NOTIFY snapshotChanged)
    Q_PROPERTY(QVariantList operationalProfiles READ operationalProfiles NOTIFY operationalProfileChanged)
    Q_PROPERTY(QVariantList operationalDomains READ operationalDomains NOTIFY operationalProfileChanged)
    Q_PROPERTY(QVariantList operationalFields READ operationalFields NOTIFY operationalProfileChanged)
    Q_PROPERTY(QVariantList operationalDomainSummaries READ operationalDomainSummaries NOTIFY operationalProfileChanged)
    Q_PROPERTY(QVariantList operationalValidationIssues READ operationalValidationIssues NOTIFY operationalProfileChanged)
    Q_PROPERTY(QVariantMap operationalValidationReport READ operationalValidationReport NOTIFY operationalProfileChanged)
    Q_PROPERTY(QVariantMap operationalSummary READ operationalSummary NOTIFY operationalProfileChanged)
    Q_PROPERTY(QVariantMap operationalSelectedField READ operationalSelectedField NOTIFY operationalProfileChanged)
    Q_PROPERTY(QString operationalSelectedProfile READ operationalSelectedProfile NOTIFY operationalProfileChanged)
    Q_PROPERTY(QString operationalSelectedDomain READ operationalSelectedDomain NOTIFY operationalProfileChanged)
    Q_PROPERTY(QString operationalCatalogStatus READ operationalCatalogStatus NOTIFY operationalProfileChanged)
    Q_PROPERTY(QString operationalDraftPath READ operationalDraftPath NOTIFY operationalProfileChanged)

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
    [[nodiscard]] QString engineDataState() const { return engine_data_state_; }
    [[nodiscard]] QString dataSourceState() const { return data_source_state_; }
    [[nodiscard]] QString sensorNodeState() const { return sensor_node_state_; }
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
    [[nodiscard]] QVariantMap engineLimits() const { return engine_limits_; }
    [[nodiscard]] QVariantMap engineCapabilities() const { return engine_capabilities_; }
    [[nodiscard]] double masterHours() const { return runtime_tracker_.totals().master_hours; }
    [[nodiscard]] double engineHours() const { return runtime_tracker_.totals().engine_hours; }
    [[nodiscard]] double tachHours() const { return runtime_tracker_.totals().tach_hours; }
    [[nodiscard]] double flightHours() const { return runtime_tracker_.totals().flight_hours; }
    [[nodiscard]] bool engineRunning() const { return runtime_tracker_.engine_running(); }
    [[nodiscard]] QVariantList engineSensorConfiguration() const { return engine_sensor_configuration_; }
    [[nodiscard]] int calibrationRevision() const { return calibration_revision_; }
    [[nodiscard]] bool sensorCalibrationReady() const;
    [[nodiscard]] QString sensorTestSignal() const { return sensor_test_signal_; }
    [[nodiscard]] QString sensorTestStatus() const { return sensor_test_status_; }
    [[nodiscard]] double sensorTestRaw() const { return sensor_test_raw_; }
    [[nodiscard]] double sensorTestValue() const { return sensor_test_value_; }
    [[nodiscard]] double sensorTestRateHz() const { return sensor_test_rate_hz_; }
    [[nodiscard]] int sensorTestAgeMs() const { return sensor_test_age_ms_; }
    [[nodiscard]] QString sensorTestDiagnostic() const { return sensor_test_diagnostic_; }
    [[nodiscard]] QVariantList electricalLoads() const { return electrical_loads_; }
    [[nodiscard]] QVariantList electricalBuses() const { return electrical_buses_; }
    [[nodiscard]] QVariantList electricalSources() const { return electrical_sources_; }
    [[nodiscard]] QString electricalTestMode() const { return electrical_test_mode_; }
    [[nodiscard]] QVariantList maintenanceTasks() const { return maintenance_tasks_; }
    [[nodiscard]] QString maintenanceStatus() const;
    [[nodiscard]] QString runtimePersistenceState() const { return runtime_persistence_state_; }
    [[nodiscard]] qulonglong runtimeRevision() const { return runtime_revision_; }
    [[nodiscard]] QString runtimeAuthority() const { return QStringLiteral("EMS NODE • AUTHORITATIVE"); }
    [[nodiscard]] QString unitSystem() const { return unit_system_; }
    [[nodiscard]] double fuelUsedGallons() const { return engine_snapshot_.fuel_flow_gph.is_simulated() ? simulated_fuel_used_gallons_ : fuel_used_gallons_; }
    [[nodiscard]] double fuelReserveGallons() const { return fuel_reserve_gallons_; }
    [[nodiscard]] double fuelOnBoardGallons() const { return fuel_on_board_gallons_; }
    [[nodiscard]] double fuelRemainingGallons() const { return qMax(0.0, fuel_on_board_gallons_ - fuelUsedGallons()); }
    [[nodiscard]] QVariantList fuelCalibrationLeft() const { return fuel_calibration_left_; }
    [[nodiscard]] QVariantList fuelCalibrationRight() const { return fuel_calibration_right_; }
    [[nodiscard]] QString fuelLeakState() const { return fuel_leak_state_; }
    [[nodiscard]] double fuelLeakExcessGph() const { return fuel_leak_excess_gph_; }
    [[nodiscard]] QVariantList fuelEventHistory() const { return fuel_event_history_; }
    [[nodiscard]] QString centralBlackBoxState() const { return central_blackbox_state_; }
    [[nodiscard]] QVariantList centralBlackBoxNodes() const { return central_blackbox_nodes_; }
    [[nodiscard]] QVariantList centralFuelEvents() const { return central_fuel_events_; }
    [[nodiscard]] QVariantMap centralBlackBoxMaintenance() const { return central_blackbox_maintenance_; }
    [[nodiscard]] QVariantList centralBlackBoxArchives() const { return central_blackbox_archives_; }
    [[nodiscard]] QVariantMap centralBlackBoxArchiveSummary() const { return central_blackbox_archive_summary_; }
    [[nodiscard]] QVariantList operationalProfiles() const { return operational_profile_model_.profiles(); }
    [[nodiscard]] QVariantList operationalDomains() const { return operational_profile_model_.domains(); }
    [[nodiscard]] QVariantList operationalFields() const { return operational_profile_model_.fields(); }
    [[nodiscard]] QVariantList operationalDomainSummaries() const { return operational_profile_model_.domainSummaries(); }
    [[nodiscard]] QVariantList operationalValidationIssues() const { return operational_profile_model_.validationIssues(); }
    [[nodiscard]] QVariantMap operationalValidationReport() const { return operational_profile_model_.validationReport(); }
    [[nodiscard]] QVariantMap operationalSummary() const { return operational_profile_model_.summary(); }
    [[nodiscard]] QVariantMap operationalSelectedField() const { return operational_profile_model_.selectedField(); }
    [[nodiscard]] QString operationalSelectedProfile() const { return operational_profile_model_.selectedProfileKey(); }
    [[nodiscard]] QString operationalSelectedDomain() const { return operational_profile_model_.selectedDomain(); }
    [[nodiscard]] QString operationalCatalogStatus() const { return operational_profile_model_.status(); }
    [[nodiscard]] QString operationalDraftPath() const { return operational_profile_model_.draftPath(); }

    Q_INVOKABLE void cycleEngineFault();
    Q_INVOKABLE bool setUnitSystem(const QString& system);
    Q_INVOKABLE bool cycleSensorCalibration(int index);
    Q_INVOKABLE bool cycleSensorType(int index);
    Q_INVOKABLE bool cycleSensorInput(int index);
    Q_INVOKABLE bool captureSensorPoint(int index, const QString& point);
    Q_INVOKABLE bool startEngineSensorTest(const QString& signal, bool benchSimulation);
    Q_INVOKABLE void stopEngineSensorTest();
    Q_INVOKABLE void cycleSensorTestDiagnostic();
    Q_INVOKABLE void cycleElectricalTestMode();
    Q_INVOKABLE void cycleMaintenanceInterval(int index);
    Q_INVOKABLE bool completeMaintenanceTask(int index);
    Q_INVOKABLE bool setFuelReserveGallons(double gallons);
    Q_INVOKABLE bool setFuelOnBoardGallons(double gallons);
    Q_INVOKABLE bool captureFuelCalibrationPoint(const QString& tank);
    Q_INVOKABLE bool clearFuelCalibration(const QString& tank);
    Q_INVOKABLE bool requestCentralBlackBoxAction(const QString& action);
    Q_INVOKABLE void resetFuelUsed();
    Q_INVOKABLE bool selectOperationalProfile(const QString& profileKey);
    Q_INVOKABLE bool selectOperationalDomain(const QString& domain);
    Q_INVOKABLE bool selectOperationalField(const QString& parameterKey);
    Q_INVOKABLE void filterOperationalFields(const QString& filter);
    Q_INVOKABLE bool stageOperationalField(const QString& disposition, const QString& valueText,
        const QString& rationale, const QString& sourceDocument,
        const QString& sourceRevision, const QString& sourcePage);
    Q_INVOKABLE bool selectNextOperationalIssue();
    Q_INVOKABLE bool preloadOperationalSimulation(const QString& profileKey);
    Q_INVOKABLE bool importOperationalDraft(const QString& path);
    Q_INVOKABLE bool validateOperationalDraft();
    Q_INVOKABLE bool discardOperationalDraft();

signals:
    void snapshotChanged();
    void unitsChanged();
    void sensorConfigurationChanged();
    void sensorTestChanged();
    void electricalTestChanged();
    void maintenanceChanged();
    void fuelComputerChanged();
    void operationalProfileChanged();

private:
    void updateSnapshot();
    void updateRecorderStatus(efis::MonoTime now);
    void loadAircraftProfile();
    void persistRuntime();
    void loadUnitSystem();
    void loadMaintenance();
    void loadFuelComputer();
    void persistFuelComputer();
    void updateFuelLeakMonitor(efis::MonoTime now);
    void loadFuelEventHistory();
    void recordFuelEvent(const QString& state);
    void loadCentralBlackBoxCache();
    void loadOperationalCatalog();

    QTimer timer_;
    efis::EngineSimulationProvider engine_provider_{84};
    std::unique_ptr<efis::EdpUdpEngineSource> engine_network_source_;
    efis::engine::EngineSnapshot engine_snapshot_{};
    QString engine_data_state_{QStringLiteral("SIM FALLBACK")};
    QString data_source_state_{QStringLiteral("SIMULATION")};
    QString sensor_node_state_{QStringLiteral("SIMULATED")};
    QString last_logged_engine_data_state_;
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
    QVariantMap engine_limits_;
    QVariantMap engine_capabilities_;
    QVariantList engine_sensor_configuration_;
    QVariantList electrical_loads_;
    QVariantList electrical_buses_;
    QVariantList electrical_sources_;
    efis::engine::RuntimeTracker runtime_tracker_{};
    efis::MonoTime last_runtime_update_{};
    efis::MonoTime last_runtime_save_{};
    std::unique_ptr<efis::engine::RuntimeStore> runtime_store_;
    QString profile_id_{QStringLiteral("unconfigured")};
    QString runtime_persistence_state_{QStringLiteral("INITIALIZING")};
    qulonglong runtime_revision_{};
    int calibration_revision_{};
    QString sensor_test_signal_;
    QString sensor_test_status_{QStringLiteral("IDLE")};
    double sensor_test_raw_{};
    double sensor_test_value_{};
    double sensor_test_rate_hz_{};
    int sensor_test_age_ms_{};
    bool sensor_test_bench_{};
    qulonglong sensor_test_ticks_{};
    QString sensor_test_diagnostic_{QStringLiteral("NORMAL")};
    QString electrical_test_mode_{QStringLiteral("NORMAL")};
    QVariantList maintenance_tasks_;
    QString unit_system_{QStringLiteral("AVIATION")};
    double fuel_used_gallons_{};
    double simulated_fuel_used_gallons_{};
    double fuel_reserve_gallons_{5.0};
    double fuel_on_board_gallons_{};
    bool fuel_on_board_initialized_{};
    QVariantList fuel_calibration_left_;
    QVariantList fuel_calibration_right_;
    QString fuel_leak_state_{QStringLiteral("INITIALIZING")};
    double fuel_leak_excess_gph_{};
    double fuel_leak_last_measured_{};
    efis::MonoTime fuel_leak_last_update_{};
    efis::MonoTime fuel_leak_alarm_since_{};
    efis::MonoTime fuel_leak_suppressed_until_{};
    QString fuel_event_log_path_;
    QVariantList fuel_event_history_;
    QString central_blackbox_cache_path_;
    QString central_blackbox_state_{QStringLiteral("OFFLINE")};
    QVariantList central_blackbox_nodes_;
    QVariantList central_fuel_events_;
    QVariantMap central_blackbox_maintenance_;
    QVariantList central_blackbox_archives_;
    QVariantMap central_blackbox_archive_summary_;
    QString central_blackbox_request_path_;
    efis::MonoTime last_central_blackbox_check_{};
    efis::MonoTime last_fuel_update_{};
    efis::MonoTime last_fuel_save_{};
    efis::MonoTime last_fuel_notify_{};
    OperationalProfileModel operational_profile_model_;
};
