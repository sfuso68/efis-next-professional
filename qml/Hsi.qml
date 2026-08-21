import QtQuick

Rectangle {
    id: hsi
    required property real heading
    required property real selectedCourse
    required property real cdiDeviation
    required property bool navTo
    property real uiScale: 1
    property bool valid: true
    property color cyan: "#00C8F0"

    width: 174 * uiScale; height: width; radius: width / 2
    color: "#071019"; opacity: 0.94
    border.color: valid ? "#9AAEBD" : "#FF3B30"; border.width: valid ? 2 : 3
    Item {
        anchors.fill: parent; rotation: -hsi.heading
        Repeater {
            model: [{t:"N",a:0},{t:"E",a:90},{t:"S",a:180},{t:"W",a:270}]
            delegate: Text {
                required property var modelData
                x: hsi.width/2 - width/2 + Math.sin(modelData.a*Math.PI/180) * hsi.width*0.38
                y: hsi.height/2 - height/2 - Math.cos(modelData.a*Math.PI/180) * hsi.height*0.38
                text: modelData.t; color: "#F4F7FA"; font.bold: true; font.pixelSize: 15 * hsi.uiScale
                rotation: hsi.heading
            }
        }
    }
    Rectangle { visible: hsi.valid; anchors.horizontalCenter: parent.horizontalCenter; y: 12 * hsi.uiScale; width: 3; height: hsi.height - 24 * hsi.uiScale; color: hsi.cyan; rotation: hsi.selectedCourse - hsi.heading; transformOrigin: Item.Center }
    Rectangle { visible: hsi.valid; width: 5 * hsi.uiScale; height: hsi.height * 0.48; color: "#D829FF"; anchors.centerIn: parent; transform: Translate { x: hsi.cdiDeviation * 14 * hsi.uiScale } }
    Text { anchors.horizontalCenter: parent.horizontalCenter; y: 2 * hsi.uiScale; text: "▼"; color: "#F4F7FA"; font.pixelSize: 18 * hsi.uiScale }
    Text { anchors.centerIn: parent; text: hsi.valid ? Math.round(hsi.heading) + "°" : "NAV"; color: hsi.valid ? "#F4F7FA" : "#FF3B30"; font.pixelSize: 25 * hsi.uiScale; font.bold: true }
    Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 13 * hsi.uiScale; text: hsi.valid ? (hsi.navTo ? "TO" : "FROM") : "FAIL"; color: hsi.valid ? hsi.cyan : "#FF3B30"; font.pixelSize: 12 * hsi.uiScale; font.bold: !hsi.valid }
}
