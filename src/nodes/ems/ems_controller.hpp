#pragma once

// Controllore del nodo EMS.
//
// Espone alla pagina motore esattamente le grandezze del motore, e nulla piu'.
// Non e' una riduzione di PfdController: e' il confine di processo previsto
// dalla ADR-002 reso reale. Un nodo che non conosce assetto, navigazione,
// terreno o meteo non puo' guastarsi per una loro ragione, e la superficie che
// pubblica e' la stessa che la pagina EMS consuma dentro il PFD completo.

#include "efis/providers/engine_simulation_provider.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <chrono>

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

    Q_INVOKABLE void cycleEngineFault();

signals:
    void snapshotChanged();

private:
    void updateSnapshot();
    void updateRecorderStatus(efis::MonoTime now);
    void loadAircraftProfile();

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
};
