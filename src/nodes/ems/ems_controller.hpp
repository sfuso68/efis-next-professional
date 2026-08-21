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
    Q_PROPERTY(QString blackBoxState READ blackBoxState NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong blackBoxRecords READ blackBoxRecords NOTIFY snapshotChanged)
    Q_PROPERTY(qulonglong sequence READ sequence NOTIFY snapshotChanged)

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
    [[nodiscard]] QString blackBoxState() const { return black_box_state_; }
    [[nodiscard]] qulonglong blackBoxRecords() const { return black_box_records_; }
    [[nodiscard]] qulonglong sequence() const {
        return static_cast<qulonglong>(engine_snapshot_.sequence);
    }

    Q_INVOKABLE void cycleEngineFault();

signals:
    void snapshotChanged();

private:
    void updateSnapshot();
    void updateRecorderStatus(efis::MonoTime now);

    QTimer timer_;
    efis::EngineSimulationProvider engine_provider_{84};
    efis::engine::EngineSnapshot engine_snapshot_{};
    efis::MonoTime started_at_{efis::MonoClock::now()};
    efis::MonoTime last_recorder_check_{};
    QString recorder_status_path_;
    QString black_box_state_{QStringLiteral("OFFLINE")};
    qulonglong black_box_records_{};
};
