import QtQuick

Rectangle {
    id: canvas
    required property real roll
    required property real pitch
    required property real heading
    required property real selectedCourse
    required property real commandedRoll
    required property real commandedPitch
    required property bool flightDirectorActive
    required property bool navigationValid
    property real uiScale: 1
    color: "#166FA5"; clip: true; border.color: "#314654"
    Item {
        id: world; width: canvas.width * 2.5; height: canvas.height * 3; anchors.centerIn: parent
        rotation: -canvas.roll; transform: Translate { y: canvas.pitch * canvas.height / 42 }
        Rectangle { width: parent.width; height: parent.height / 2; color: "#166FA5" }
        Rectangle { y: parent.height / 2; width: parent.width; height: parent.height / 2; color: "#71502D" }
        Rectangle { y: parent.height / 2 - 2; width: parent.width; height: 4; color: "#F2F0E9" }
        Repeater {
            model: [-25,-20,-15,-10,-5,5,10,15,20,25]
            delegate: Item {
                required property int modelData
                x: world.width / 2 - width / 2; y: world.height / 2 - modelData * canvas.height / 42
                width: modelData % 10 === 0 ? 132 * canvas.uiScale : 78 * canvas.uiScale; height: 2
                Rectangle { anchors.fill: parent; color: "#F2F0E9" }
                Text { anchors.right: parent.left; anchors.rightMargin: 7; anchors.verticalCenter: parent.verticalCenter; text: Math.abs(modelData); color: "#F2F0E9"; font.pixelSize: 11 * canvas.uiScale; font.bold: true }
                Text { anchors.left: parent.right; anchors.leftMargin: 7; anchors.verticalCenter: parent.verticalCenter; text: Math.abs(modelData); color: "#F2F0E9"; font.pixelSize: 11 * canvas.uiScale; font.bold: true }
            }
        }
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 7 * canvas.uiScale }
        height: 42 * canvas.uiScale; color: "#08121B"; opacity: 0.93; border.color: "#314654"
        Repeater {
            model: 13
            delegate: Item {
                required property int index
                property real offset: (index - 6) * 5
                x: parent.width / 2 + offset * parent.width / 65; width: 2; height: parent.height
                Rectangle { width: 2; height: index % 2 === 0 ? 9 * canvas.uiScale : 5 * canvas.uiScale; color: "#F2F0E9" }
                Text { visible: index % 2 === 0; anchors.horizontalCenter: parent.horizontalCenter; y: 11 * canvas.uiScale; text: ("0" + Math.round(((Math.round(canvas.heading/5)*5 + parent.offset + 360)%360)/10)).slice(-2); color: "#F2F0E9"; font.pixelSize: 10 * canvas.uiScale; font.bold: true }
            }
        }
        Rectangle { visible: canvas.navigationValid; width: 3; height: 13 * canvas.uiScale; color: "#19C3E6"; x: parent.width/2 + ((canvas.selectedCourse-canvas.heading+540)%360-180)*parent.width/65 }
        Rectangle { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; width: 55 * canvas.uiScale; height: 21 * canvas.uiScale; color: "#08121B"; border.color: "#F2F0E9"; Text { anchors.centerIn: parent; text: ("000" + Math.round(canvas.heading)).slice(-3) + "°"; color: "#F2F0E9"; font.pixelSize: 12 * canvas.uiScale; font.bold: true } }
    }
    Item {
        anchors.centerIn: parent; width: 190 * canvas.uiScale; height: 70 * canvas.uiScale
        Rectangle { x: 0; y: 33 * canvas.uiScale; width: 70 * canvas.uiScale; height: 5 * canvas.uiScale; color: "#F2D13D" }
        Rectangle { x: 120 * canvas.uiScale; y: 33 * canvas.uiScale; width: 70 * canvas.uiScale; height: 5 * canvas.uiScale; color: "#F2D13D" }
        Rectangle { anchors.centerIn: parent; width: 10 * canvas.uiScale; height: 10 * canvas.uiScale; radius: width/2; color: "transparent"; border.color: "#F2D13D"; border.width: 3 }
    }
    Item {
        visible: canvas.flightDirectorActive; anchors.centerIn: parent
        x: (canvas.commandedRoll - canvas.roll) * 3.2 * canvas.uiScale; y: -(canvas.commandedPitch - canvas.pitch) * 4 * canvas.uiScale
        Rectangle { x: -82 * canvas.uiScale; width: 64 * canvas.uiScale; height: 5 * canvas.uiScale; color: "#ED3DEB" }
        Rectangle { x: 18 * canvas.uiScale; width: 64 * canvas.uiScale; height: 5 * canvas.uiScale; color: "#ED3DEB" }
    }
    Text { anchors { left: parent.left; bottom: parent.bottom; margins: 10 * canvas.uiScale } text: "FLIGHT CANVAS • SYNTHETIC TERRAIN NOT INSTALLED"; color: "#F2F0E9"; font.pixelSize: 9 * canvas.uiScale; font.bold: true }
    Text { anchors { right: parent.right; bottom: parent.bottom; margins: 10 * canvas.uiScale } text: "ENERGY • UNVERIFIED"; color: "#E6A72E"; font.pixelSize: 9 * canvas.uiScale; font.bold: true }
}
