pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root
    required property var controller
    property real uiScale: 1
    readonly property color red: "#FF3B30"
    readonly property color amber: "#F5B642"
    readonly property color cyan: "#35D5E5"
    Theme { id: theme }

    clip: true

    Rectangle { anchors.fill: parent; color: "#07131D"; border.color: "#506572"; radius: 5*root.uiScale }

    Terrain3DSurface {
        id: terrain
        anchors { fill: parent; margins: 2*root.uiScale }
        visible: root.controller.positionValid && root.controller.altitudeValid
                 && root.controller.attitudeValid && root.controller.headingValid
        opacity: 0.82
        roll: root.controller.roll
        pitch: root.controller.pitch
        heading: root.controller.heading
        latitude: root.controller.latitude
        longitude: root.controller.longitude
        altitude: root.controller.altitude
    }

    // Fail-closed fallback: no position or altitude means no invented terrain.
    Item {
        anchors { fill: parent; margins: 2*root.uiScale }
        visible: !terrain.visible
        clip: true
        Rectangle { anchors.fill: parent; color: "#1A5C91" }
        Item {
            width: parent.width*2.4; height: parent.height*2.4
            anchors.centerIn: parent
            visible: root.controller.attitudeValid
            transform: Translate { y: root.controller.pitch*3.0*root.uiScale }
            rotation: root.controller.attitudeValid ? -root.controller.roll : 0
            Rectangle { x: 0; y: parent.height/2; width: parent.width; height: parent.height/2; color: "#59402B" }
            Rectangle { x: 0; y: parent.height/2-1*root.uiScale; width: parent.width; height: 2*root.uiScale; color: "#EEF4F5" }
        }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 42*root.uiScale
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#EE02070B" }
            GradientStop { position: 1.0; color: "#1002070B" }
        }
    }

    Item {
        id: headingRibbon
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 3*root.uiScale }
        width: 260*root.uiScale; height: 39*root.uiScale
        Repeater {
            model: 13
            delegate: Item {
                id: headingMark
                required property int index
                property real delta: (headingMark.index-6)*5
                x: headingRibbon.width/2 + headingMark.delta*4.0*root.uiScale
                y: 24*root.uiScale
                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 1*root.uiScale; height: headingMark.index%2===0 ? 9*root.uiScale : 5*root.uiScale; color: "#E7EEF0" }
                Text {
                    visible: headingMark.index%2===0
                    anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.top; bottomMargin: -22*root.uiScale }
                    text: root.controller.headingValid ? ("000"+Math.round((root.controller.heading+headingMark.delta+360)%360)).slice(-3) : "---"
                    color: "white"; font.pixelSize: 7*root.uiScale; font.bold: true
                }
            }
        }
        Rectangle {
            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top }
            width: 58*root.uiScale; height: 23*root.uiScale; radius: 3*root.uiScale
            color: "#D9040A0F"; border.color: root.cyan
            Text { anchors.centerIn: parent; text: root.controller.headingValid ? ("000"+Math.round(root.controller.heading)).slice(-3)+"°" : "HDG ---"; color: root.controller.headingValid ? "white" : root.red; font.pixelSize: 11*root.uiScale; font.bold: true }
        }
        Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 19*root.uiScale } text: "▼"; color: root.cyan; font.pixelSize: 11*root.uiScale; font.bold: true }
    }

    Item {
        anchors.fill: parent
        visible: root.controller.attitudeValid
        rotation: root.controller.attitudeValid ? -root.controller.roll : 0
        transform: Translate { y: root.controller.pitch*2.8*root.uiScale }
        Repeater {
            model: [-15,-10,-5,5,10,15]
            delegate: Item {
                id: pitchMark
                required property int modelData
                x: parent.width/2-width/2
                y: parent.height*.47-pitchMark.modelData*2.8*root.uiScale
                width: Math.abs(pitchMark.modelData)%10===0 ? 88*root.uiScale : 48*root.uiScale
                height: 2*root.uiScale
                Rectangle { anchors.fill: parent; color: "#EEF4F5"; opacity: 0.9 }
                Text { anchors { right: parent.left; rightMargin: 5*root.uiScale; verticalCenter: parent.verticalCenter } text: Math.abs(pitchMark.modelData); color: "white"; font.pixelSize: 7*root.uiScale; font.bold: true }
                Text { anchors { left: parent.right; leftMargin: 5*root.uiScale; verticalCenter: parent.verticalCenter } text: Math.abs(pitchMark.modelData); color: "white"; font.pixelSize: 7*root.uiScale; font.bold: true }
            }
        }
    }

    Item {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 8*root.uiScale
        width: 150*root.uiScale; height: 34*root.uiScale
        Rectangle { x: 0; y: 16*root.uiScale; width: 58*root.uiScale; height: 4*root.uiScale; color: "#F4D63D" }
        Rectangle { x: 92*root.uiScale; y: 16*root.uiScale; width: 58*root.uiScale; height: 4*root.uiScale; color: "#F4D63D" }
        Rectangle { anchors.centerIn: parent; width: 11*root.uiScale; height: 11*root.uiScale; radius: width/2; color: "transparent"; border.color: "#F4D63D"; border.width: 3*root.uiScale }
    }

    Rectangle {
        visible: !root.controller.attitudeValid
        anchors.centerIn: parent
        width: 182*root.uiScale; height: 38*root.uiScale; radius: 4*root.uiScale
        color: "#E22A0808"; border.color: root.red; border.width: 2*root.uiScale
        Text { anchors.centerIn: parent; text: "ATTITUDE INVALID"; color: "white"; font.pixelSize: 12*root.uiScale; font.bold: true }
    }

    Rectangle {
        anchors { left: parent.left; leftMargin: 8*root.uiScale; verticalCenter: parent.verticalCenter }
        width: 78*root.uiScale; height: 70*root.uiScale; radius: 4*root.uiScale
        color: "#D8050D14"; border.color: root.controller.airspeedValid ? root.cyan : root.red
        Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 6*root.uiScale } text: "IAS"; color: root.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
        Text { anchors.centerIn: parent; text: root.controller.airspeedValid ? Math.round(root.controller.airspeed) : "---"; color: root.controller.airspeedValid ? "white" : root.red; font.pixelSize: 24*root.uiScale; font.bold: true }
        Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 5*root.uiScale } text: "KT"; color: "#B4C3C9"; font.pixelSize: 7*root.uiScale }
    }

    Rectangle {
        anchors { right: parent.right; rightMargin: 8*root.uiScale; verticalCenter: parent.verticalCenter }
        width: 86*root.uiScale; height: 70*root.uiScale; radius: 4*root.uiScale
        color: "#D8050D14"; border.color: root.controller.altitudeValid ? root.cyan : root.red
        Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 6*root.uiScale } text: "ALT FT"; color: root.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
        Text { anchors.centerIn: parent; text: root.controller.altitudeValid ? Math.round(root.controller.altitude) : "---"; color: root.controller.altitudeValid ? "white" : root.red; font.pixelSize: 20*root.uiScale; font.bold: true }
        Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 5*root.uiScale } text: root.controller.verticalSpeedValid ? "VS "+Math.round(root.controller.verticalSpeed) : "VS ---"; color: root.controller.verticalSpeedValid ? "#D6E1E4" : root.red; font.pixelSize: 7*root.uiScale; font.bold: true }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 31*root.uiScale; color: "#E9050C12"
        Text { anchors { left: parent.left; leftMargin: 9*root.uiScale; verticalCenter: parent.verticalCenter } text: "ROLL "+(root.controller.attitudeValid ? Number(root.controller.roll).toFixed(0)+"°" : "---")+"   PITCH "+(root.controller.attitudeValid ? Number(root.controller.pitch).toFixed(0)+"°" : "---"); color: root.controller.attitudeValid ? "white" : root.red; font.pixelSize: 8*root.uiScale; font.bold: true }
        Text {
            anchors { right: parent.right; rightMargin: 9*root.uiScale; verticalCenter: parent.verticalCenter }
            text: terrain.visible ? "SKYVIEW • TERRAIN DISPLAY • NON-CERT"
                                  : (root.controller.attitudeValid ? "ATTITUDE ONLY • TERRAIN GATED"
                                                                   : "FLIGHT ATTITUDE INVALID")
            color: terrain.visible ? root.amber : root.red
            font.pixelSize: 7*root.uiScale; font.bold: true
        }
    }
}
