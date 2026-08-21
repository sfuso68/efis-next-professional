import QtQuick

Rectangle {
    id: strip
    property real uiScale: 1
    property bool compact: false
    property real groundSpeed: 0
    property real heading: 0
    property real selectedCourse: 0
    property real verticalSpeed: 0
    property int healthLevel: 0
    property bool navigationValid: true

    color: "#071019"
    border.color: "#263B4A"
    border.width: 1

    Row {
        anchors { fill: parent; leftMargin: 8 * strip.uiScale; rightMargin: 8 * strip.uiScale }
        Repeater {
            model: [
                { tag: "GS", value: Math.round(strip.groundSpeed) + " KT", color: "#F4F7FA" },
                { tag: "TRK", value: Math.round(strip.heading) + "°", color: "#00C8F0" },
                { tag: "CRS", value: Math.round(strip.selectedCourse) + "°", color: "#00C8F0" },
                { tag: "V/S", value: Math.round(strip.verticalSpeed) + " FPM", color: "#F4F7FA" },
                { tag: "SENSORS", value: strip.healthLevel >= 2 ? "CHECK" : "NOMINAL", color: strip.healthLevel >= 2 ? "#FF3B30" : strip.healthLevel === 1 ? "#FFB000" : "#58D330" },
                { tag: "NAV", value: strip.navigationValid ? "VALID" : "FAIL", color: strip.navigationValid ? "#58D330" : "#FF3B30" }
            ]
            delegate: Item {
                required property var modelData
                width: parent.width / 6
                height: parent.height
                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.tag
                    color: "#8299A8"
                    font.pixelSize: (strip.compact ? 9 : 11) * strip.uiScale
                    font.bold: true
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8 * strip.uiScale
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.value
                    color: modelData.color
                    font.pixelSize: (strip.compact ? 11 : 14) * strip.uiScale
                    font.bold: true
                }
            }
        }
    }
}
