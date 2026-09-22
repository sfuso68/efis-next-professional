import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

Node {
    id: tile
    required property url lowGeometry
    required property url highGeometry
    required property url textureSource
    required property real cameraX
    required property real cameraZ
    required property real aircraftAltitudeFt
    property real tileX: 0
    property real tileZ: 0
    property real tileWidth: terrainProjection.tileWidth
    property real tileDepth: terrainProjection.tileDepth
    property real maximumElevation: 3200
    readonly property real footprintDistance: terrainProjection.footprintDistance(
        cameraX, cameraZ, tileX, tileZ)
    property bool highDetail: true
    property bool tileEnabled: true

    function updateLodState() {
        highDetail = terrainProjection.highDetailForDistance(
            footprintDistance, highDetail)
        tileEnabled = terrainProjection.tileEnabledForDistance(footprintDistance)
    }
    onFootprintDistanceChanged: updateLodState()
    Component.onCompleted: updateLodState()

    position: Qt.vector3d(tileX, maximumElevation/2, tileZ)

    Loader3D {
        active: tile.tileEnabled
        sourceComponent: terrainContent
    }
    Component {
      id: terrainContent
      Node {
    Model {
        visible: tile.tileEnabled && opacity > 0.001
        opacity: tile.highDetail ? 0.0 : 1.0
        Behavior on opacity { NumberAnimation { duration: 550; easing.type: Easing.InOutQuad } }
        geometry: HeightFieldGeometry {
            source: tile.lowGeometry
            extents: Qt.vector3d(tile.tileWidth,tile.maximumElevation,tile.tileDepth)
            smoothShading: true
        }
        materials: tileMaterial
    }
    Model {
        visible: tile.tileEnabled && opacity > 0.001
        opacity: tile.highDetail ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 550; easing.type: Easing.InOutQuad } }
        geometry: HeightFieldGeometry {
            source: tile.highGeometry
            extents: Qt.vector3d(tile.tileWidth,tile.maximumElevation,tile.tileDepth)
            smoothShading: true
        }
        materials: tileMaterial
    }
    CustomMaterial {
        id: tileMaterial
        shadingMode: CustomMaterial.Shaded
        fragmentShader: "shaders/terrain_taws.frag"
        alwaysDirty: true
        property real uAircraftAltitudeM: tile.aircraftAltitudeFt * 0.3048
        property real uTawsBlend: Math.max(0.0, Math.min(1.0, (tile.aircraftAltitudeFt - 1200.0) / 1000.0))
        property TextureInput uColorMap: TextureInput {
            texture: Texture {
                source: tile.tileEnabled ? tile.textureSource : ""
                generateMipmaps: true
                magFilter: Texture.Linear
                minFilter: Texture.Linear
                mipFilter: Texture.Linear
            }
        }
    }
      }
    }
}
