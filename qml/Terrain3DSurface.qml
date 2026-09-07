import QtQuick
import QtQuick3D
import QtQuick3D.Helpers

Item {
    id: terrain
    required property real roll
    required property real pitch
    required property real heading
    required property real latitude
    required property real longitude
    required property real altitude

    readonly property real tileWidth: 77150
    readonly property real tileDepth: 111320
    readonly property real cameraX: (longitude - 12.5) * tileWidth
    readonly property real cameraZ: -(latitude - 46.5) * tileDepth
    readonly property string gpuRoot: efisTerrainGpuRoot
    readonly property string imageryRoot: efisImageryTextureRoot
    function asset(root, name) { return root + name }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.00; color: "#12385d" }
            GradientStop { position: 0.40; color: "#397cad" }
            GradientStop { position: 0.72; color: "#79b3d0" }
            GradientStop { position: 1.00; color: "#c5d7d8" }
        }
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: parent.height * 0.44
        color: "transparent"
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#00182f4d" }
            GradientStop { position: 0.72; color: "#00182f4d" }
            GradientStop { position: 1.0; color: "#26f6d3a0" }
        }
    }

    View3D {
        anchors.fill: parent
        environment: ExtendedSceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
            tonemapMode: SceneEnvironment.TonemapModeFilmic
            temporalAAEnabled: true
            temporalAAStrength: 0.35
            fog: Fog {
                enabled: true
                color: "#668fa7"
                density: 0.42
                depthEnabled: true
                depthNear: 18000
                depthFar: 72000
                depthCurve: 0.8
            }
        }

        PerspectiveCamera {
            id: flightCamera
            position: Qt.vector3d(terrain.cameraX, terrain.altitude * 0.3048, terrain.cameraZ)
            eulerRotation.x: -terrain.pitch
            eulerRotation.y: -terrain.heading
            eulerRotation.z: -terrain.roll
            fieldOfView: 48
            clipNear: 20
            clipFar: 90000
        }
        camera: flightCamera

        DirectionalLight {
            eulerRotation: Qt.vector3d(-48, -32, 0)
            brightness: 0.78
            color: "#fff3dc"
            ambientColor: "#91a3a0"
            castsShadow: false
        }

        // Six seamless DEM tiles. Each tile switches between 257 and 513
        // samples based on camera distance; the active N46E012 tile retains a
        // 1025-sample near mesh and the Sentinel-2 texture.
        TerrainTile3D { cameraX: terrain.cameraX; cameraZ: terrain.cameraZ; aircraftAltitudeFt: terrain.altitude; tileX: -terrain.tileWidth; tileZ: 0
            lowGeometry: terrain.asset(terrain.gpuRoot,"N46E011-257.pgm"); highGeometry: terrain.asset(terrain.gpuRoot,"N46E011-513.pgm"); textureSource: terrain.asset(terrain.gpuRoot,"N46E011-sentinel2.png") }
        TerrainTile3D { cameraX: terrain.cameraX; cameraZ: terrain.cameraZ; aircraftAltitudeFt: terrain.altitude; tileX: 0; tileZ: 0
            lowGeometry: terrain.asset(terrain.gpuRoot,"N46E012-257.pgm"); highGeometry: terrain.asset(terrain.gpuRoot,"N46E012-1025.pgm"); textureSource: terrain.asset(terrain.imageryRoot,"N46E012-sentinel2-active.png") }
        TerrainTile3D { cameraX: terrain.cameraX; cameraZ: terrain.cameraZ; aircraftAltitudeFt: terrain.altitude; tileX: terrain.tileWidth; tileZ: 0
            lowGeometry: terrain.asset(terrain.gpuRoot,"N46E013-257.pgm"); highGeometry: terrain.asset(terrain.gpuRoot,"N46E013-513.pgm"); textureSource: terrain.asset(terrain.gpuRoot,"N46E013-sentinel2.png") }
        TerrainTile3D { cameraX: terrain.cameraX; cameraZ: terrain.cameraZ; aircraftAltitudeFt: terrain.altitude; tileX: -terrain.tileWidth; tileZ: terrain.tileDepth
            lowGeometry: terrain.asset(terrain.gpuRoot,"N45E011-257.pgm"); highGeometry: terrain.asset(terrain.gpuRoot,"N45E011-513.pgm"); textureSource: terrain.asset(terrain.gpuRoot,"N45E011-sentinel2.png") }
        TerrainTile3D { cameraX: terrain.cameraX; cameraZ: terrain.cameraZ; aircraftAltitudeFt: terrain.altitude; tileX: 0; tileZ: terrain.tileDepth
            lowGeometry: terrain.asset(terrain.gpuRoot,"N45E012-513.pgm"); highGeometry: terrain.asset(terrain.gpuRoot,"N45E012-1025.pgm"); textureSource: terrain.asset(terrain.imageryRoot,"N45E012-sentinel2-active.png") }
        TerrainTile3D { cameraX: terrain.cameraX; cameraZ: terrain.cameraZ; aircraftAltitudeFt: terrain.altitude; tileX: terrain.tileWidth; tileZ: terrain.tileDepth
            lowGeometry: terrain.asset(terrain.gpuRoot,"N45E013-257.pgm"); highGeometry: terrain.asset(terrain.gpuRoot,"N45E013-513.pgm"); textureSource: terrain.asset(terrain.gpuRoot,"N45E013-sentinel2.png") }

        // A translucent horizontal reference adds distance perception without
        // drawing wireframe over the flight-critical terrain.
        Model {
            source: "#Rectangle"
            y: 0
            scale: Qt.vector3d(800, 800, 800)
            materials: PrincipledMaterial {
                baseColor: "#274138"
                opacity: 0.22
                alphaMode: PrincipledMaterial.Blend
                lighting: PrincipledMaterial.NoLighting
            }
        }
    }
}
