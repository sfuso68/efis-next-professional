import QtQuick

Item {
    id: root
    required property var controller
    property real uiScale: Math.min(width/1024, height/600)
    signal autoRevertRequested()
    Timer { interval: 3000; running: root.visible; repeat: false; onTriggered: root.autoRevertRequested() }
    Rectangle { anchors.fill: parent; color: "#061018" }
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 54*root.uiScale; color: "#08121B"; border.color: "#314654"
        Text { x: 14*root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "EFIS <font color='#19C3E6'>NEXT</font>"; textFormat: Text.RichText; color: "#F2F0E9"; font.pixelSize: 20*root.uiScale; font.bold: true }
        Text { anchors.centerIn: parent; text: "FPV VISION • VERIFYING SOURCE"; color: "#E6A72E"; font.pixelSize: 15*root.uiScale; font.bold: true }
        Text { anchors.right: parent.right; anchors.rightMargin: 14*root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "AUTO REVERT ARMED"; color: "#62D347"; font.pixelSize: 11*root.uiScale; font.bold: true }
    }
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: health.top; margins: 10*root.uiScale; topMargin: 64*root.uiScale }
        color: "#090D11"; border.color: "#E6A72E"; border.width: 2
        Text { anchors.centerIn: parent; text: "NO VALID VIDEO SOURCE\nCAM 1–4 • NOT INSTALLED"; horizontalAlignment: Text.AlignHCenter; color: "#E6A72E"; font.pixelSize: 28*root.uiScale; font.bold: true }
        Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 24*root.uiScale } text: "FLIGHT DATA REMAINS AVAILABLE • RETURNING TO PFD"; color: "#F2F0E9"; font.pixelSize: 12*root.uiScale; font.bold: true }
    }
    Rectangle {
        id: health; anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 10*root.uiScale }
        height: 96*root.uiScale; color: "#0A151F"; border.color: "#314654"
        Row { anchors.fill: parent
            Repeater { model: [
                {t:"CAMERAS",v:"NOT INSTALLED"},{t:"VIDEO LATENCY",v:"UNAVAILABLE"},{t:"PACKET LOSS",v:"UNAVAILABLE"},{t:"GIMBAL",v:"NOT INSTALLED"},{t:"BLACK BOX",v:controller.blackBoxState},{t:"FAIL-SAFE",v:"AUTO REVERT PFD"}
            ]; delegate: Item { required property var modelData; width: parent.width/6; height: parent.height
                    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: "#314654" }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; y: 17*root.uiScale; text: modelData.t; color: "#7F929E"; font.pixelSize: 9*root.uiScale; font.bold: true }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; y: 45*root.uiScale; text: modelData.v; color: modelData.t === "FAIL-SAFE" ? "#62D347" : "#E6A72E"; font.pixelSize: 10*root.uiScale; font.bold: true }
            } }
        }
    }
}
