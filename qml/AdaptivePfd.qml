import QtQuick

Item {
    id: root
    property real uiScale: Math.min(width/1024, height/600)
    required property var controller
    signal navigationRequested()
    readonly property var lsaSpeedProfile: controller.aircraftSpeedProfile
    Theme { id: theme }

    Rectangle { anchors.fill: parent; color: theme.background }

    ProfessionalFma { id: fma; anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 58*root.uiScale; uiScale: root.uiScale; controller: root.controller }

    ProfessionalVisionSurface { id: vision; z: 0; anchors { left: parent.left; right: parent.right; top: fma.bottom; bottom: dock.top }
        selectedAirport: root.controller.selectedAirport
        uiScale: root.uiScale; roll: controller.roll; pitch: controller.pitch; heading: controller.heading; latitude: controller.latitude; longitude: controller.longitude; altitude: controller.altitude; groundSpeed: controller.groundSpeed;
        selectedCourse: controller.selectedCourse; commandedRoll: controller.commandedRoll;
        commandedPitch: controller.commandedPitch; flightDirectorActive: controller.flightDirectorActive;
        navigationValid: controller.navigationValid
        leftInstrumentInset: 120*root.uiScale; rightInstrumentInset: 128*root.uiScale
        imageryState: controller.imageryState; imageryOfflineReady: controller.imageryOfflineReady
        imageryTileCount: controller.imageryTileCount; imageryCoverageGaps: controller.imageryCoverageGaps
        onNavigationRequested: root.navigationRequested() }

    ProfessionalTape { id: speedTape; z: 2; anchors { left: parent.left; top: fma.bottom; topMargin: 12*root.uiScale; bottom: dock.top; bottomMargin: 12*root.uiScale }
        width: 120*root.uiScale; uiScale: root.uiScale; role: "IAS"; value: controller.airspeed;
        speedProfile: root.lsaSpeedProfile;
        selectedValue: controller.selectedAirspeed; trend: controller.airspeedTrend; valid: controller.airspeedValid }

    ProfessionalTape { id: altitudeTape; z: 2; anchors { right: parent.right; top: fma.bottom; topMargin: 12*root.uiScale; bottom: dock.top; bottomMargin: 12*root.uiScale }
        width: 128*root.uiScale; uiScale: root.uiScale; role: "ALT"; value: controller.altitude;
        selectedValue: controller.selectedAltitude; trend: controller.altitudeTrend; valid: controller.altitudeValid;
        verticalSpeed: controller.verticalSpeed }

    WeatherRadarOverlay {
        z: 3
        anchors { right: altitudeTape.left; top: fma.bottom; rightMargin: 8*root.uiScale; topMargin: 9*root.uiScale }
        width: 178*root.uiScale; height: 103*root.uiScale
        uiScale: root.uiScale; radarOpacity: 0.72
    }

    Rectangle { visible: false
        anchors { horizontalCenter: vision.horizontalCenter; bottom: dock.top; bottomMargin: 7*root.uiScale }
        width: 344*root.uiScale; height: 61*root.uiScale; radius: 3*root.uiScale
        color: "#E8040B11"; border.color: theme.borderBright
        Text { x: 10*root.uiScale; y: 6*root.uiScale; text: "NAV • SIM"; color: theme.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
        Text { x: 10*root.uiScale; y: 21*root.uiScale; text: "HDG  " + Math.round(controller.heading) + "°"; color: theme.primary; font.pixelSize: 13*root.uiScale; font.bold: true }
        Text { x: 10*root.uiScale; y: 42*root.uiScale; text: "GS  " + Math.round(controller.groundSpeed) + " KT"; color: theme.secondary; font.pixelSize: 8*root.uiScale; font.bold: true }
        Rectangle { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 8*root.uiScale }
            width: 145*root.uiScale; height: 1*root.uiScale; color: theme.primary }
        Rectangle { anchors.centerIn: parent; width: 3*root.uiScale; height: 25*root.uiScale; color: theme.magenta }
        Text { anchors { right: parent.right; rightMargin: 10*root.uiScale; top: parent.top; topMargin: 7*root.uiScale }
            text: "CRS  " + Math.round(controller.selectedCourse) + "°"; color: theme.magenta; font.pixelSize: 10*root.uiScale; font.bold: true }
        Text { anchors { right: parent.right; rightMargin: 10*root.uiScale; bottom: parent.bottom; bottomMargin: 7*root.uiScale }
            text: controller.navigationValid ? "CDI VALID" : "NAV INVALID"; color: controller.navigationValid ? theme.green : theme.amber; font.pixelSize: 7*root.uiScale; font.bold: true }
    }

    ProfessionalSystemsDeck { id: dock; anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 140*root.uiScale; uiScale: root.uiScale; controller: root.controller }
}
