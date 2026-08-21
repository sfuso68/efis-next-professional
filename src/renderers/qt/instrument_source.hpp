#pragma once

#include "efis/core/flight_model.hpp"
#include "efis/core/flight_source.hpp"
#include "efis/providers/simulation_source.hpp"

#include <QObject>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

#include <memory>

// Alimenta la pagina analogica con lo stato di volo.
//
// Espone valore e validita' separatamente per ogni grandezza, e non un solo
// numero: e' la distinzione su cui poggia tutto il resto, perche' uno strumento
// deve poter mostrare la bandierina invece di una lancetta e non puo' farlo se
// riceve soltanto un numero.
//
// Legge attraverso il router delle sorgenti e il modello di volo, cioe' dalla
// stessa catena del PFD digitale: la simulazione entra come una sorgente fra le
// altre e lo stato arriva gia' arbitrato. Collegare qui il Pixhawk significa
// registrare una seconda sorgente nel router, e nient'altro: e' il motivo per
// cui gli strumenti non parlano con i provider.
class InstrumentSource : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(double airspeed READ airspeed NOTIFY updated)
    Q_PROPERTY(int airspeedValidity READ airspeedValidity NOTIFY updated)
    Q_PROPERTY(QVariantList airspeedBands READ airspeedBands CONSTANT)

    Q_PROPERTY(double altitude READ altitude NOTIFY updated)
    Q_PROPERTY(int altitudeValidity READ altitudeValidity NOTIFY updated)
    Q_PROPERTY(QString baroSetting READ baroSetting NOTIFY updated)

    Q_PROPERTY(double verticalSpeed READ verticalSpeed NOTIFY updated)
    Q_PROPERTY(int verticalSpeedValidity READ verticalSpeedValidity NOTIFY updated)

    Q_PROPERTY(double roll READ roll NOTIFY updated)
    Q_PROPERTY(int rollValidity READ rollValidity NOTIFY updated)
    Q_PROPERTY(double pitch READ pitch NOTIFY updated)
    Q_PROPERTY(int pitchValidity READ pitchValidity NOTIFY updated)

    Q_PROPERTY(double heading READ heading NOTIFY updated)
    Q_PROPERTY(int headingValidity READ headingValidity NOTIFY updated)
    Q_PROPERTY(double selectedCourse READ selectedCourse NOTIFY updated)
    Q_PROPERTY(int selectedCourseValidity READ selectedCourseValidity NOTIFY updated)

    Q_PROPERTY(double turnRate READ turnRate NOTIFY updated)
    Q_PROPERTY(int turnRateValidity READ turnRateValidity NOTIFY updated)
    Q_PROPERTY(double slip READ slip NOTIFY updated)
    Q_PROPERTY(int slipValidity READ slipValidity NOTIFY updated)

    Q_PROPERTY(bool simulated READ simulated NOTIFY updated)

    // Provenienza dei limiti del profilo attivo, secondo efis::LimitsProvenance.
    Q_PROPERTY(int limitsProvenance READ limitsProvenance CONSTANT)

public:
    explicit InstrumentSource(QObject* parent = nullptr);

    // Toglie una grandezza per mostrare come si comporta lo strumento quando
    // manca. Serve alla verifica sul display vero: la bandierina va guardata,
    // non dedotta da una prova.
    Q_INVOKABLE void injectFailure(const QString& signal);

    double airspeed() const;
    int airspeedValidity() const;
    QVariantList airspeedBands() const;
    double altitude() const;
    int altitudeValidity() const;
    QString baroSetting() const;
    double verticalSpeed() const;
    int verticalSpeedValidity() const;
    double roll() const;
    int rollValidity() const;
    double pitch() const;
    int pitchValidity() const;
    double heading() const;
    int headingValidity() const;
    double selectedCourse() const;
    int selectedCourseValidity() const;
    double turnRate() const;
    int turnRateValidity() const;
    double slip() const;
    int slipValidity() const;
    bool simulated() const;
    int limitsProvenance() const;

signals:
    void updated();

private:
    void step();

    std::shared_ptr<efis::SimulationSource> simulation_{
        std::make_shared<efis::SimulationSource>(42)};
    efis::FlightSourceRouter router_{};
    efis::FlightModel model_;
    efis::FlightSnapshot snapshot_;
    efis::MonoTime started_{efis::MonoClock::now()};
    QTimer timer_;
    QString failed_signal_;
};
