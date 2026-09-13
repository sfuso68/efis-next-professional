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
    property bool operationalProfilePreview: Qt.application.arguments.indexOf("--setup-operational-preview") >= 0 || Qt.application.arguments.indexOf("--setup-operational-entry-preview") >= 0 || Qt.application.arguments.indexOf("--setup-operational-load-preview") >= 0 || Qt.application.arguments.indexOf("--setup-operational-check-preview") >= 0 || Qt.application.arguments.indexOf("--setup-operational-keyboard-preview") >= 0
    property bool maintenanceVisible: Qt.application.arguments.indexOf("--setup-preview") >= 0 || Qt.application.arguments.indexOf("--setup-aircraft-preview") >= 0 || Qt.application.arguments.indexOf("--setup-catalog-preview") >= 0 || Qt.application.arguments.indexOf("--setup-review-preview") >= 0 || Qt.application.arguments.indexOf("--setup-verify-preview") >= 0 || Qt.application.arguments.indexOf("--setup-engine-preview") >= 0 || Qt.application.arguments.indexOf("--setup-sensor-map-preview") >= 0 || Qt.application.arguments.indexOf("--setup-channel-test-preview") >= 0 || Qt.application.arguments.indexOf("--setup-airport-preview") >= 0 || Qt.application.arguments.indexOf("--setup-nearest-preview") >= 0 || Qt.application.arguments.indexOf("--setup-bench-preview") >= 0 || root.operationalProfilePreview
    property bool simulatorVisible: Qt.application.arguments.indexOf("--simulator-preview") >= 0
    function selectedEmergencyPreview() {
        const prefix = "--emergency-preview="
        for (let argument of Qt.application.arguments) {
            if (argument.indexOf(prefix) === 0)
                return argument.substring(prefix.length).trim().toUpperCase()
        }
        return Qt.application.arguments.indexOf("--emergency-preview") >= 0
                ? "ENGINE_POWER_LOSS" : ""
    }
    property string emergencyPreviewKind: selectedEmergencyPreview()
    property bool emergencyPreview: emergencyPreviewKind.length > 0
    Component.onCompleted: { if (Qt.application.arguments.indexOf("--setup-bench-preview") >= 0) bench.startReplay(bench.defaultSource); if (Qt.application.arguments.indexOf("--setup-review-preview") >= 0 || Qt.application.arguments.indexOf("--setup-verify-preview") >= 0) pfd.stageAircraft(1); if (Qt.application.arguments.indexOf("--setup-engine-preview") >= 0 || Qt.application.arguments.indexOf("--setup-sensor-map-preview") >= 0 || Qt.application.arguments.indexOf("--setup-channel-test-preview") >= 0) pfd.stageEngine(1); if (Qt.application.arguments.indexOf("--setup-channel-test-preview") >= 0) pfd.startEngineSensorTest("RPM",true); if (Qt.application.arguments.indexOf("--simulator-preview") >= 0) pfd.startSystemSimulation("full-flight"); if (root.emergencyPreview) { pfd.startSystemSimulation(root.emergencyPreviewKind === "ENGINE_POWER_LOSS" ? "engine-emergency" : "full-flight"); pfd.activateEmergencyAssist(root.emergencyPreviewKind, "preview argument"); pfd.confirmEmergencyAssist(root.emergencyPreviewKind) } }

    AdaptiveShell { id: adaptiveShell; anchors.fill: parent; controller: pfd; initialPage: startupPage }
    Shortcut { sequence: "Ctrl+1"; onActivated: adaptiveShell.currentPage = "PFD" }
    Shortcut { sequence: "Ctrl+2"; onActivated: adaptiveShell.currentPage = "MISSION" }
    Shortcut { sequence: "Ctrl+3"; onActivated: adaptiveShell.currentPage = "FPV" }
    Shortcut { sequence: "Ctrl+4"; onActivated: adaptiveShell.currentPage = "EMS" }
    Shortcut { sequence: "Ctrl+5"; onActivated: adaptiveShell.currentPage = "NAV" }
    Shortcut { sequence: "Ctrl+6"; onActivated: root.simulatorVisible = !root.simulatorVisible }
    Shortcut { sequence: "F12"; onActivated: { pfd.activateEmergencyAssist("ENGINE_POWER_LOSS", "keyboard test input"); adaptiveShell.currentPage = "EMERGENCY" } }

    Rectangle {
        visible: adaptiveShell.currentPage !== "EMERGENCY" && !root.maintenanceVisible && !root.simulatorVisible
        anchors { right: parent.right; bottom: parent.bottom; rightMargin: 10*Math.min(root.width/1024,root.height/600); bottomMargin: 158*Math.min(root.width/1024,root.height/600) }
        width: 106*Math.min(root.width/1024,root.height/600); height: 39*Math.min(root.width/1024,root.height/600); z: 90
        radius: 4*Math.min(root.width/1024,root.height/600); color: "#4A130F"; border.color: "#FF3B30"; border.width: 1
        Text { anchors.centerIn: parent; text: "HOLD EMERGENCY"; color: "white"; font.pixelSize: 8*Math.min(root.width/1024,root.height/600); font.bold: true }
        TapHandler { longPressThreshold: 1.2; onLongPressed: { pfd.activateEmergencyAssist("ENGINE_POWER_LOSS", "touch long press"); adaptiveShell.currentPage = "EMERGENCY" } }
    }

    MouseArea {
        anchors { left: parent.left; top: parent.top }
        width: 190 * Math.min(root.width/1024, root.height/600)
        height: 54 * Math.min(root.width/1024, root.height/600)
        pressAndHoldInterval: 1200
        onPressAndHold: root.maintenanceVisible = true
    }
    Loader {
        anchors.fill: parent
        z: 100
        active: root.maintenanceVisible
        asynchronous: false
        sourceComponent: maintenanceComponent
    }
    Component {
        id: maintenanceComponent
        MaintenanceOverlay {
            sectionIndex: Qt.application.arguments.indexOf("--setup-bench-preview") >= 0 ? 3 : (Qt.application.arguments.indexOf("--setup-airport-preview") >= 0 || Qt.application.arguments.indexOf("--setup-nearest-preview") >= 0 ? 5 : (Qt.application.arguments.indexOf("--setup-engine-preview") >= 0 || Qt.application.arguments.indexOf("--setup-sensor-map-preview") >= 0 || Qt.application.arguments.indexOf("--setup-channel-test-preview") >= 0 ? 6 : (Qt.application.arguments.indexOf("--setup-aircraft-preview") >= 0 || Qt.application.arguments.indexOf("--setup-catalog-preview") >= 0 || Qt.application.arguments.indexOf("--setup-review-preview") >= 0 || Qt.application.arguments.indexOf("--setup-verify-preview") >= 0 || root.operationalProfilePreview ? 2 : 0)))
            operationalProfileVisible: root.operationalProfilePreview
            operationalPreviewPage: Qt.application.arguments.indexOf("--setup-operational-entry-preview") >= 0 || Qt.application.arguments.indexOf("--setup-operational-keyboard-preview") >= 0 ? 1 : (Qt.application.arguments.indexOf("--setup-operational-load-preview") >= 0 ? 2 : (Qt.application.arguments.indexOf("--setup-operational-check-preview") >= 0 ? 3 : 0))
            catalogVisible: Qt.application.arguments.indexOf("--setup-catalog-preview") >= 0
            reviewVisible: Qt.application.arguments.indexOf("--setup-review-preview") >= 0
            verificationVisible: Qt.application.arguments.indexOf("--setup-verify-preview") >= 0
            engineReviewVisible: Qt.application.arguments.indexOf("--setup-engine-preview") >= 0
            sensorMappingVisible: Qt.application.arguments.indexOf("--setup-sensor-map-preview") >= 0
            benchVisible: Qt.application.arguments.indexOf("--setup-bench-preview") >= 0
            channelTestVisible: Qt.application.arguments.indexOf("--setup-channel-test-preview") >= 0
            airportSelectorVisible: Qt.application.arguments.indexOf("--setup-airport-preview") >= 0 || Qt.application.arguments.indexOf("--setup-nearest-preview") >= 0
            airportNearestOnly: Qt.application.arguments.indexOf("--setup-nearest-preview") >= 0
            controller: pfd
            uiScale: Math.min(root.width/1024, root.height/600)
            onCloseRequested: root.maintenanceVisible = false
        }
    }
    Loader {
        anchors.fill: parent
        active: root.simulatorVisible
        asynchronous: false
        sourceComponent: simulatorComponent
    }
    Component {
        id: simulatorComponent
        SystemSimulatorOverlay {
            controller: pfd
            onCloseRequested: root.simulatorVisible = false
        }
    }
}
