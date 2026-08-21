import QtQuick

Rectangle {
    id: lane
    required property string side
    property real uiScale: 1
    property real airspeed: 0
    property real altitude: 0
    property real verticalSpeed: 0
    property real trend: 0
    property bool primaryValid: true

    color: "#0A151F"; border.color: primaryValid ? "#314654" : "#FF4136"; border.width: primaryValid ? 1 : 3
    readonly property bool leftSide: side === "left"

    Text { x: 12 * lane.uiScale; y: 13 * lane.uiScale; text: lane.leftSide ? "IAS" : "ALT"; color: "#19C3E6"; font.pixelSize: 14 * lane.uiScale; font.bold: true }
    Text { anchors.right: parent.right; anchors.rightMargin: 12 * lane.uiScale; y: 16 * lane.uiScale; text: lane.leftSide ? "KT" : "FT"; color: "#7F929E"; font.pixelSize: 10 * lane.uiScale; font.bold: true }
    Text { anchors.horizontalCenter: parent.horizontalCenter; y: 54 * lane.uiScale; text: lane.primaryValid ? Math.round(lane.leftSide ? lane.airspeed : lane.altitude) : "---"; color: lane.primaryValid ? "#F2F0E9" : "#FF4136"; font.pixelSize: 40 * lane.uiScale; font.bold: true }

    Rectangle { x: 12 * lane.uiScale; y: 116 * lane.uiScale; width: parent.width - 24 * lane.uiScale; height: 1; color: "#314654" }
    Column {
        x: 12 * lane.uiScale; y: 137 * lane.uiScale; spacing: 23 * lane.uiScale
        Repeater {
            model: lane.leftSide ? [
                { tag: "TAS", value: Math.round(lane.airspeed + 4) + " KT", state: "SIM" },
                { tag: "AOA", value: "NOT INSTALLED", state: "N/A" },
                { tag: "6S TREND", value: (lane.trend >= 0 ? "+" : "") + lane.trend.toFixed(1), state: "SIM" },
                { tag: "CONFIDENCE", value: lane.primaryValid ? "AVAILABLE" : "FAIL", state: "SIM" }
            ] : [
                { tag: "RADIO ALT", value: "NOT INSTALLED", state: "N/A" },
                { tag: "V/S", value: Math.round(lane.verticalSpeed) + " FPM", state: "SIM" },
                { tag: "6S TREND", value: (lane.trend >= 0 ? "+" : "") + Math.round(lane.trend) + " FT", state: "SIM" },
                { tag: "MINIMUMS", value: "NOT SET", state: "UNVERIFIED" }
            ]
            delegate: Item {
                required property var modelData
                width: lane.width - 24 * lane.uiScale; height: 45 * lane.uiScale
                Text { text: modelData.tag; color: "#7F929E"; font.pixelSize: 9 * lane.uiScale; font.bold: true }
                Text { y: 16 * lane.uiScale; text: modelData.value; color: modelData.value.indexOf("NOT") >= 0 ? "#7F929E" : "#F2F0E9"; font.pixelSize: 13 * lane.uiScale; font.bold: true }
                Text { anchors.right: parent.right; y: 18 * lane.uiScale; text: modelData.state; color: modelData.state === "SIM" ? "#19C3E6" : "#E6A72E"; font.pixelSize: 8 * lane.uiScale; font.bold: true }
            }
        }
    }
}
