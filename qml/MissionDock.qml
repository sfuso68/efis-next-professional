import QtQuick

Rectangle {
    id: dock
    property real uiScale: 1
    property real groundSpeed: 0
    property real heading: 0
    property bool navigationValid: true
    property bool engineAvailable: false
    property real engineRpm: 0
    property real fuelTotal: 0
    property real busVoltage: 0
    property int engineHealth: 0
    color: "#08121B"; border.color: "#314654"
    Row {
        anchors { fill: parent; margins: 5 * dock.uiScale } spacing: 5 * dock.uiScale
        Repeater {
            model: [
                { tag: "ENGINE", value: dock.engineAvailable ? Math.round(dock.engineRpm) + " RPM" : "NOT INSTALLED", state: dock.engineAvailable ? "SIM" : "N/A", color: dock.engineHealth >= 2 ? "#FF4136" : dock.engineHealth === 1 ? "#E6A72E" : dock.engineAvailable ? "#62D347" : "#7F929E" },
                { tag: "FUEL", value: dock.engineAvailable ? dock.fuelTotal.toFixed(1) + " GAL" : "NOT INSTALLED", state: dock.engineAvailable ? "SIM" : "N/A", color: dock.engineAvailable ? "#62D347" : "#7F929E" },
                { tag: "ELECTRICAL", value: dock.engineAvailable ? dock.busVoltage.toFixed(1) + " V" : "NOT INSTALLED", state: dock.engineAvailable ? "SIM" : "N/A", color: dock.engineAvailable ? "#62D347" : "#7F929E" },
                { tag: "NAVIGATION", value: "HDG " + Math.round(dock.heading) + "°", state: dock.navigationValid ? "SIM" : "FAIL", color: dock.navigationValid ? "#19C3E6" : "#FF4136" },
                { tag: "GROUND", value: Math.round(dock.groundSpeed) + " KT", state: "SIM", color: "#19C3E6" },
                { tag: "FPV / VIDEO", value: "NOT INSTALLED", state: "N/A", color: "#7F929E" },
                { tag: "CHECKLIST", value: "READY", state: "LOCAL", color: "#62D347" }
            ]
            delegate: Rectangle {
                required property var modelData
                width: (dock.width - 40 * dock.uiScale) / 7; height: dock.height - 10 * dock.uiScale
                color: "#0D1A25"; border.color: "#314654"; radius: 3
                Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 3 * dock.uiScale; color: modelData.color }
                Text { x: 9 * dock.uiScale; y: 12 * dock.uiScale; text: modelData.tag; color: "#7F929E"; font.pixelSize: 9 * dock.uiScale; font.bold: true }
                Text { x: 9 * dock.uiScale; y: 37 * dock.uiScale; text: modelData.value; color: modelData.color; font.pixelSize: 12 * dock.uiScale; font.bold: true; elide: Text.ElideRight; width: parent.width - 18 * dock.uiScale }
                Text { x: 9 * dock.uiScale; anchors.bottom: parent.bottom; anchors.bottomMargin: 9 * dock.uiScale; text: modelData.state; color: modelData.color; font.pixelSize: 8 * dock.uiScale; font.bold: true }
            }
        }
    }
}
