#pragma once

#include "efis/core/flight_model.hpp"
#include "efis/core/flight_source.hpp"
#include "efis/providers/simulation_source.hpp"
#include "efis/providers/ddp_udp_source.hpp"
#include "efis/core/engine_source.hpp"
#include "efis/core/engine_runtime.hpp"
#include "efis/providers/engine_simulation_source.hpp"
#include "efis/simulation/system_simulator.hpp"

#include <memory>

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class PfdController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double roll READ roll NOTIFY snapshotChanged)
    Q_PROPERTY(double pitch READ pitch NOTIFY snapshotChanged)
    Q_PROPERTY(double heading READ heading NOTIFY snapshotChanged)
    Q_PROPERTY(double airspeed READ airspeed NOTIFY snapshotChanged)
    Q_PROPERTY(double altitude READ altitude NOTIFY snapshotChanged)
    Q_PROPERTY(double verticalSpeed READ verticalSpeed NOTIFY snapshotChanged)
    Q_PROPERTY(double groundSpeed READ groundSpeed NOTIFY snapshotChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY snapshotChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY snapshotChanged)
    Q_PROPERTY(double commandedRoll READ commandedRoll NOTIFY snapshotChanged)
    Q_PROPERTY(double commandedPitch READ commandedPitch NOTIFY snapshotChanged)
    Q_PROPERTY(double selectedAltitude READ selectedAltitude NOTIFY snapshotChanged)
    Q_PROPERTY(double selectedCourse READ selectedCourse NOTIFY snapshotChanged)
    Q_PROPERTY(double selectedAirspeed READ selectedAirspeed NOTIFY snapshotChanged)
    Q_PROPERTY(double cdiDeviation READ cdiDeviation NOTIFY snapshotChanged)
    Q_PROPERTY(double glideSlope READ glideSlope NOTIFY snapshotChanged)
    Q_PROPERTY(double airspeedTrend READ airspeedTrend NOTIFY snapshotChanged)
    Q_PROPERTY(double altitudeTrend READ altitudeTrend NOTIFY snapshotChanged)
    Q_PROPERTY(bool navTo READ navTo NOTIFY snapshotChanged)
    Q_PROPERTY(bool flightDirectorActive READ flightDirectorActive NOTIFY snapshotChanged)
    Q_PROPERTY(QString annunciation READ annunciation NOTIFY snapshotChanged)
    Q_PROPERTY(int annunciationSeverity READ annunciationSeverity NOTIFY snapshotChanged)
    Q_PROPERTY(int healthLevel READ healthLevel NOTIFY snapshotChanged)
    Q_PROPERTY(bool airspeedValid READ airspeedValid NOTIFY snapshotChanged)
    Q_PROPERTY(bool altitudeValid READ altitudeValid NOTIFY snapshotChanged)
    Q_PROPERTY(bool navigationValid READ navigationValid NOTIFY snapshotChanged)
    Q_PROPERTY(QString faultMode READ faultMode NOTIFY snapshotChanged)
    Q_PROPERTY(QString simulationRole READ simulationRole NOTIFY snapshotChanged)
    Q_PROPERTY(QString attitudeSource READ attitudeSource NOTIFY snapshotChanged)
    Q_PROPERTY(bool showingSimulatedData READ showingSimulatedData NOTIFY snapshotChanged)
    Q_PROPERTY(QString dataSourceState READ dataSourceState NOTIFY snapshotChanged)
    Q_PROPERTY(QString sensorNodeState READ sensorNodeState NOTIFY snapshotChanged)
    Q_PROPERTY(QString ddpLinkState READ ddpLinkState NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong ddpAccepted READ ddpAccepted NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong ddpRejected READ ddpRejected NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong ddpSequenceGaps READ ddpSequenceGaps NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong ddpSocketErrors READ ddpSocketErrors NOTIFY snapshotChanged)
    Q_PROPERTY(QString engineDataState READ engineDataState NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong sequence READ sequence NOTIFY snapshotChanged)
    Q_PROPERTY(QString blackBoxState READ blackBoxState NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong blackBoxRecords READ blackBoxRecords NOTIFY snapshotChanged)
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
    Q_PROPERTY(QVariantList electricalLoads READ electricalLoads CONSTANT)
    Q_PROPERTY(QVariantList electricalBuses READ electricalBuses CONSTANT)
    Q_PROPERTY(QVariantList electricalSources READ electricalSources CONSTANT)
    Q_PROPERTY(QString electricalTestMode READ electricalTestMode NOTIFY electricalTestChanged)
    Q_PROPERTY(QVariantList maintenanceTasks READ maintenanceTasks NOTIFY maintenanceChanged)
    Q_PROPERTY(QString maintenanceStatus READ maintenanceStatus NOTIFY maintenanceChanged)
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
    Q_PROPERTY(QVariantMap aircraftSpeedProfile READ aircraftSpeedProfile CONSTANT)
    Q_PROPERTY(QString aircraftProfileName READ aircraftProfileName CONSTANT)
    Q_PROPERTY(QString aircraftProfileState READ aircraftProfileState CONSTANT)
    Q_PROPERTY(QString catalogVersion READ catalogVersion CONSTANT)
    Q_PROPERTY(QVariantList aircraftCatalog READ aircraftCatalog CONSTANT)
    Q_PROPERTY(QVariantList engineCatalog READ engineCatalog CONSTANT)
    Q_PROPERTY(QVariantMap stagedAircraftProfile READ stagedAircraftProfile NOTIFY setupChanged)
    Q_PROPERTY(QString stagingStatus READ stagingStatus NOTIFY setupChanged)
    Q_PROPERTY(QString setupMessage READ setupMessage NOTIFY setupChanged)
    Q_PROPERTY(QVariantMap stagedEngineProfile READ stagedEngineProfile NOTIFY setupChanged)
    Q_PROPERTY(QString engineStagingStatus READ engineStagingStatus NOTIFY setupChanged)
    Q_PROPERTY(QVariantList engineSensorMappings READ engineSensorMappings NOTIFY setupChanged)
    Q_PROPERTY(QString sensorMappingStatus READ sensorMappingStatus NOTIFY setupChanged)
    Q_PROPERTY(QString sensorTestSignal READ sensorTestSignal NOTIFY setupChanged)
    Q_PROPERTY(QString sensorTestStatus READ sensorTestStatus NOTIFY setupChanged)
    Q_PROPERTY(double sensorTestRaw READ sensorTestRaw NOTIFY setupChanged)
    Q_PROPERTY(double sensorTestValue READ sensorTestValue NOTIFY setupChanged)
    Q_PROPERTY(double sensorTestRateHz READ sensorTestRateHz NOTIFY setupChanged)
    Q_PROPERTY(int sensorTestAgeMs READ sensorTestAgeMs NOTIFY setupChanged)
    Q_PROPERTY(bool sensorTestFault READ sensorTestFault NOTIFY setupChanged)
    Q_PROPERTY(QString sensorTestDiagnostic READ sensorTestDiagnostic NOTIFY setupChanged)
    Q_PROPERTY(QString navigationDatabaseVersion READ navigationDatabaseVersion CONSTANT)
    Q_PROPERTY(QVariantList airportCatalog READ airportCatalog CONSTANT)
    Q_PROPERTY(QVariantMap selectedAirport READ selectedAirport NOTIFY navigationSetupChanged)
    Q_PROPERTY(QVariantList nearestAirports READ nearestAirports NOTIFY snapshotChanged)
    Q_PROPERTY(QString radionavigationDatabaseVersion READ radionavigationDatabaseVersion CONSTANT)
    Q_PROPERTY(QString radionavigationDatabaseStatus READ radionavigationDatabaseStatus CONSTANT)
    Q_PROPERTY(QVariantList radionavigationFacilities READ radionavigationFacilities CONSTANT)
    Q_PROPERTY(QString imageryState READ imageryState NOTIFY imageryChanged)
    Q_PROPERTY(bool imageryOfflineReady READ imageryOfflineReady NOTIFY imageryChanged)
    Q_PROPERTY(int imageryTileCount READ imageryTileCount NOTIFY imageryChanged)
    Q_PROPERTY(int imageryCoverageGaps READ imageryCoverageGaps NOTIFY imageryChanged)
    Q_PROPERTY(QString imageryAtlasSource READ imageryAtlasSource NOTIFY imageryChanged)
    Q_PROPERTY(bool systemSimulationActive READ systemSimulationActive NOTIFY simulationChanged)
    Q_PROPERTY(bool systemSimulationRunning READ systemSimulationRunning NOTIFY simulationChanged)
    Q_PROPERTY(QString systemSimulationScenario READ systemSimulationScenario NOTIFY simulationChanged)
    Q_PROPERTY(QString systemSimulationPhase READ systemSimulationPhase NOTIFY simulationChanged)
    Q_PROPERTY(double systemSimulationProgress READ systemSimulationProgress NOTIFY simulationChanged)
    Q_PROPERTY(double systemSimulationSpeed READ systemSimulationSpeed NOTIFY simulationChanged)
    Q_PROPERTY(QString systemSimulationLastEvent READ systemSimulationLastEvent NOTIFY simulationChanged)
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

public:
    explicit PfdController(QObject* parent = nullptr);

    [[nodiscard]] double roll() const;
    [[nodiscard]] double pitch() const;
    [[nodiscard]] double heading() const;
    [[nodiscard]] double airspeed() const;
    [[nodiscard]] double altitude() const;
    [[nodiscard]] double verticalSpeed() const;
    [[nodiscard]] double groundSpeed() const;
    [[nodiscard]] double latitude() const;
    [[nodiscard]] double longitude() const;
    [[nodiscard]] double commandedRoll() const;
    [[nodiscard]] double commandedPitch() const;
    [[nodiscard]] double selectedAltitude() const;
    [[nodiscard]] double selectedCourse() const;
    [[nodiscard]] double selectedAirspeed() const;
    [[nodiscard]] double cdiDeviation() const;
    [[nodiscard]] double glideSlope() const;
    [[nodiscard]] double airspeedTrend() const;
    [[nodiscard]] double altitudeTrend() const;
    [[nodiscard]] bool navTo() const;
    [[nodiscard]] bool flightDirectorActive() const;
    [[nodiscard]] QString annunciation() const;
    [[nodiscard]] int annunciationSeverity() const;
    [[nodiscard]] int healthLevel() const;
    [[nodiscard]] bool airspeedValid() const;
    [[nodiscard]] bool altitudeValid() const;
    [[nodiscard]] bool navigationValid() const;
    [[nodiscard]] QString faultMode() const;
    Q_INVOKABLE void cycleFault();
    [[nodiscard]] QString simulationRole() const;
    [[nodiscard]] QString attitudeSource() const;
    [[nodiscard]] bool showingSimulatedData() const;
    [[nodiscard]] QString dataSourceState() const;
    [[nodiscard]] QString sensorNodeState() const;
    [[nodiscard]] QString ddpLinkState() const { return sensorNodeState(); }
    [[nodiscard]] qulonglong ddpAccepted() const;
    [[nodiscard]] qulonglong ddpRejected() const;
    [[nodiscard]] qulonglong ddpSequenceGaps() const;
    [[nodiscard]] qulonglong ddpSocketErrors() const;
    [[nodiscard]] QString engineDataState() const;
    Q_INVOKABLE void cycleSimulationRole();
    [[nodiscard]] qulonglong sequence() const;
    [[nodiscard]] QString blackBoxState() const;
    [[nodiscard]] qulonglong blackBoxRecords() const;
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
    [[nodiscard]] QVariantList electricalLoads() const { return electrical_loads_; }
    [[nodiscard]] QVariantList electricalBuses() const { return electrical_buses_; }
    [[nodiscard]] QVariantList electricalSources() const { return electrical_sources_; }
    [[nodiscard]] QString electricalTestMode() const { return electrical_test_mode_; }
    [[nodiscard]] QVariantList maintenanceTasks() const { return maintenance_tasks_; }
    [[nodiscard]] QString maintenanceStatus() const;
    [[nodiscard]] bool landingGearInstalled() const { return landing_gear_installed_; }
    [[nodiscard]] bool retractableGear() const { return retractable_gear_; }
    [[nodiscard]] bool gearNoseLocked() const { return system_simulation_active_ ? system_frame_.airframe.gear_nose_locked : true; }
    [[nodiscard]] bool gearLeftLocked() const { return system_simulation_active_ ? system_frame_.airframe.gear_left_locked : true; }
    [[nodiscard]] bool gearRightLocked() const { return system_simulation_active_ ? system_frame_.airframe.gear_right_locked : true; }
    [[nodiscard]] QString gearState() const { return !landing_gear_installed_ ? QStringLiteral("NOT INSTALLED") : (gearNoseLocked() && gearLeftLocked() && gearRightLocked() ? QStringLiteral("DOWN & LOCKED") : QStringLiteral("UP / TRANSIT")); }
    [[nodiscard]] bool flapsInstalled() const { return flaps_installed_; }
    [[nodiscard]] QVariantList flapDetents() const { return flap_detents_; }
    [[nodiscard]] double flapAngle() const { return system_simulation_active_ ? system_frame_.airframe.flap_angle_deg : 15.0; }
    [[nodiscard]] double flapCommandedAngle() const { return system_simulation_active_ ? system_frame_.airframe.flap_command_deg : 15.0; }
    Q_INVOKABLE void cycleEngineFault();
    [[nodiscard]] QVariantMap aircraftSpeedProfile() const { return aircraft_speed_profile_; }
    [[nodiscard]] QString aircraftProfileName() const { return aircraft_profile_name_; }
    [[nodiscard]] QString aircraftProfileState() const { return aircraft_profile_state_; }
    [[nodiscard]] QString catalogVersion() const { return catalog_version_; }
    [[nodiscard]] QVariantList aircraftCatalog() const { return aircraft_catalog_; }
    [[nodiscard]] QVariantList engineCatalog() const { return engine_catalog_; }
    [[nodiscard]] QVariantMap stagedAircraftProfile() const { return staged_aircraft_profile_; }
    [[nodiscard]] QString stagingStatus() const { return staging_status_; }
    [[nodiscard]] QString setupMessage() const { return setup_message_; }
    Q_INVOKABLE bool stageAircraft(int catalogId);
    Q_INVOKABLE bool verifyStagedAircraft(const QString& pohReference, bool speedsConfirmed,
                                          bool weightsConfirmed, bool engineConfirmed);
    Q_INVOKABLE bool applyStagedAircraft();
    Q_INVOKABLE bool rollbackAircraftProfile();
    [[nodiscard]] QVariantMap stagedEngineProfile() const { return staged_engine_profile_; }
    [[nodiscard]] QString engineStagingStatus() const { return engine_staging_status_; }
    Q_INVOKABLE bool stageEngine(int catalogId);
    Q_INVOKABLE bool verifyStagedEngine(const QString& manualReference, int cylinders,
        double rpmMaximum, double oilPressureMinimum, double oilPressureMaximum,
        double oilTemperatureMaximum, double chtMaximum, double egtMaximum,
        double fuelPressureMinimum, double fuelPressureMaximum);
    [[nodiscard]] QVariantList engineSensorMappings() const { return engine_sensor_mappings_; }
    [[nodiscard]] QString sensorMappingStatus() const { return sensor_mapping_status_; }
    Q_INVOKABLE bool setEngineSensorMapping(const QString& signal, const QString& source,
                                            const QString& node, int channel);
    Q_INVOKABLE bool validateEngineSensorMappings();
    [[nodiscard]] QString sensorTestSignal() const { return sensor_test_signal_; }
    [[nodiscard]] QString sensorTestStatus() const { return sensor_test_status_; }
    [[nodiscard]] double sensorTestRaw() const { return sensor_test_raw_; }
    [[nodiscard]] double sensorTestValue() const { return sensor_test_value_; }
    [[nodiscard]] double sensorTestRateHz() const { return sensor_test_rate_hz_; }
    [[nodiscard]] int sensorTestAgeMs() const { return sensor_test_age_ms_; }
    [[nodiscard]] bool sensorTestFault() const { return sensor_test_fault_; }
    [[nodiscard]] QString sensorTestDiagnostic() const { return sensor_test_diagnostic_; }
    Q_INVOKABLE bool startEngineSensorTest(const QString& signal, bool benchSimulation);
    Q_INVOKABLE void stopEngineSensorTest();
    Q_INVOKABLE void setEngineSensorCalibration(double gain, double offset);
    Q_INVOKABLE void toggleEngineSensorTestFault();
    Q_INVOKABLE void cycleSensorTestDiagnostic();
    Q_INVOKABLE void cycleElectricalTestMode();
    Q_INVOKABLE void cycleMaintenanceInterval(int index);
    Q_INVOKABLE bool completeMaintenanceTask(int index);
    [[nodiscard]] QString navigationDatabaseVersion() const { return navigation_database_version_; }
    [[nodiscard]] QVariantList airportCatalog() const { return airport_catalog_; }
    [[nodiscard]] QVariantMap selectedAirport() const { return selected_airport_; }
    [[nodiscard]] QVariantList nearestAirports() const { return nearest_airports_; }
    Q_INVOKABLE bool selectAirport(const QString& icao);
    [[nodiscard]] QString radionavigationDatabaseVersion() const { return radionavigation_database_version_; }
    [[nodiscard]] QString radionavigationDatabaseStatus() const { return radionavigation_database_status_; }
    [[nodiscard]] QVariantList radionavigationFacilities() const { return radionavigation_facilities_; }
    [[nodiscard]] QString imageryState() const { return imagery_state_; }
    [[nodiscard]] bool imageryOfflineReady() const { return imagery_offline_ready_; }
    [[nodiscard]] int imageryTileCount() const { return imagery_tile_count_; }
    [[nodiscard]] int imageryCoverageGaps() const { return imagery_coverage_gaps_; }
    [[nodiscard]] QString imageryAtlasSource() const { return imagery_atlas_source_; }
    [[nodiscard]] bool systemSimulationActive() const { return system_simulation_active_; }
    [[nodiscard]] bool systemSimulationRunning() const { return system_simulation_active_ && system_simulation_running_; }
    [[nodiscard]] QString systemSimulationScenario() const;
    [[nodiscard]] QString systemSimulationPhase() const;
    [[nodiscard]] double systemSimulationProgress() const { return system_frame_.progress; }
    [[nodiscard]] double systemSimulationSpeed() const { return system_simulation_speed_; }
    [[nodiscard]] QString systemSimulationLastEvent() const { return system_simulation_last_event_; }
    Q_INVOKABLE bool startSystemSimulation(const QString& scenario);
    Q_INVOKABLE void toggleSystemSimulation();
    Q_INVOKABLE void stopSystemSimulation();
    Q_INVOKABLE void seekSystemSimulation(double progress);
    Q_INVOKABLE void setSystemSimulationSpeed(double speed);
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
    Q_INVOKABLE bool setUnitSystem(const QString& system);
    Q_INVOKABLE bool cycleSensorCalibration(int index);
    Q_INVOKABLE bool cycleSensorType(int index);
    Q_INVOKABLE bool cycleSensorInput(int index);
    Q_INVOKABLE bool captureSensorPoint(int index, const QString& point);
    Q_INVOKABLE bool setFuelReserveGallons(double gallons);
    Q_INVOKABLE bool setFuelOnBoardGallons(double gallons);
    Q_INVOKABLE bool captureFuelCalibrationPoint(const QString& tank);
    Q_INVOKABLE bool clearFuelCalibration(const QString& tank);
    Q_INVOKABLE bool requestCentralBlackBoxAction(const QString& action);
    Q_INVOKABLE void resetFuelUsed();

signals:
    void snapshotChanged();
    void setupChanged();
    void navigationSetupChanged();
    void imageryChanged();
    void simulationChanged();
    void unitsChanged();
    void sensorConfigurationChanged();
    void electricalTestChanged();
    void maintenanceChanged();
    void fuelComputerChanged();

private slots:
    void updateSnapshot();

private:
    void loadAircraftProfile();
    void loadCatalogIndex();
    void loadNavigationIndex();
    void loadRadionavigationIndex();
    void loadImageryStatus();
    void loadUnitSystem();
    void loadMaintenance();
    void loadFuelComputer();
    void persistFuelComputer();
    void updateFuelLeakMonitor(efis::MonoTime now);
    void loadFuelEventHistory();
    void recordFuelEvent(const QString& state);
    void loadCentralBlackBoxCache();
    void updateNearestAirports();
    [[nodiscard]] QString activeProfilePath() const;
    [[nodiscard]] QString stagingProfilePath() const;
    [[nodiscard]] QString backupDirectoryPath() const;
    void appendSetupAudit(const QString& action, const QString& result, const QString& detail) const;
    void applySimulationRole(efis::SimulationRole role);
    // Il display non conosce piu' una sorgente concreta: consuma cio' che il
    // router arbitra. Aggiungere il collegamento verso il nodo di ingestion
    // significhera' registrare una sorgente in piu', non toccare questa classe.
    std::shared_ptr<efis::SimulationSource> simulation_{
        std::make_shared<efis::SimulationSource>(42)};
    std::shared_ptr<efis::DdpUdpFlightSource> ddp_source_;
    efis::FlightSourceRouter router_{};
    std::shared_ptr<efis::EngineSimulationSource> engine_simulation_{
        std::make_shared<efis::EngineSimulationSource>(84)};
    efis::EngineSourceRouter engine_router_{};
    efis::FlightModel model_{};
    efis::MonoTime started_at_{efis::MonoClock::now()};
    efis::FlightSnapshot snapshot_{};
    efis::engine::EngineSnapshot engine_snapshot_{};
    efis::engine::RuntimeTracker runtime_tracker_{};
    efis::MonoTime last_runtime_update_{};
    QTimer timer_;
    double previous_airspeed_{};
    double airspeed_trend_{};
    bool trend_initialized_{};
    QString recorder_status_path_;
    QString black_box_state_{QStringLiteral("OFFLINE")};
    qulonglong black_box_records_{};
    efis::MonoTime last_recorder_check_{};
    QVariantMap aircraft_speed_profile_;
    QString aircraft_profile_name_{QStringLiteral("LSA SIM • BENCH")};
    QString aircraft_profile_state_{QStringLiteral("SIMULATION")};
    QString catalog_version_{QStringLiteral("UNAVAILABLE")};
    QVariantList aircraft_catalog_;
    QVariantList engine_catalog_;
    QString engine_profile_name_{QStringLiteral("UNCONFIGURED")};
    QString engine_serial_number_{QStringLiteral("TO BE ENTERED")};
    QVariantMap engine_limits_;
    QVariantMap engine_capabilities_;
    QVariantList engine_sensor_configuration_;
    QVariantList electrical_loads_;
    QVariantList electrical_buses_;
    QVariantList electrical_sources_;
    bool landing_gear_installed_{};
    bool retractable_gear_{};
    bool flaps_installed_{};
    QVariantList flap_detents_;
    QVariantMap staged_aircraft_profile_;
    QString staging_status_{QStringLiteral("NO_SELECTION")};
    QString setup_message_{QStringLiteral("Select an aircraft from the catalog")};
    QString latest_backup_path_;
    QVariantMap staged_engine_profile_;
    QString engine_staging_status_{QStringLiteral("NO_SELECTION")};
    QVariantList engine_sensor_mappings_;
    QString sensor_mapping_status_{QStringLiteral("NOT_CONFIGURED")};
    QString sensor_test_signal_;
    QString sensor_test_status_{QStringLiteral("IDLE")};
    double sensor_test_raw_{};
    double sensor_test_value_{};
    double sensor_test_rate_hz_{};
    int sensor_test_age_ms_{};
    bool sensor_test_fault_{};
    bool sensor_test_bench_{};
    double sensor_test_gain_{1.0};
    double sensor_test_offset_{};
    qulonglong sensor_test_ticks_{};
    QString sensor_test_diagnostic_{QStringLiteral("NORMAL")};
    QString electrical_test_mode_{QStringLiteral("NORMAL")};
    QVariantList maintenance_tasks_;
    QString navigation_database_version_{QStringLiteral("UNAVAILABLE")};
    QVariantList airport_catalog_;
    QVariantMap selected_airport_;
    QVariantList nearest_airports_;
    int navigation_update_ticks_{};
    QString radionavigation_database_version_{QStringLiteral("UNAVAILABLE")};
    QString radionavigation_database_status_{QStringLiteral("NO_DATA")};
    QVariantList radionavigation_facilities_;
    QString imagery_state_{QStringLiteral("NO PACKAGE")};
    bool imagery_offline_ready_{};
    int imagery_tile_count_{};
    int imagery_coverage_gaps_{};
    QString imagery_atlas_source_;
    efis::simulation::SystemSimulator system_simulator_{};
    efis::simulation::SimulationFrame system_frame_{};
    bool system_simulation_active_{};
    bool system_simulation_running_{};
    double system_simulation_speed_{1.0};
    double system_simulation_elapsed_ms_{};
    efis::MonoTime system_simulation_last_tick_{};
    QString system_simulation_last_event_{QStringLiteral("READY")};
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
    int calibration_revision_{};
};
