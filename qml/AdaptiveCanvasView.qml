import QtQuick

Item {
    id: root
    required property var controller
    property real uiScale: Math.min(width/1024, height/600)
    readonly property var lsaSpeedProfile: controller.aircraftSpeedProfile
    Theme { id: theme }
    Rectangle { anchors.fill: parent; color: theme.background }

    ProfessionalFma { id: fma; anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 58*root.uiScale; uiScale: root.uiScale; controller: root.controller }

    ProfessionalVisionSurface { id: vision; z: 0; anchors { left: parent.left; right: context.left; top: fma.bottom; bottom: deck.top }
        selectedAirport: root.controller.selectedAirport
        uiScale: root.uiScale; roll: controller.roll; pitch: controller.pitch; heading: controller.heading; latitude: controller.latitude; longitude: controller.longitude; altitude: controller.altitude; groundSpeed: controller.groundSpeed;
        selectedCourse: controller.selectedCourse; commandedRoll: controller.commandedRoll;
        commandedPitch: controller.commandedPitch; flightDirectorActive: controller.flightDirectorActive;
        navigationValid: controller.navigationValid
        leftInstrumentInset: 118*root.uiScale; rightInstrumentInset: 120*root.uiScale
        imageryState: controller.imageryState; imageryOfflineReady: controller.imageryOfflineReady
        imageryTileCount: controller.imageryTileCount; imageryCoverageGaps: controller.imageryCoverageGaps }

    ProfessionalTape { id: speed; z: 2; anchors { left: parent.left; top: fma.bottom; topMargin: 12*root.uiScale; bottom: deck.top; bottomMargin: 12*root.uiScale }
        width: 118*root.uiScale; uiScale: root.uiScale; role: "IAS"; value: controller.airspeed;
        speedProfile: root.lsaSpeedProfile;
        selectedValue: controller.selectedAirspeed; trend: controller.airspeedTrend; valid: controller.airspeedValid }

    ProfessionalTape { id: altitude; z: 2; anchors { right: context.left; top: fma.bottom; topMargin: 12*root.uiScale; bottom: deck.top; bottomMargin: 12*root.uiScale }
        width: 120*root.uiScale; uiScale: root.uiScale; role: "ALT"; value: controller.altitude;
        selectedValue: controller.selectedAltitude; trend: controller.altitudeTrend; valid: controller.altitudeValid;
        verticalSpeed: controller.verticalSpeed }

    Rectangle {
        id: context
        anchors { right: parent.right; top: fma.bottom; bottom: deck.top }
        width: 194*root.uiScale; color: "#050D14"; border.color: theme.border
        Column { anchors { fill: parent; margins: 6*root.uiScale } spacing: 6*root.uiScale
            Rectangle { width: parent.width; height: 139*root.uiScale; color: theme.surface; border.color: theme.borderBright; clip: true
                GpuMapSurface { anchors.fill: parent; heading: controller.heading; uiScale: root.uiScale*.62 }
                Rectangle { anchors { left: parent.left; top: parent.top; margins: 5*root.uiScale } width: 66*root.uiScale; height: 18*root.uiScale; color: "#E6050D14"; border.color: theme.border
                    Text { anchors.centerIn: parent; text: "MAP • SIM"; color: theme.cyan; font.pixelSize: 6*root.uiScale; font.bold: true }
                }
            }
            Rectangle { width: parent.width; height: 63*root.uiScale; color: theme.surface; border.color: theme.borderBright
                Text { x: 7*root.uiScale; y: 5*root.uiScale; text: "DESTINATION • SIM"; color: theme.secondary; font.pixelSize: 6*root.uiScale; font.bold: true }
                Text { x: 7*root.uiScale; y: 20*root.uiScale; text: "SIM04"; color: theme.magenta; font.pixelSize: 13*root.uiScale; font.bold: true }
                Text { x: 7*root.uiScale; y: 40*root.uiScale; text: "DTK 273°   DIS 9 NM"; color: theme.primary; font.pixelSize: 7*root.uiScale; font.bold: true }
                Text { anchors { right: parent.right; rightMargin: 7*root.uiScale; bottom: parent.bottom; bottomMargin: 7*root.uiScale } text: "ETE 04:12"; color: theme.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
            }
            Rectangle { width: parent.width; height: 158*root.uiScale; color: theme.surface; border.color: theme.borderBright
                Text { x: 7*root.uiScale; y: 5*root.uiScale; text: "TRAFFIC • SIM"; color: theme.secondary; font.pixelSize: 6*root.uiScale; font.bold: true }
                Item { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 6*root.uiScale } width: 132*root.uiScale; height: 132*root.uiScale
                    Repeater { model: [1,.66,.33]; delegate: Rectangle { required property real modelData; anchors.centerIn: parent; width: parent.width*modelData; height: width; radius: width/2; color: "transparent"; border.color: "#536A76"; border.width: 1 } }
                    Rectangle { anchors.centerIn: parent; width: 2*root.uiScale; height: parent.height; color: "#304753" }
                    Rectangle { anchors.centerIn: parent; width: parent.width; height: 2*root.uiScale; color: "#304753" }
                    Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top } text: "N"; color: theme.primary; font.pixelSize: 7*root.uiScale; font.bold: true }
                    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom } text: "▲"; color: theme.cyan; font.pixelSize: 13*root.uiScale }
                    Repeater { model: [{x:.68,y:.25,c:theme.amber,t:"+09"},{x:.24,y:.62,c:theme.warning,t:"-07"},{x:.74,y:.72,c:theme.amber,t:"-12"}]
                        delegate: Item { required property var modelData; x: parent.width*modelData.x; y: parent.height*modelData.y
                            Rectangle { width: 9*root.uiScale; height: 9*root.uiScale; rotation: 45; color: modelData.c }
                            Text { x: 11*root.uiScale; y: -2*root.uiScale; text: modelData.t; color: modelData.c; font.pixelSize: 6*root.uiScale; font.bold: true }
                        }
                    }
                }
            }
        }
    }

    ProfessionalSystemsDeck { id: deck; engineDataState: controller.engineDataState; anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 140*root.uiScale; uiScale: root.uiScale; controller: root.controller }
}
