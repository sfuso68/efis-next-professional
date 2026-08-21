import QtQuick

Rectangle {
    id: tape
    required property string label
    required property real value
    required property string unit
    required property real step
    required property real selectedValue
    property real trend: 0
    property real uiScale: 1
    property bool valid: true
    property color accent: "#00C8F0"

    color: "#0B1722"
    border.color: valid ? "#547080" : "#FF3B30"
    border.width: valid ? 1 : 3

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 18 * tape.uiScale
        text: tape.label + " • " + tape.unit
        color: "#9AAEBD"; font.pixelSize: 15 * tape.uiScale; font.bold: true
    }
    Repeater {
        model: 9
        delegate: Item {
            required property int index
            width: tape.width; height: 1
            y: tape.height / 2 + (index - 4) * 38 * tape.uiScale
            property real tickValue: Math.round(tape.value / tape.step) * tape.step - (index - 4) * tape.step
            Rectangle { anchors.right: parent.right; width: index % 2 === 0 ? 18 * tape.uiScale : 11 * tape.uiScale; height: 2; color: "#9AAEBD" }
            Text { anchors.right: parent.right; anchors.rightMargin: 24 * tape.uiScale; anchors.verticalCenter: parent.verticalCenter; text: Math.round(parent.tickValue); color: "#C9D5DD"; font.pixelSize: 12 * tape.uiScale }
        }
    }
    Rectangle {
        visible: tape.valid
        width: 12 * tape.uiScale; height: 18 * tape.uiScale; color: tape.accent
        anchors.right: parent.right
        y: Math.max(44 * tape.uiScale, Math.min(tape.height - 62 * tape.uiScale,
           tape.height / 2 - (tape.selectedValue - tape.value) / tape.step * 38 * tape.uiScale - height/2))
    }
    Rectangle {
        visible: tape.valid && Math.abs(tape.trend) > tape.step * 0.05
        anchors.horizontalCenter: parent.horizontalCenter
        x: tape.width * 0.57
        y: tape.height / 2 - Math.max(0, tape.trend / tape.step * 38 * tape.uiScale)
        width: 4 * tape.uiScale
        height: Math.max(5 * tape.uiScale, Math.min(tape.height * 0.28, Math.abs(tape.trend / tape.step * 38 * tape.uiScale)))
        color: "#D829FF"
        Rectangle { width: 13 * tape.uiScale; height: 4 * tape.uiScale; color: parent.color; anchors.horizontalCenter: parent.horizontalCenter; anchors.top: tape.trend >= 0 ? parent.top : undefined; anchors.bottom: tape.trend < 0 ? parent.bottom : undefined }
    }
    Rectangle {
        anchors.centerIn: parent; width: parent.width - 12 * tape.uiScale; height: 62 * tape.uiScale
        color: "#071019"; border.color: tape.valid ? "#F4F7FA" : "#FF3B30"; border.width: 2
        Text { anchors.centerIn: parent; text: tape.valid ? Math.round(tape.value) : "---"; color: tape.valid ? "#F4F7FA" : "#FF3B30"; font.pixelSize: 38 * tape.uiScale; font.bold: true }
    }
    Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 12 * tape.uiScale; text: tape.valid ? "SIM • 6S TREND" : "FAIL"; color: tape.valid ? tape.accent : "#FF3B30"; font.pixelSize: 12 * tape.uiScale; font.bold: !tape.valid }
}
