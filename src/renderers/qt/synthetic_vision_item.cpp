#include "synthetic_vision_item.hpp"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent/QtConcurrentRun>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGTexture>
#include <QSGTextureMaterial>
#include <QSGVertexColorMaterial>
#include <QQuickWindow>

#include <cmath>
#include <array>
#include <vector>

namespace {
using Points = std::vector<QPointF>;

QSGGeometryNode* geometry_node(const Points& points, QColor color,
                               QSGGeometry::DrawingMode mode, float width = 1.0F) {
    auto* geometry = new QSGGeometry{QSGGeometry::defaultAttributes_Point2D(),
                                     static_cast<int>(points.size())};
    geometry->setDrawingMode(mode);
    geometry->setLineWidth(width);
    auto* vertices = geometry->vertexDataAsPoint2D();
    for (std::size_t i = 0; i < points.size(); ++i)
        vertices[i].set(static_cast<float>(points[i].x()), static_cast<float>(points[i].y()));
    auto* material = new QSGFlatColorMaterial;
    material->setColor(color);
    auto* node = new QSGGeometryNode;
    node->setGeometry(geometry); node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material); node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

struct TerrainVertex { QPointF point; QColor color; };
QSGGeometryNode* terrain_mesh(const std::vector<TerrainVertex>& vertices) {
    auto* geometry = new QSGGeometry{QSGGeometry::defaultAttributes_ColoredPoint2D(),
                                     static_cast<int>(vertices.size())};
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* output = geometry->vertexDataAsColoredPoint2D();
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const auto& v = vertices[i];
        output[i].set(static_cast<float>(v.point.x()), static_cast<float>(v.point.y()),
                      static_cast<uchar>(v.color.red()), static_cast<uchar>(v.color.green()),
                      static_cast<uchar>(v.color.blue()), static_cast<uchar>(v.color.alpha()));
    }
    auto* node = new QSGGeometryNode;
    node->setGeometry(geometry); node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(new QSGVertexColorMaterial); node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

struct TexturedTerrainVertex { QPointF point; QPointF uv; };
QSGGeometryNode* textured_terrain_mesh(const std::vector<TexturedTerrainVertex>& vertices,
                                       QSGTexture* texture) {
    auto* geometry = new QSGGeometry{QSGGeometry::defaultAttributes_TexturedPoint2D(),
                                     static_cast<int>(vertices.size())};
    geometry->setDrawingMode(QSGGeometry::DrawTriangles);
    auto* output = geometry->vertexDataAsTexturedPoint2D();
    for (std::size_t i=0;i<vertices.size();++i)
        output[i].set(vertices[i].point.x(),vertices[i].point.y(),vertices[i].uv.x(),vertices[i].uv.y());
    auto* material = new QSGTextureMaterial;
    material->setTexture(texture);
    material->setFiltering(QSGTexture::Linear);
    auto* node = new QSGGeometryNode;
    node->setGeometry(geometry); node->setFlag(QSGNode::OwnsGeometry);
    node->setMaterial(material); node->setFlag(QSGNode::OwnsMaterial);
    return node;
}

QPointF rotate_point(QPointF point, QPointF center, double radians) {
    const double x = point.x() - center.x();
    const double y = point.y() - center.y();
    return {center.x() + x * std::cos(radians) - y * std::sin(radians),
            center.y() + x * std::sin(radians) + y * std::cos(radians)};
}

Points rotated(Points points, QPointF center, double radians) {
    for (auto& point : points) point = rotate_point(point, center, radians);
    return points;
}
}

SyntheticVisionItem::SyntheticVisionItem(QQuickItem* parent) : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    loadScenario();
    loadTerrain();
    loadObstacles();
}

void SyntheticVisionItem::loadObstacles() {
    if (!qEnvironmentVariableIsSet("EFIS_EXPERIMENTAL_OBSTACLES")) return;
    const QString app=QCoreApplication::applicationDirPath();
    const QStringList candidates{
        QDir::current().filePath("assets/obstacles/veneto/liph-open-obstacles.json"),
        QDir(app).filePath("assets/obstacles/veneto/liph-open-obstacles.json"),
        QDir(app).filePath("../../assets/obstacles/veneto/liph-open-obstacles.json")};
    for(const auto& path:candidates){
        QFile file(path); if(!file.open(QIODevice::ReadOnly))continue;
        const auto document=QJsonDocument::fromJson(file.readAll()); if(!document.isObject())continue;
        const auto elements=document.object().value("elements").toArray();
        obstacles_.reserve(elements.size());
        for(const auto& value:elements){
            const auto object=value.toObject();
            if(!object.contains("lat")||!object.contains("lon"))continue;
            const auto tags=object.value("tags").toObject();
            bool ok=false; double height=tags.value("height").toString().split(' ').first().toDouble(&ok);
            if(!ok)height=tags.value("power").toString()=="tower"?25.0:40.0;
            const Obstacle obstacle{object.value("lat").toDouble(),object.value("lon").toDouble(),std::clamp(height,8.0,250.0)};
            obstacles_.push_back(obstacle);
            obstacle_cells_[obstacleCellKey(obstacle.latitude,obstacle.longitude)].push_back(obstacle);
        }
        return;
    }
}

QString SyntheticVisionItem::obstacleCellKey(double latitude, double longitude) const {
    return QStringLiteral("%1:%2").arg(static_cast<int>(std::floor(latitude*10.0)))
                                     .arg(static_cast<int>(std::floor(longitude*10.0)));
}

void SyntheticVisionItem::loadScenario() {
    const QString requested = qEnvironmentVariable("EFIS_SCENARIO_FILE");
    const QString app = QCoreApplication::applicationDirPath();
    const QStringList candidates = requested.isEmpty()
        ? QStringList{QDir::current().filePath("config/scenarios/dolomites-development.json"),
                      QDir(app).filePath("config/scenarios/dolomites-development.json"),
                      QDir(app).filePath("../../config/scenarios/dolomites-development.json"),
                      QDir::current().filePath("config/scenarios/treviso-liph.json")}
        : QStringList{requested};
    for (const auto& path : candidates) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) continue;
        const auto root = document.object();
        const auto reference = root.value("reference").toObject();
        const auto runway = root.value("runway").toObject();
        scenario_latitude_ = reference.value("latitude_deg").toDouble(scenario_latitude_);
        scenario_longitude_ = reference.value("longitude_deg").toDouble(scenario_longitude_);
        runway_course_ = runway.value("true_course_25_deg").toDouble(runway_course_);
        scenario_id_ = root.value("id").toString("DOLOMITES") + " / " + root.value("status").toString("SIM");
        // This flag controls runway rendering. A terrain-only scenario must not
        // invent an airport or project a fictitious runway into the scene.
        scenario_loaded_ = !runway.isEmpty();
        return;
    }
}

void SyntheticVisionItem::loadTerrain() {
    const QString app = QCoreApplication::applicationDirPath();
    terrain_root_=qEnvironmentVariable("EFIS_TERRAIN_ROOT");
    if(terrain_root_.isEmpty()) {
        const QString configured=qEnvironmentVariable("EFIS_TERRAIN_ACTIVE");
        const QStringList manifests=configured.isEmpty()?QStringList{
            QDir::current().filePath("data/terrain/active-package.json"),
            QDir(app).filePath("../../data/terrain/active-package.json")}:QStringList{configured};
        for(const auto& path:manifests){
            QFile file(path); if(!file.open(QIODevice::ReadOnly))continue;
            const auto document=QJsonDocument::fromJson(file.readAll()); if(!document.isObject())continue;
            const auto root=document.object(); const QString tiles=root.value("tiles_path").toString();
            if(!QDir(tiles).exists())continue;
            terrain_root_=tiles; terrain_package_id_=root.value("package_id").toString(QStringLiteral("UNKNOWN"));
            qInfo().noquote() << "event=terrain_package_selected package=" << terrain_package_id_
                              << "tiles_path=" << terrain_root_; break;
        }
    }
    if(terrain_root_.isEmpty()) terrain_root_=QDir(app).filePath("../../assets/terrain/tiles");
    updateTerrainRequests();
    // Orthophoto projection remains opt-in until the full 3D camera pipeline is
    // used for terrain, runway and obstacles. A 2D warp is visually misleading.
    if (!qEnvironmentVariableIsSet("EFIS_EXPERIMENTAL_ORTHO")) return;
    const QStringList imageryCandidates{
        QDir::current().filePath("assets/terrain/veneto/imagery/liph-ortho-2018.jpg"),
        QDir(app).filePath("assets/terrain/veneto/imagery/liph-ortho-2018.jpg"),
        QDir(app).filePath("../../assets/terrain/veneto/imagery/liph-ortho-2018.jpg")};
    for (const auto& path : imageryCandidates) {
        QImage image(path);
        if (image.isNull()) continue;
        terrain_image_ = image.convertToFormat(QImage::Format_RGBA8888);
        terrain_source_ = "VENETO ORTHO EXPERIMENTAL";
        break;
    }
}

QString SyntheticVisionItem::terrainTileKeyFor(double latitude,double longitude) const {
    const int lat=static_cast<int>(std::floor(latitude));
    const int lon=static_cast<int>(std::floor(longitude));
    return QStringLiteral("%1%2%3%4").arg(lat>=0?QLatin1Char('N'):QLatin1Char('S'))
        .arg(std::abs(lat),2,10,QLatin1Char('0')).arg(lon>=0?QLatin1Char('E'):QLatin1Char('W'))
        .arg(std::abs(lon),3,10,QLatin1Char('0'));
}

QStringList SyntheticVisionItem::terrainTileCandidates(const QString& key) const {
    QStringList candidates{QDir(terrain_root_).filePath(key+QStringLiteral(".hgt"))};
    const QString requested=qEnvironmentVariable("EFIS_TERRAIN_HGT");
    if(!requested.isEmpty()&&QFileInfo(requested).baseName()==key)candidates.prepend(requested);
    const bool authoritativePackage = terrain_package_id_ != QStringLiteral("LEGACY-ASSET")
        || qEnvironmentVariableIsSet("EFIS_TERRAIN_ROOT")
        || qEnvironmentVariableIsSet("EFIS_TERRAIN_ACTIVE");
    if(!authoritativePackage) {
        const QString app=QCoreApplication::applicationDirPath();
        candidates << QDir::current().filePath(QStringLiteral("assets/terrain/veneto/")+key+QStringLiteral(".hgt"))
                   << QDir(app).filePath(QStringLiteral("../../assets/terrain/veneto/")+key+QStringLiteral(".hgt"));
    }
    candidates.removeDuplicates(); return candidates;
}

void SyntheticVisionItem::touchTerrainTile(const QString& key) {
    terrain_lru_.removeAll(key); terrain_lru_.prepend(key);
    while(terrain_lru_.size()>terrain_cache_capacity_){
        const QString evicted=terrain_lru_.takeLast();
        if(evicted!=active_tile_key_)terrain_tile_cache_.remove(evicted);
    }
}

void SyntheticVisionItem::requestTerrainTile(const QString& key) {
    if(key.isEmpty()||terrain_tile_cache_.contains(key)||terrain_pending_.contains(key)||terrain_unavailable_.contains(key))return;
    terrain_pending_.insert(key); terrain_loading_=true; emit terrainChanged();
    auto* watcher=new QFutureWatcher<QVariantMap>(this);
    connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher,key]{
        const QVariantMap result=watcher->result(); watcher->deleteLater(); terrain_pending_.remove(key);
        const QByteArray data=result.value("data").toByteArray();
        if(data.size()==3601LL*3601LL*2LL){
            terrain_tile_cache_.insert(key,data); touchTerrainTile(key);
            qInfo().noquote() << "event=terrain_tile_loaded key=" << key
                              << "cache=" << terrain_tile_cache_.size();
            if(key==requested_tile_key_)activateTerrainTile(key);
        }else {
            terrain_unavailable_.insert(key);
            qWarning().noquote() << "event=terrain_tile_unavailable key=" << key;
        }
        terrain_loading_=!terrain_pending_.isEmpty();
        if(!terrain_real_&&!terrain_loading_)terrain_source_=QStringLiteral("PROCEDURAL FALLBACK / TILE UNAVAILABLE");
        emit terrainChanged(); update();
    });
    const QStringList candidates=terrainTileCandidates(key);
    watcher->setFuture(QtConcurrent::run([candidates,key]{
        constexpr qsizetype expected=3601LL*3601LL*2LL;
        for(const auto& path:candidates){ QFile file(path); if(file.open(QIODevice::ReadOnly)&&file.size()==expected)return QVariantMap{{"key",key},{"path",path},{"data",file.readAll()}}; }
        return QVariantMap{{"key",key}};
    }));
}

void SyntheticVisionItem::activateTerrainTile(const QString& key) {
    if(!terrain_tile_cache_.contains(key))return;
    active_tile_key_=key; terrain_data_=terrain_tile_cache_.value(key); terrain_real_=true;
    active_tile_lat_=static_cast<int>(std::floor(aircraft_latitude_));
    active_tile_lon_=static_cast<int>(std::floor(aircraft_longitude_));
    touchTerrainTile(key);
    terrain_source_=key+QStringLiteral(" HGT / ASYNC LRU");
    qInfo().noquote() << "event=terrain_tile_active key=" << key
                      << "cache=" << terrain_tile_cache_.size();
    emit terrainChanged(); update();
}

void SyntheticVisionItem::updateTerrainRequests() {
    requested_tile_key_=terrainTileKeyFor(aircraft_latitude_,aircraft_longitude_);
    if(terrain_tile_cache_.contains(requested_tile_key_)) {
        if(active_tile_key_!=requested_tile_key_)activateTerrainTile(requested_tile_key_);
    }
    else {
        if(requested_tile_key_!=active_tile_key_){ terrain_real_=false; terrain_source_=QStringLiteral("TILE TRANSITION / PROCEDURAL FALLBACK"); }
        requestTerrainTile(requested_tile_key_);
    }
    const double bearing=heading_*3.141592653589793/180.0;
    constexpr double prefetchDistance=25000.0;
    const double futureLat=aircraft_latitude_+std::cos(bearing)*prefetchDistance/111320.0;
    const double futureLon=aircraft_longitude_+std::sin(bearing)*prefetchDistance/(111320.0*std::cos(aircraft_latitude_*3.141592653589793/180.0));
    requestTerrainTile(terrainTileKeyFor(futureLat,futureLon));
}

double SyntheticVisionItem::terrainElevation(double latitude, double longitude) const {
    const bool active=latitude>=active_tile_lat_&&latitude<=active_tile_lat_+1.0&&longitude>=active_tile_lon_&&longitude<=active_tile_lon_+1.0;
    const QByteArray* data=&terrain_data_;
    QByteArray adjacent;
    if(!active){ adjacent=terrain_tile_cache_.value(terrainTileKeyFor(latitude,longitude)); data=&adjacent; }
    if(data->size()!=3601LL*3601LL*2LL)return 0.0;
    const double baseLat=std::floor(latitude),baseLon=std::floor(longitude);
    const int row = std::clamp(static_cast<int>(std::lround((baseLat+1.0-latitude)*3600.0)), 0, 3600);
    const int column = std::clamp(static_cast<int>(std::lround((longitude-baseLon)*3600.0)), 0, 3600);
    const qsizetype offset = (static_cast<qsizetype>(row)*3601 + column)*2;
    const auto high = static_cast<unsigned char>(data->at(offset));
    const auto low = static_cast<unsigned char>(data->at(offset+1));
    const auto elevation = static_cast<qint16>((high << 8) | low);
    return elevation == static_cast<qint16>(-32768) ? 0.0 : elevation;
}

void SyntheticVisionItem::setValue(double& target, double value) {
    if (qFuzzyCompare(target, value)) return;
    target = value; emit attitudeChanged(); update();
}
void SyntheticVisionItem::setRoll(double value) { setValue(roll_, value); }
void SyntheticVisionItem::setPitch(double value) { setValue(pitch_, value); }
void SyntheticVisionItem::setHeading(double value) {
    if(qFuzzyCompare(heading_,value))return; heading_=value; emit attitudeChanged(); update();
    const int sector=static_cast<int>(std::floor((std::fmod(heading_+360.0,360.0))/15.0));
    if(sector!=prefetch_heading_sector_){prefetch_heading_sector_=sector;updateTerrainRequests();}
}
void SyntheticVisionItem::setAircraftLatitude(double value) {
    if (qFuzzyCompare(aircraft_latitude_, value)) return;
    aircraft_latitude_ = value; emit positionChanged(); update();
    if(terrainTileKeyFor(aircraft_latitude_,aircraft_longitude_)!=requested_tile_key_)updateTerrainRequests();
}
void SyntheticVisionItem::setAircraftLongitude(double value) {
    if (qFuzzyCompare(aircraft_longitude_, value)) return;
    aircraft_longitude_ = value; emit positionChanged(); update();
    if(terrainTileKeyFor(aircraft_latitude_,aircraft_longitude_)!=requested_tile_key_)updateTerrainRequests();
}
void SyntheticVisionItem::setAircraftAltitude(double value) {
    if(qFuzzyCompare(aircraft_altitude_,value))return;
    aircraft_altitude_=value; emit positionChanged(); update();
}
void SyntheticVisionItem::setTargetAirportLatitude(double value) {
    if (qFuzzyCompare(scenario_latitude_,value)) return; scenario_latitude_=value; update(); emit targetChanged();
}
void SyntheticVisionItem::setTargetAirportLongitude(double value) {
    if (qFuzzyCompare(scenario_longitude_,value)) return; scenario_longitude_=value; update(); emit targetChanged();
}
void SyntheticVisionItem::setTargetRunwayCourse(double value) {
    if (qFuzzyCompare(runway_course_,value)) return; runway_course_=value; update(); emit targetChanged();
}

QSGNode* SyntheticVisionItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    delete oldNode;
    auto* root = new QSGNode;
    const double w = width(), h = height();
    if (w <= 0 || h <= 0) return root;
    const QPointF center{w * 0.5, h * 0.46};
    const double horizon = center.y() + pitch_ * h / 42.0;
    const double angle = -roll_ * 3.141592653589793 / 180.0;
    if (!terrain_texture_ && !terrain_image_.isNull() && window())
        terrain_texture_ = window()->createTextureFromImage(terrain_image_);

    // One GPU-interpolated quad gives a continuous sky gradient without banding.
    const TerrainVertex skyTopLeft{{0,0},QColor{"#061B37"}};
    const TerrainVertex skyTopRight{{w,0},QColor{"#061B37"}};
    const TerrainVertex skyBottomLeft{{0,horizon},QColor{"#55A9D2"}};
    const TerrainVertex skyBottomRight{{w,horizon},QColor{"#55A9D2"}};
    root->appendChildNode(terrain_mesh({skyTopLeft,skyBottomLeft,skyTopRight,
                                       skyTopRight,skyBottomLeft,skyBottomRight}));

    Points ground{{-w,horizon},{2*w,horizon},{-w,2*h},{2*w,2*h}};
    root->appendChildNode(geometry_node(rotated(ground, center, angle), QColor{"#384B35"}, QSGGeometry::DrawTriangleStrip));

    // Layered silhouettes are retained only for the explicit procedural fallback.
    Points ridge1{{-0.1*w,horizon+25},{0.06*w,horizon-18},{0.18*w,horizon+8},{0.31*w,horizon-42},
                  {0.43*w,horizon-6},{0.57*w,horizon-53},{0.69*w,horizon-12},{0.82*w,horizon-39},
                  {1.08*w,horizon+20},{1.08*w,h},{-0.1*w,h}};
    if (!terrain_real_) root->appendChildNode(geometry_node(rotated(ridge1, center, angle), QColor{"#263D35"}, QSGGeometry::DrawTriangleFan));
    Points ridge2{{-0.1*w,horizon+72},{0.13*w,horizon+19},{0.28*w,horizon+49},{0.46*w,horizon+3},
                  {0.61*w,horizon+44},{0.78*w,horizon+8},{1.1*w,horizon+66},{1.1*w,h},{-0.1*w,h}};
    if (!terrain_real_) root->appendChildNode(geometry_node(rotated(ridge2, center, angle), QColor{"#43523A"}, QSGGeometry::DrawTriangleFan));

    // Terrain Camera 3D V2. DEM samples are converted to local ENU coordinates,
    // transformed into the aircraft camera and projected with a pinhole model.
    // QSG still rasterises the resulting triangles efficiently on the embedded GPU.
    const double meshVanX = w * 0.50;
    const double meshVanY = h * 0.46;
    // A regular high-density angular grid avoids the large, visible facets of
    // the first prototype. Vertices are cached below, so this is also cheaper
    // than evaluating every shared corner four times per frame.
    const int columns = aircraft_altitude_ < 5000.0 ? 112 : 96;
    const int rows = aircraft_altitude_ < 5000.0 ? 84 : 72;
    std::vector<TerrainVertex> mesh;
    mesh.reserve(columns * rows * 6);
    const double referenceElevation = terrain_real_ ? terrainElevation(aircraft_latitude_, aircraft_longitude_) : 0.0;
    const double aircraftMsl = aircraft_altitude_ * .3048;
    const double headingRadians = heading_ * 3.141592653589793 / 180.0;
    const double pitchRadians = pitch_ * 3.141592653589793 / 180.0;
    const double verticalFov = 48.0 * 3.141592653589793 / 180.0;
    const double focalLength = h / (2.0 * std::tan(verticalFov * .5));
    const double horizontalFov = 2.0 * std::atan((w / h) * std::tan(verticalFov * .5));
    struct MeshSample { QPointF point; double elevation; double terrain_msl; double latitude; double longitude; double shade; };
    auto meshSample = [&](int column, int row) {
        const double depth = static_cast<double>(row) / rows;
        const double xFactor = (static_cast<double>(column) / columns - 0.5) * 2.0;
        double elevation = 0.0, terrainMsl=0.0, shade=.58;
        double latitude=aircraft_latitude_, longitude=aircraft_longitude_;
        QPointF projected;
        if (terrain_real_) {
            const double nearRange = 450.0;
            const double farRange = aircraft_altitude_ < 5000.0 ? 35000.0 : 60000.0;
            const double distance = farRange * std::pow(nearRange / farRange, depth);
            const double lateral = xFactor * distance * std::tan(horizontalFov * .55);
            const double north = std::cos(headingRadians)*distance - std::sin(headingRadians)*lateral;
            const double east = std::sin(headingRadians)*distance + std::cos(headingRadians)*lateral;
            latitude = aircraft_latitude_ + north/111320.0;
            longitude = aircraft_longitude_ + east/(111320.0*std::cos(aircraft_latitude_*3.141592653589793/180.0));
            terrainMsl = terrainElevation(latitude, longitude);
            elevation = terrainMsl - referenceElevation;
            const double up = terrainMsl - aircraftMsl;
            const double cameraRight = lateral;
            const double cameraUp = up*std::cos(pitchRadians) - distance*std::sin(pitchRadians);
            const double cameraForward = up*std::sin(pitchRadians) + distance*std::cos(pitchRadians);
            const double safeForward = std::max(1.0, cameraForward);
            projected = QPointF{meshVanX + focalLength*cameraRight/safeForward,
                                meshVanY - focalLength*cameraUp/safeForward};
            const double sampleStep = 1.0/3600.0;
            const double northSlope = terrainElevation(latitude+sampleStep,longitude)-terrainMsl;
            const double eastSlope = terrainElevation(latitude,longitude+sampleStep)-terrainMsl;
            const double slopeNorth = northSlope / 30.9;
            const double slopeEast = eastSlope / (30.9*std::cos(latitude*3.141592653589793/180.0));
            const double normalLength = std::sqrt(slopeEast*slopeEast+slopeNorth*slopeNorth+1.0);
            const double normalEast=-slopeEast/normalLength, normalNorth=-slopeNorth/normalLength, normalUp=1.0/normalLength;
            const double directional=normalEast*(-.30)+normalNorth*(-.24)+normalUp*.88;
            shade=std::clamp(.58+directional*.34,.42,.96);
        } else {
            elevation = -(std::sin(column*1.17+row*.83)+std::cos(column*.47-row*1.31)*.55)*180.0*depth;
            const double spread = std::pow(depth, 0.72) * w * 0.78;
            const double relief = -elevation * (0.018 + depth*0.035);
            projected=QPointF{meshVanX+xFactor*spread,meshVanY+8+depth*depth*(h-meshVanY)+relief};
        }
        return MeshSample{projected,elevation,terrainMsl,latitude,longitude,shade};
    };
    auto meshColor = [&](int column, int row, double elevation, double terrainMsl, double shade) {
        if (terrain_real_) {
            const double clearance=aircraft_altitude_*.3048-terrainMsl;
            if (clearance<30.0) return QColor{190,34,28};
            if (clearance<300.0) return QColor{190,132,24};
            const double altitudeBand=std::clamp((terrainMsl-20.0)/1450.0,0.0,1.0);
            double red=(48.0+altitudeBand*73.0)*shade;
            double green=(78.0-altitudeBand*20.0)*shade;
            double blue=(43.0-altitudeBand*9.0)*shade;
            // Atmospheric perspective blends only the distant terrain. It
            // improves depth while preserving TAWS colours and true geometry.
            const double distanceFog=std::pow(1.0-static_cast<double>(row)/rows,2.2)*.38;
            red=red*(1.0-distanceFog)+70.0*distanceFog;
            green=green*(1.0-distanceFog)+91.0*distanceFog;
            blue=blue*(1.0-distanceFog)+99.0*distanceFog;
            return QColor{std::clamp(static_cast<int>(red),22,150),
                          std::clamp(static_cast<int>(green),28,118),
                          std::clamp(static_cast<int>(blue),20,105)};
        }
        const double light = std::sin(column*1.31+row*2.17) * 0.65 + std::cos(column*.41-row*.73)*0.35;
        const int variation = static_cast<int>((light+1.0)*11.0);
        const int distanceLift = (rows-row)*2;
        const int altitudeTint = std::clamp(static_cast<int>(elevation/90.0), 0, 18);
        return QColor{34+variation+distanceLift+altitudeTint, 52+variation+distanceLift+altitudeTint,
                      31+variation/2+distanceLift+altitudeTint/2};
    };
    std::vector<MeshSample> samples;
    samples.reserve((columns+1)*(rows+1));
    for (int row=0;row<=rows;++row)
        for (int column=0;column<=columns;++column)
            samples.push_back(meshSample(column,row));
    auto sampleAt=[&](int column,int row)->const MeshSample& {
        return samples[static_cast<std::size_t>(row*(columns+1)+column)];
    };
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const auto& sa=sampleAt(column,row); const auto& sb=sampleAt(column+1,row);
            const auto& sc=sampleAt(column,row+1); const auto& sd=sampleAt(column+1,row+1);
            const TerrainVertex a{sa.point,meshColor(column,row,sa.elevation,sa.terrain_msl,sa.shade)};
            const TerrainVertex b{sb.point,meshColor(column+1,row,sb.elevation,sb.terrain_msl,sb.shade)};
            const TerrainVertex c{sc.point,meshColor(column,row+1,sc.elevation,sc.terrain_msl,sc.shade)};
            const TerrainVertex d{sd.point,meshColor(column+1,row+1,sd.elevation,sd.terrain_msl,sd.shade)};
            mesh.insert(mesh.end(), {a,c,b,b,c,d});
        }
    }
    for (auto& vertex : mesh) vertex.point = rotate_point(vertex.point, center, angle);
    if (terrain_texture_) {
        std::vector<TexturedTerrainVertex> textured;
        textured.reserve(columns*rows*6);
        auto texturedVertex=[&](const MeshSample& sample){
            QPointF point=rotate_point(sample.point,center,angle);
            const double u=std::clamp((sample.longitude-12.10)/(12.29-12.10),0.0,1.0);
            const double v=std::clamp((45.72-sample.latitude)/(45.72-45.59),0.0,1.0);
            return TexturedTerrainVertex{point,{u,v}};
        };
        for(int row=0;row<rows;++row) for(int column=0;column<columns;++column){
            const auto& a=sampleAt(column,row); const auto& b=sampleAt(column+1,row);
            const auto& c=sampleAt(column,row+1); const auto& d=sampleAt(column+1,row+1);
            textured.insert(textured.end(),{texturedVertex(a),texturedVertex(c),texturedVertex(b),
                                            texturedVertex(b),texturedVertex(c),texturedVertex(d)});
        }
        root->appendChildNode(textured_terrain_mesh(textured,terrain_texture_));
        Points terrainShade{{-w,horizon},{2*w,horizon},{-w,2*h},{2*w,2*h}};
        root->appendChildNode(geometry_node(rotated(terrainShade,center,angle),QColor{4,18,16,72},QSGGeometry::DrawTriangleStrip));
    } else root->appendChildNode(terrain_mesh(mesh));

    root->appendChildNode(geometry_node(rotated({{-w,horizon},{2*w,horizon}}, center, angle), QColor{"#F2F0E9"}, QSGGeometry::DrawLines, 2.0F));

    // Perspective runway and cyan guidance corridor.
    const double airportNorth = (scenario_latitude_-aircraft_latitude_)*111320.0;
    const double airportEast = (scenario_longitude_-aircraft_longitude_)*111320.0*
                               std::cos(aircraft_latitude_*3.141592653589793/180.0);
    const double airportBearing = std::atan2(airportEast,airportNorth)*180.0/3.141592653589793;
    double bearingDelta = std::remainder(airportBearing-heading_,360.0);
    const double vanX = std::clamp(w*.5 + std::tan(std::clamp(bearingDelta,-55.0,55.0)*3.141592653589793/180.0)*w*.42,
                                   w*.08,w*.92);
    const double vanY = horizon + 10;
    if (scenario_loaded_) {
    const double runwayNearY=h*.84;
    Points runway{{vanX-10,vanY},{vanX+10,vanY},{vanX+w*.13,runwayNearY},{vanX-w*.13,runwayNearY}};
    root->appendChildNode(geometry_node(rotated(runway, center, angle), QColor{"#20272A"}, QSGGeometry::DrawTriangleFan));
    root->appendChildNode(geometry_node(rotated({{vanX-10,vanY},{vanX-w*.13,runwayNearY}}, center, angle), QColor{"#E8ECEB"}, QSGGeometry::DrawLines, 2.0F));
    root->appendChildNode(geometry_node(rotated({{vanX+10,vanY},{vanX+w*.13,runwayNearY}}, center, angle), QColor{"#E8ECEB"}, QSGGeometry::DrawLines, 2.0F));
    for (int i = 1; i < 8; ++i) {
        const double t = static_cast<double>(i) / 8.0;
        const double y = vanY + (runwayNearY-vanY) * t*t;
        const double half = 3 + (w*0.13-3)*t*t;
        root->appendChildNode(geometry_node(rotated({{vanX-half,y},{vanX+half,y}}, center, angle), QColor{"#AEB9B8"}, QSGGeometry::DrawLines, i%2 ? 1.0F : 2.0F));
    }
    // Centreline, aiming-point blocks and threshold bars.
    for (int i = 1; i < 7; ++i) {
        const double nearT = i / 7.0;
        const double farT = std::min(0.98, nearT + 0.055);
        const double y1 = vanY + (runwayNearY-vanY)*nearT*nearT;
        const double y2 = vanY + (runwayNearY-vanY)*farT*farT;
        const double half1 = 0.8 + nearT*2.0;
        const double half2 = 0.8 + farT*2.0;
        root->appendChildNode(geometry_node(rotated({{vanX-half1,y1},{vanX+half1,y1},{vanX-half2,y2},{vanX+half2,y2}}, center, angle),
                                                 QColor{"#F4F4EA"}, QSGGeometry::DrawTriangleStrip));
    }
    const double markY = h*.68;
    root->appendChildNode(geometry_node(rotated({{vanX-w*.055,markY},{vanX-w*.025,markY},{vanX-w*.048,markY+12},{vanX-w*.02,markY+12}}, center, angle), QColor{"#F4F4EA"}, QSGGeometry::DrawTriangleStrip));
    root->appendChildNode(geometry_node(rotated({{vanX+w*.025,markY},{vanX+w*.055,markY},{vanX+w*.02,markY+12},{vanX+w*.048,markY+12}}, center, angle), QColor{"#F4F4EA"}, QSGGeometry::DrawTriangleStrip));
    // Threshold piano keys.
    for (int side : {-1, 1}) {
        for (int stripe = 0; stripe < 4; ++stripe) {
            const double x1 = vanX + side * (w*.05 + stripe*w*.012);
            const double x2 = x1 + side*w*.008;
            root->appendChildNode(geometry_node(rotated({{x1,h*.79},{x2,h*.79},{x1,h*.83},{x2,h*.83}}, center, angle),
                                                     QColor{"#F4F4EA"}, QSGGeometry::DrawTriangleStrip));
        }
    }
    // Four-light PAPI, deliberately marked as simulated by the QML overlay.
    for (int lamp = 0; lamp < 4; ++lamp) {
        const double x = vanX+w*.13 + lamp*7.0;
        const QColor color = lamp < 2 ? QColor{"#F6F3DF"} : QColor{"#FF3A32"};
        root->appendChildNode(geometry_node({{x,h*.67},{x+5,h*.67},{x,h*.67+4},{x+5,h*.67+4}}, color, QSGGeometry::DrawTriangleStrip));
    }
    root->appendChildNode(geometry_node(rotated({{vanX-w*.09,h*.76},{vanX,h*.58},{vanX+w*.09,h*.76}}, center, angle), QColor{"#20D6F4"}, QSGGeometry::DrawLineStrip, 3.0F));
    }

    // Perspective ground grid.
    for (int i = -4; i <= 4; ++i)
        root->appendChildNode(geometry_node(rotated({{vanX+i*9,vanY},{vanX+i*w*0.13,h}}, center, angle), QColor{"#64705E"}, QSGGeometry::DrawLines, 0.7F));

    // Open obstacle layer. Visual simulation only; never substitutes AIP/eTOD.
    const double headingRad=heading_*3.141592653589793/180.0;
    int obstacleCount=0;
    int obstacleCandidate=0;
    std::vector<Obstacle> nearbyObstacles;
    const int baseLat=static_cast<int>(std::floor(aircraft_latitude_*10.0));
    const int baseLon=static_cast<int>(std::floor(aircraft_longitude_*10.0));
    for(int latOffset=-2;latOffset<=2;++latOffset) for(int lonOffset=-2;lonOffset<=2;++lonOffset){
        const auto cell=obstacle_cells_.value(QStringLiteral("%1:%2").arg(baseLat+latOffset).arg(baseLon+lonOffset));
        nearbyObstacles.insert(nearbyObstacles.end(),cell.begin(),cell.end());
    }
    for(const auto& obstacle:nearbyObstacles){
        const double north=(obstacle.latitude-aircraft_latitude_)*111320.0;
        const double east=(obstacle.longitude-aircraft_longitude_)*111320.0*std::cos(aircraft_latitude_*3.141592653589793/180.0);
        const double forward=north*std::cos(headingRad)+east*std::sin(headingRad);
        const double right=-north*std::sin(headingRad)+east*std::cos(headingRad);
        if(forward<500.0||forward>12000.0||std::abs(right)>forward*.62)continue;
        if((obstacleCandidate++%5)!=0||obstacleCount>=24)continue;
        const double depth=std::clamp((20000.0-forward)/19500.0,0.02,1.0);
        const double xFactor=right/(forward*.58);
        const double spread=std::pow(depth,.72)*w*.78;
        QPointF base{meshVanX+xFactor*spread,meshVanY+depth*depth*(h-meshVanY)};
        const double markerHeight=std::clamp(obstacle.height_m/forward*h*18.0,4.0,18.0);
        const double obstacleMsl=terrainElevation(obstacle.latitude,obstacle.longitude)+obstacle.height_m;
        const double clearance=aircraft_altitude_*.3048-obstacleMsl;
        const QColor threat=clearance<300.0?QColor{"#FF3B35"}:QColor{"#F2C230"};
        base=rotate_point(base,center,angle);
        const QPointF top{base.x(),base.y()-markerHeight};
        root->appendChildNode(geometry_node({base,top},threat,QSGGeometry::DrawLines,1.0F));
        root->appendChildNode(geometry_node({{top.x(),top.y()-3},{top.x()-3,top.y()+2},{top.x()+3,top.y()+2}},threat,QSGGeometry::DrawTriangleFan));
        ++obstacleCount;
    }

    // Lower HSI arc.
    Points arc;
    const double compassRadius=w*.4257;
    const double compassCenterY=h*.96+compassRadius*.81915204;
    for (int i = 0; i <= 80; ++i) {
        const double a = (235.0 + i*70.0/80.0) * 3.141592653589793/180.0;
        arc.emplace_back(w*.5+std::cos(a)*compassRadius,compassCenterY+std::sin(a)*compassRadius);
    }
    root->appendChildNode(geometry_node(arc, QColor{"#D6E0E3"}, QSGGeometry::DrawLineStrip, 2.0F));
    for (int i = 0; i <= 14; ++i) {
        const double a = (235.0 + i*70.0/14.0) * 3.141592653589793/180.0;
        const QPointF p1{w*.5+std::cos(a)*compassRadius,compassCenterY+std::sin(a)*compassRadius};
        const QPointF p2{w*.5+std::cos(a)*(compassRadius-11),compassCenterY+std::sin(a)*(compassRadius-11)};
        root->appendChildNode(geometry_node({p1,p2}, QColor{"#D6E0E3"}, QSGGeometry::DrawLines, 1.0F));
    }
    return root;
}
