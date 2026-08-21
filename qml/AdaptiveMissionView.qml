import QtQuick

Item {
    id: root
    required property var controller
    property real uiScale: Math.min(width/1024, height/600)
    CommandRail { id: rail; anchors { left: parent.left; right: parent.right; top: parent.top } height: 54*root.uiScale; uiScale: root.uiScale; viewName: "MISSION VIEW"; healthLevel: controller.healthLevel; alertText: controller.annunciation; alertSeverity: controller.annunciationSeverity; flightDirectorActive: controller.flightDirectorActive; selectedAltitude: controller.selectedAltitude; blackBoxState: controller.blackBoxState; blackBoxRecords: controller.blackBoxRecords }
    Rectangle {
        id: miniPfd; anchors { left: parent.left; top: rail.bottom; bottom: dock.top } width: 270*root.uiScale
        color: "#0A151F"; border.color: "#314654"; clip: true
        Text { x: 12*root.uiScale; y: 10*root.uiScale; text: "INVARIANT MINI-PFD"; color: "#19C3E6"; font.pixelSize: 10*root.uiScale; font.bold: true }
        Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top; bottom: navBlock.top; margins: 12*root.uiScale; topMargin: 38*root.uiScale } color: "#166FA5"; clip: true
            Rectangle { y: parent.height/2 + controller.pitch*3*root.uiScale; width: parent.width; height: parent.height; color: "#71502D"; rotation: -controller.roll; transformOrigin: Item.Top }
            Repeater { model: [-20,-10,10,20]
                delegate: Item { required property int modelData; width: 72*root.uiScale; height: 10*root.uiScale;
                    x: (parent.width-width)/2; y: parent.height/2 + modelData*2.2*root.uiScale
                    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 54*root.uiScale; height: 1*root.uiScale; color: "#DDF2F0E9" }
                    Text { anchors { right: parent.left; rightMargin: 3*root.uiScale; verticalCenter: parent.verticalCenter }
                        text: Math.abs(modelData); color: "#F2F0E9"; font.pixelSize: 7*root.uiScale; font.bold: true }
                    Text { anchors { left: parent.right; leftMargin: 3*root.uiScale; verticalCenter: parent.verticalCenter }
                        text: Math.abs(modelData); color: "#F2F0E9"; font.pixelSize: 7*root.uiScale; font.bold: true }
                }
            }
            Row { anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 7*root.uiScale } spacing: 12*root.uiScale
                Repeater { model: ["30","20","10","▲","10","20","30"]
                    delegate: Text { required property string modelData; text: modelData; color: modelData === "▲" ? "#F2D13D" : "#E5E8E8";
                        font.pixelSize: (modelData === "▲" ? 10 : 6)*root.uiScale; font.bold: true }
                }
            }
            Rectangle { anchors { left: parent.left; verticalCenter: parent.verticalCenter } width: 56*root.uiScale; height: 40*root.uiScale;
                color: "#D407111A"; border.color: "#8AA0AD"
                Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 3*root.uiScale } text: "IAS"; color: "#7F929E"; font.pixelSize: 6*root.uiScale; font.bold: true }
                Text { anchors.centerIn: parent; text: Math.round(controller.airspeed); color: "#F2F0E9"; font.pixelSize: 18*root.uiScale; font.bold: true }
                Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 2*root.uiScale } text: "KT"; color: "#19C3E6"; font.pixelSize: 6*root.uiScale; font.bold: true }
            }
            Rectangle { anchors { right: parent.right; verticalCenter: parent.verticalCenter } width: 63*root.uiScale; height: 40*root.uiScale;
                color: "#D407111A"; border.color: "#8AA0AD"
                Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 3*root.uiScale } text: "ALT"; color: "#7F929E"; font.pixelSize: 6*root.uiScale; font.bold: true }
                Text { anchors.centerIn: parent; text: Math.round(controller.altitude); color: "#F2F0E9"; font.pixelSize: 16*root.uiScale; font.bold: true }
                Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 2*root.uiScale } text: "FT"; color: "#19C3E6"; font.pixelSize: 6*root.uiScale; font.bold: true }
            }
            Rectangle { anchors.centerIn: parent; width: 95*root.uiScale; height: 3*root.uiScale; color: "#F2D13D" }
            Rectangle { anchors.centerIn: parent; width: 3*root.uiScale; height: 13*root.uiScale; color: "#F2D13D" }
            Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 6*root.uiScale }
                text: "HDG  " + Math.round(controller.heading) + "°   •   SIM"; color: "#F2F0E9"; font.pixelSize: 8*root.uiScale; font.bold: true }
        }
        Rectangle { id: navBlock; anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12*root.uiScale } height: 82*root.uiScale; color: "#0D1A25"; border.color: "#314654"
            Text { x: 10*root.uiScale; y: 9*root.uiScale; text: "HDG  "+Math.round(controller.heading)+"°"; color: "#F2F0E9"; font.pixelSize: 15*root.uiScale; font.bold: true }
            Text { x: 10*root.uiScale; y: 34*root.uiScale; text: "GS   "+Math.round(controller.groundSpeed)+" KT"; color: "#F2F0E9"; font.pixelSize: 15*root.uiScale; font.bold: true }
            Text { x: 10*root.uiScale; anchors.bottom: parent.bottom; anchors.bottomMargin: 7*root.uiScale; text: "SOURCE • SIM"; color: "#19C3E6"; font.pixelSize: 8*root.uiScale; font.bold: true }
        }
    }
    GpuMapSurface {
        anchors { left: miniPfd.right; right: prediction.left; top: rail.bottom; bottom: dock.top }
        uiScale: root.uiScale; heading: controller.heading
    }
    Rectangle {
        id: prediction; anchors { right: parent.right; top: rail.bottom; bottom: dock.top } width: 220*root.uiScale
        color: "#0A151F"; border.color: "#314654"
        Column { anchors { fill: parent; margins: 12*root.uiScale } spacing: 12*root.uiScale
            Repeater { model: [
                {t:"NEXT WAYPOINT",v:"NOT AVAILABLE",s:"NO NAV DB"},
                {t:"VERTICAL PROFILE",v:"NOT AVAILABLE",s:"NO FLIGHT PLAN"},
                {t:"DESTINATION WX",v:"NOT AVAILABLE",s:"NO DATALINK"},
                {t:"GLIDE REACH",v:"UNVERIFIED",s:"AIRCRAFT DATA NEEDED"},
                {t:"DIVERSION",v:"UNAVAILABLE",s:"NO NAV DB"}
            ]; delegate: Rectangle { required property var modelData; width: parent.width; height: 67*root.uiScale; color: "#0D1A25"; border.color: "#314654"
                    Rectangle { width: 3*root.uiScale; height: parent.height; color: "#E6A72E" }
                    Rectangle { anchors { right: parent.right; top: parent.top; margins: 7*root.uiScale } width: 7*root.uiScale; height: 7*root.uiScale;
                        radius: width/2; color: "#E6A72E" }
                    Text { x: 8*root.uiScale; y: 7*root.uiScale; text: modelData.t; color: "#7F929E"; font.pixelSize: 8*root.uiScale; font.bold: true }
                    Text { x: 8*root.uiScale; y: 25*root.uiScale; text: modelData.v; color: "#E6A72E"; font.pixelSize: 12*root.uiScale; font.bold: true }
                    Text { x: 8*root.uiScale; anchors.bottom: parent.bottom; anchors.bottomMargin: 6*root.uiScale; text: modelData.s; color: "#7F929E"; font.pixelSize: 7*root.uiScale; font.bold: true }
            } }
        }
    }
    MissionDock { id: dock; anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 106*root.uiScale; uiScale: root.uiScale; groundSpeed: controller.groundSpeed; heading: controller.heading; navigationValid: controller.navigationValid; engineAvailable: true; engineRpm: controller.engineRpm; fuelTotal: controller.fuelLeft + controller.fuelRight; busVoltage: controller.busVoltage; engineHealth: controller.engineHealth }
}
