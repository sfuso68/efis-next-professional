import QtQuick
import QtQuick.Controls
import QtQuick.Window

ApplicationWindow {
    id: root
    visible: true
    width: 1024; height: 600
    minimumWidth: 800; minimumHeight: 480
    title: "EFIS NEXT Adaptive Mission Deck"
    color: "#061018"
    visibility: efisKiosk ? Window.FullScreen : Window.Windowed
    flags: efisKiosk ? (Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint) : Qt.Window
    onClosing: function(close) { if (efisKiosk) close.accepted = false }
    property bool maintenanceVisible: Qt.application.arguments.indexOf("--setup-preview") >= 0 || Qt.application.arguments.indexOf("--setup-aircraft-preview") >= 0 || Qt.application.arguments.indexOf("--setup-catalog-preview") >= 0 || Qt.application.arguments.indexOf("--setup-review-preview") >= 0 || Qt.application.arguments.indexOf("--setup-verify-preview") >= 0 || Qt.application.arguments.indexOf("--setup-engine-preview") >= 0 || Qt.application.arguments.indexOf("--setup-sensor-map-preview") >= 0 || Qt.application.arguments.indexOf("--setup-channel-test-preview") >= 0 || Qt.application.arguments.indexOf("--setup-airport-preview") >= 0 || Qt.application.arguments.indexOf("--setup-nearest-preview") >= 0 || Qt.application.arguments.indexOf("--setup-bench-preview") >= 0
    property bool simulatorVisible: Qt.application.arguments.indexOf("--simulator-preview") >= 0
    Component.onCompleted: { if (Qt.application.arguments.indexOf("--setup-bench-preview") >= 0) bench.startReplay(bench.defaultSource); if (Qt.application.arguments.indexOf("--setup-review-preview") >= 0 || Qt.application.arguments.indexOf("--setup-verify-preview") >= 0) pfd.stageAircraft(1); if (Qt.application.arguments.indexOf("--setup-engine-preview") >= 0 || Qt.application.arguments.indexOf("--setup-sensor-map-preview") >= 0 || Qt.application.arguments.indexOf("--setup-channel-test-preview") >= 0) pfd.stageEngine(1); if (Qt.application.arguments.indexOf("--setup-channel-test-preview") >= 0) pfd.startEngineSensorTest("RPM",true); if (Qt.application.arguments.indexOf("--simulator-preview") >= 0) pfd.startSystemSimulation("full-flight") }

    AdaptiveShell { id: adaptiveShell; anchors.fill: parent; controller: pfd; initialPage: startupPage }
    Shortcut { sequence: "Ctrl+1"; onActivated: adaptiveShell.currentPage = "PFD" }
    Shortcut { sequence: "Ctrl+2"; onActivated: adaptiveShell.currentPage = "MISSION" }
    Shortcut { sequence: "Ctrl+3"; onActivated: adaptiveShell.currentPage = "FPV" }
    Shortcut { sequence: "Ctrl+4"; onActivated: adaptiveShell.currentPage = "EMS" }
    Shortcut { sequence: "Ctrl+5"; onActivated: adaptiveShell.currentPage = "NAV" }
    Shortcut { sequence: "Ctrl+6"; onActivated: root.simulatorVisible = !root.simulatorVisible }

    MouseArea {
        anchors { left: parent.left; top: parent.top }
        width: 190 * Math.min(root.width/1024, root.height/600)
        height: 54 * Math.min(root.width/1024, root.height/600)
        pressAndHoldInterval: 1200
        onPressAndHold: root.maintenanceVisible = true
    }
    MaintenanceOverlay {
        anchors.fill: parent; z: 100; visible: root.maintenanceVisible
        sectionIndex: Qt.application.arguments.indexOf("--setup-bench-preview") >= 0 ? 3 : (Qt.application.arguments.indexOf("--setup-airport-preview") >= 0 || Qt.application.arguments.indexOf("--setup-nearest-preview") >= 0 ? 5 : (Qt.application.arguments.indexOf("--setup-engine-preview") >= 0 || Qt.application.arguments.indexOf("--setup-sensor-map-preview") >= 0 || Qt.application.arguments.indexOf("--setup-channel-test-preview") >= 0 ? 6 : (Qt.application.arguments.indexOf("--setup-aircraft-preview") >= 0 || Qt.application.arguments.indexOf("--setup-catalog-preview") >= 0 || Qt.application.arguments.indexOf("--setup-review-preview") >= 0 || Qt.application.arguments.indexOf("--setup-verify-preview") >= 0 ? 2 : 0)))
        catalogVisible: Qt.application.arguments.indexOf("--setup-catalog-preview") >= 0
        reviewVisible: Qt.application.arguments.indexOf("--setup-review-preview") >= 0
        verificationVisible: Qt.application.arguments.indexOf("--setup-verify-preview") >= 0
        engineReviewVisible: Qt.application.arguments.indexOf("--setup-engine-preview") >= 0
        sensorMappingVisible: Qt.application.arguments.indexOf("--setup-sensor-map-preview") >= 0
        benchVisible: Qt.application.arguments.indexOf("--setup-bench-preview") >= 0
        channelTestVisible: Qt.application.arguments.indexOf("--setup-channel-test-preview") >= 0
        airportSelectorVisible: Qt.application.arguments.indexOf("--setup-airport-preview") >= 0 || Qt.application.arguments.indexOf("--setup-nearest-preview") >= 0
        airportNearestOnly: Qt.application.arguments.indexOf("--setup-nearest-preview") >= 0
        controller: pfd; uiScale: Math.min(root.width/1024, root.height/600)
        onCloseRequested: root.maintenanceVisible = false
    }
    SystemSimulatorOverlay {
        anchors.fill:parent; visible:root.simulatorVisible; controller:pfd
        onCloseRequested:root.simulatorVisible=false
    }
}
