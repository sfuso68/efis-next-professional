import QtQuick

Rectangle {
    id: tape
    required property real heading
    required property real selectedCourse
    property real uiScale: 1
    property bool valid: true

    color: "#071019"
    opacity: 0.92
    border.color: valid ? "#547080" : "#FF3B30"
    clip: true

    Repeater {
        model: 13
        delegate: Item {
            required property int index
            property real offsetDeg: (index - 6) * 5
            property real tickHeading: (Math.round(tape.heading / 5) * 5 + offsetDeg + 360) % 360
            x: tape.width / 2 + offsetDeg * tape.width / 60 - 1
            width: 2; height: tape.height
            Rectangle { width: 2; height: index % 2 === 0 ? 10 * tape.uiScale : 6 * tape.uiScale; color: "#C9D5DD" }
            Text {
                visible: index % 2 === 0
                anchors.horizontalCenter: parent.horizontalCenter
                y: 11 * tape.uiScale
                text: parent.tickHeading === 0 ? "N" : parent.tickHeading === 90 ? "E" : parent.tickHeading === 180 ? "S" : parent.tickHeading === 270 ? "W" : Math.round(parent.tickHeading / 10)
                color: "#F4F7FA"; font.bold: true; font.pixelSize: 12 * tape.uiScale
            }
        }
    }
    Rectangle {
        visible: tape.valid
        width: 3 * tape.uiScale; height: 15 * tape.uiScale; color: "#00C8F0"
        x: tape.width / 2 + ((tape.selectedCourse - tape.heading + 540) % 360 - 180) * tape.width / 60 - width / 2
    }
    Text { anchors.horizontalCenter: parent.horizontalCenter; y: -5 * tape.uiScale; text: "▼"; color: "#F4F7FA"; font.pixelSize: 17 * tape.uiScale }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom
        width: 58 * tape.uiScale; height: 23 * tape.uiScale; color: "#071019"; border.color: "#F4F7FA"
        Text { anchors.centerIn: parent; text: tape.valid ? ("000" + Math.round(tape.heading)).slice(-3) + "°" : "HDG"; color: tape.valid ? "#F4F7FA" : "#FF3B30"; font.pixelSize: 14 * tape.uiScale; font.bold: true }
    }
}
