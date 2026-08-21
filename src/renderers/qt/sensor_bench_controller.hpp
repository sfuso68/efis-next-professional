#pragma once

#include "efis/bench/sensor_bench.hpp"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

// Espone il banco prova sensori alla UI del Setup Center.
//
// E' un adattatore di presentazione e nient'altro: non decide validita', non
// arbitra, non converte unita'. Tutto questo e' gia' stato deciso nel core, e
// rifarlo qui significherebbe avere due opinioni sullo stesso dato, con la
// certezza che prima o poi divergano. La ADR-003 tiene il renderer dipendente
// dal core e da nient'altro.
//
// Il banco resta in sola osservazione. `feedsFlightControl` e' dichiarato ed e'
// sempre falso: finche' ARSPD_USE resta a zero, cio' che si vede qui non entra
// in nessuna funzione di volo, e la UI deve dirlo invece di lasciarlo intendere.
class SensorBenchController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool receiving READ receiving NOTIFY changed)
    Q_PROPERTY(bool exhausted READ exhausted NOTIFY changed)
    Q_PROPERTY(bool feedsFlightControl READ feedsFlightControl CONSTANT)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY changed)
    Q_PROPERTY(QString defaultSource READ defaultSource CONSTANT)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)

    Q_PROPERTY(int gnssFixType READ gnssFixType NOTIFY changed)
    Q_PROPERTY(int gnssSatellites READ gnssSatellites NOTIFY changed)
    Q_PROPERTY(QString gnssSummary READ gnssSummary NOTIFY changed)
    Q_PROPERTY(bool airDataPresent READ airDataPresent NOTIFY changed)
    Q_PROPERTY(bool airDataHealthy READ airDataHealthy NOTIFY changed)
    Q_PROPERTY(QString airDataSummary READ airDataSummary NOTIFY changed)

    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantList nodes READ nodes NOTIFY changed)
    Q_PROPERTY(QVariantMap link READ link NOTIFY changed)

public:
    explicit SensorBenchController(QObject* parent = nullptr);
    ~SensorBenchController() override;

    // Avvia il banco da una registrazione. Restituisce falso e valorizza
    // `message` se la sorgente non e' leggibile: un banco che non parte deve
    // dirlo, non restare muto con le righe vuote.
    Q_INVOKABLE bool startReplay(const QString& path);
    Q_INVOKABLE void stop();

    [[nodiscard]] bool active() const noexcept { return bench_ != nullptr; }
    [[nodiscard]] bool receiving() const noexcept { return view_.receiving(); }
    [[nodiscard]] bool exhausted() const noexcept { return exhausted_; }
    [[nodiscard]] bool feedsFlightControl() const noexcept {
        return view_.feeds_flight_control;
    }
    [[nodiscard]] QString sourceName() const { return source_name_; }
    [[nodiscard]] QString defaultSource() const;
    [[nodiscard]] QString statusText() const {
        return QString::fromStdString(view_.status_text);
    }
    [[nodiscard]] QString message() const { return message_; }

    [[nodiscard]] int gnssFixType() const noexcept {
        return static_cast<int>(view_.gnss_fix_type);
    }
    [[nodiscard]] int gnssSatellites() const noexcept {
        return static_cast<int>(view_.gnss_satellites);
    }
    [[nodiscard]] QString gnssSummary() const;
    [[nodiscard]] bool airDataPresent() const noexcept { return view_.air_data_present; }
    [[nodiscard]] bool airDataHealthy() const noexcept { return view_.air_data_healthy; }
    [[nodiscard]] QString airDataSummary() const;

    [[nodiscard]] QVariantList rows() const;
    [[nodiscard]] QVariantList nodes() const;
    [[nodiscard]] QVariantMap link() const;

signals:
    void changed();

private:
    void pump();

    std::unique_ptr<efis::bench::SensorBench> bench_;
    efis::bench::BenchView view_{};
    QTimer timer_;
    QString source_name_;
    QString message_;
    bool exhausted_{};
};
