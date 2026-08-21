import QtQuick

Rectangle {
    id: fma
    property real uiScale: 1
    property bool compact: false
    property bool flightDirectorActive: false
    property real selectedAirspeed: 0
    property real selectedAltitude: 0
    property real selectedCourse: 0
    property bool navigationValid: true

    color: "#09141E"
    border.color: "#334A59"
    border.width: 1

    Row {
        anchors.fill: parent
        Repeater {
            model: [
                { label: "AP/FD", value: fma.flightDirectorActive ? "FD" : "OFF", active: fma.flightDirectorActive },
                { label: "LATERAL", value: "HDG " + Math.round(fma.selectedCourse), active: true },
                { label: "VERTICAL", value: "ALT " + Math.round(fma.selectedAltitude), active: true },
                { label: "SPEED", value: Math.round(fma.selectedAirspeed) + " KT", active: true },
                { label: "NAV", value: fma.navigationValid ? "ARM" : "FAIL", active: fma.navigationValid }
            ]
            delegate: Item {
                required property var modelData
                width: parent.width / 5
                height: parent.height
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: "#334A59" }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 2 * fma.uiScale
                    text: modelData.label
                    color: "#8299A8"
                    font.pixelSize: (fma.compact ? 8 : 10) * fma.uiScale
                    font.bold: true
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3 * fma.uiScale
                    text: modelData.value
                    color: modelData.active ? "#58D330" : "#FFB000"
                    font.pixelSize: (fma.compact ? 12 : 15) * fma.uiScale
                    font.bold: true
                }
            }
        }
    }
}
