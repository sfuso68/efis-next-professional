#pragma once

#include <QByteArray>
#include <QImage>
#include <QQuickItem>
#include <QString>
#include <QHash>
#include <QPair>
#include <QFutureWatcher>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <vector>

class QSGTexture;

class SyntheticVisionItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(double roll READ roll WRITE setRoll NOTIFY attitudeChanged)
    Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY attitudeChanged)
    Q_PROPERTY(double heading READ heading WRITE setHeading NOTIFY attitudeChanged)
    Q_PROPERTY(double aircraftLatitude READ aircraftLatitude WRITE setAircraftLatitude NOTIFY positionChanged)
    Q_PROPERTY(double aircraftLongitude READ aircraftLongitude WRITE setAircraftLongitude NOTIFY positionChanged)
    Q_PROPERTY(double aircraftAltitude READ aircraftAltitude WRITE setAircraftAltitude NOTIFY positionChanged)
    Q_PROPERTY(double targetAirportLatitude READ targetAirportLatitude WRITE setTargetAirportLatitude NOTIFY targetChanged)
    Q_PROPERTY(double targetAirportLongitude READ targetAirportLongitude WRITE setTargetAirportLongitude NOTIFY targetChanged)
    Q_PROPERTY(double targetRunwayCourse READ targetRunwayCourse WRITE setTargetRunwayCourse NOTIFY targetChanged)
    Q_PROPERTY(bool realTerrain READ realTerrain NOTIFY terrainChanged)
    Q_PROPERTY(bool terrainLoading READ terrainLoading NOTIFY terrainChanged)
    Q_PROPERTY(QString terrainSource READ terrainSource NOTIFY terrainChanged)
    Q_PROPERTY(QString terrainTileKey READ terrainTileKey NOTIFY terrainChanged)
    Q_PROPERTY(int cachedTerrainTiles READ cachedTerrainTiles NOTIFY terrainChanged)
    Q_PROPERTY(QString terrainPackageId READ terrainPackageId NOTIFY terrainChanged)
    Q_PROPERTY(QString scenarioSource READ scenarioSource CONSTANT)

public:
    explicit SyntheticVisionItem(QQuickItem* parent = nullptr);
    [[nodiscard]] double roll() const { return roll_; }
    [[nodiscard]] double pitch() const { return pitch_; }
    [[nodiscard]] double heading() const { return heading_; }
    [[nodiscard]] double aircraftLatitude() const { return aircraft_latitude_; }
    [[nodiscard]] double aircraftLongitude() const { return aircraft_longitude_; }
    [[nodiscard]] double aircraftAltitude() const { return aircraft_altitude_; }
    [[nodiscard]] double targetAirportLatitude() const { return scenario_latitude_; }
    [[nodiscard]] double targetAirportLongitude() const { return scenario_longitude_; }
    [[nodiscard]] double targetRunwayCourse() const { return runway_course_; }
    [[nodiscard]] bool realTerrain() const { return terrain_real_; }
    [[nodiscard]] bool terrainLoading() const { return terrain_loading_; }
    [[nodiscard]] QString terrainSource() const { return terrain_source_; }
    [[nodiscard]] QString terrainTileKey() const { return active_tile_key_; }
    [[nodiscard]] int cachedTerrainTiles() const { return terrain_tile_cache_.size(); }
    [[nodiscard]] QString terrainPackageId() const { return terrain_package_id_; }
    [[nodiscard]] QString scenarioSource() const { return scenario_id_; }
    void setRoll(double value);
    void setPitch(double value);
    void setHeading(double value);
    void setAircraftLatitude(double value);
    void setAircraftLongitude(double value);
    void setAircraftAltitude(double value);
    void setTargetAirportLatitude(double value);
    void setTargetAirportLongitude(double value);
    void setTargetRunwayCourse(double value);

signals:
    void attitudeChanged();
    void positionChanged();
    void targetChanged();
    void terrainChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    void setValue(double& target, double value);
    [[nodiscard]] double terrainElevation(double latitude, double longitude) const;
    void loadTerrain();
    void updateTerrainRequests();
    void requestTerrainTile(const QString& key);
    void activateTerrainTile(const QString& key);
    [[nodiscard]] QString terrainTileKeyFor(double latitude, double longitude) const;
    [[nodiscard]] QStringList terrainTileCandidates(const QString& key) const;
    void touchTerrainTile(const QString& key);
    void loadScenario();
    void loadObstacles();
    [[nodiscard]] QString obstacleCellKey(double latitude, double longitude) const;
    double roll_{};
    double pitch_{};
    double heading_{};
    double aircraft_latitude_{45.6484};
    double aircraft_longitude_{12.1944};
    double aircraft_altitude_{3500.0};
    double scenario_latitude_{45.6484};
    double scenario_longitude_{12.1944};
    double runway_course_{253.5};
    QString scenario_id_{"TREVISO-LIPH FALLBACK"};
    QByteArray terrain_data_;
    QImage terrain_image_;
    QSGTexture* terrain_texture_{};
    QString terrain_source_{"PROCEDURAL FALLBACK"};
    bool terrain_real_{false};
    bool terrain_loading_{false};
    QHash<QString,QByteArray> terrain_tile_cache_;
    QStringList terrain_lru_;
    QSet<QString> terrain_pending_;
    QSet<QString> terrain_unavailable_;
    QString active_tile_key_;
    QString requested_tile_key_;
    int active_tile_lat_{};
    int active_tile_lon_{};
    QString terrain_root_;
    QString terrain_package_id_{QStringLiteral("LEGACY-ASSET")};
    int terrain_cache_capacity_{4};
    int prefetch_heading_sector_{-1};
    bool scenario_loaded_{false};
    struct Obstacle { double latitude; double longitude; double height_m; };
    std::vector<Obstacle> obstacles_;
    QHash<QString,std::vector<Obstacle>> obstacle_cells_;
};
