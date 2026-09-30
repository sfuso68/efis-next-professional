#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVariantList>

class WeatherService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString radarSource READ radarSource NOTIFY changed)
    Q_PROPERTY(QString radarState READ radarState NOTIFY changed)
    Q_PROPERTY(int radarAgeMinutes READ radarAgeMinutes NOTIFY changed)
    Q_PROPERTY(QString radarMessage READ radarMessage NOTIFY changed)
    Q_PROPERTY(QString radarProduct READ radarProduct NOTIFY changed)
    Q_PROPERTY(QString radarArea READ radarArea NOTIFY changed)
    Q_PROPERTY(bool simulation READ simulation NOTIFY changed)
    Q_PROPERTY(int simulationPhase READ simulationPhase NOTIFY changed)
    Q_PROPERTY(bool animationRunning READ animationRunning NOTIFY changed)
    Q_PROPERTY(QString scenario READ scenario NOTIFY changed)
    Q_PROPERTY(QVariantList sigmets READ sigmets NOTIFY changed)
    Q_PROPERTY(QString routeRisk READ routeRisk NOTIFY changed)
    Q_PROPERTY(int weatherEventCount READ weatherEventCount NOTIFY changed)
    Q_PROPERTY(QString advisory READ advisory NOTIFY changed)
    Q_PROPERTY(int suggestedHeading READ suggestedHeading NOTIFY changed)
    Q_PROPERTY(QVariantList alternates READ alternates NOTIFY changed)
    Q_PROPERTY(QString voiceAlert READ voiceAlert NOTIFY changed)
    Q_PROPERTY(bool alertAcknowledged READ alertAcknowledged NOTIFY changed)
    Q_PROPERTY(bool audioEnabled READ audioEnabled NOTIFY changed)
    Q_PROPERTY(QVariantList airportWeather READ airportWeather NOTIFY changed)
public:
    explicit WeatherService(QObject* parent = nullptr);
    ~WeatherService() override;
    [[nodiscard]] static QString classifyRadarAgeSeconds(qint64 age_seconds);
    [[nodiscard]] static QString normalizedRadarProduct(const QString& product);
    [[nodiscard]] static QString radarCacheFileName(const QString& product, int zoom, int x, int y);
    [[nodiscard]] QString radarSource() const { return radar_source_; }
    [[nodiscard]] QString radarState() const { return radar_state_; }
    [[nodiscard]] int radarAgeMinutes() const;
    [[nodiscard]] QString radarMessage() const { return radar_message_; }
    [[nodiscard]] QString radarProduct() const { return radar_product_; }
    [[nodiscard]] QString radarArea() const { return radar_area_; }
    [[nodiscard]] bool simulation() const { return simulation_; }
    [[nodiscard]] int simulationPhase() const { return simulation_phase_; }
    [[nodiscard]] bool animationRunning() const { return simulation_timer_.isActive(); }
    [[nodiscard]] QString scenario() const { return scenario_; }
    [[nodiscard]] QVariantList sigmets() const { return sigmets_; }
    [[nodiscard]] QString routeRisk() const { return route_risk_; }
    [[nodiscard]] int weatherEventCount() const { return weather_event_count_; }
    [[nodiscard]] QString advisory() const { return advisory_; }
    [[nodiscard]] int suggestedHeading() const { return suggested_heading_; }
    [[nodiscard]] QVariantList alternates() const { return alternates_; }
    [[nodiscard]] QString voiceAlert() const { return voice_alert_; }
    [[nodiscard]] bool alertAcknowledged() const { return alert_acknowledged_; }
    [[nodiscard]] bool audioEnabled() const { return audio_enabled_; }
    [[nodiscard]] QVariantList airportWeather() const { return airport_weather_; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void updatePosition(double latitude, double longitude);
    Q_INVOKABLE void cycleProduct();
    Q_INVOKABLE void setSimulation(bool enabled);
    Q_INVOKABLE void toggleAnimation();
    Q_INVOKABLE void nextScenario();
    Q_INVOKABLE void acknowledgeAlert();
    Q_INVOKABLE void toggleAudio();
signals:
    void changed();
private:
    struct SimulationFrameResult {
        QString path;
        QString product;
        quint64 phase{};
        qint64 worker_milliseconds{};
        bool saved{};
        QString error;
    };

    void requestRadar();
    void requestMetar();
    void requestTaf();
    void updateAgeState();
    bool loadCurrentRadarCache();
    void clearRadarFrame(const QString& message);
    [[nodiscard]] QString cachePath(const QString& name) const;
    QNetworkAccessManager network_;
    QTimer refresh_timer_;
    QTimer age_timer_;
    QString radar_source_;
    QString radar_state_{QStringLiteral("NO DATA")};
    QString radar_message_{QStringLiteral("Waiting for Radar-DPC")};
    QDateTime radar_time_;
    QVariantList airport_weather_;
    QString radar_product_{QStringLiteral("VMI")};
    QString radar_area_{QStringLiteral("GPS AREA")};
    double latitude_{45.648};
    double longitude_{12.194};
    int tile_x_{-1};
    int tile_y_{-1};
    bool simulation_{};
    int simulation_phase_{};
    QTimer simulation_timer_;
    QThread simulation_worker_thread_;
    QObject* simulation_worker_context_{};
    bool simulation_frame_in_flight_{};
    bool simulation_frame_refresh_pending_{};
    void generateSimulation();
    void finishSimulationFrame(const SimulationFrameResult& result);
    void updateScenario(int stage);
    void recordEvent(const QString& type, const QString& detail);
    QString scenario_{QStringLiteral("VFR")};
    QVariantList sigmets_;
    QString route_risk_{QStringLiteral("NONE")};
    int forced_scenario_{-1};
    int weather_event_count_{};
    QString advisory_{QStringLiteral("ROUTE CLEAR")};
    int suggested_heading_{254};
    QVariantList alternates_;
    QString voice_alert_;
    bool alert_acknowledged_{true};
    bool audio_enabled_{true};
};
