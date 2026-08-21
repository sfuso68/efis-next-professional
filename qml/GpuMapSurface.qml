import QtQuick
import EfisNext.Native

Rectangle {
    id: map
    required property real heading
    property real uiScale: 1
    Theme { id: theme }
    color: theme.background
    border.color: theme.border
    clip: true

    GpuMapItem { anchors.fill: parent; heading: map.heading }

    WeatherRadarOverlay {
        anchors.fill: parent
        uiScale: map.uiScale
        radarOpacity: 0.48
        z: 1
    }

    Rectangle {
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 7*map.uiScale }
        width: 204*map.uiScale; height: 31*map.uiScale; radius: 3*map.uiScale
        color: "#E608121A"; border.color: "#537184"
        Row {
            anchors.centerIn: parent; spacing: 13*map.uiScale
            Repeater {
                model: [-20, -10, 0, 10, 20]
                delegate: Text {
                    required property int modelData
                    text: {
                        var value = (Math.round(map.heading / 10) * 10 + modelData + 360) % 360
                        if (modelData === 0) return ("000" + value).slice(-3) + "°"
                        return ("000" + value).slice(-3)
                    }
                    color: modelData === 0 ? theme.primary : theme.secondary
                    font.pixelSize: (modelData === 0 ? 12 : 8)*map.uiScale
                    font.bold: modelData === 0
                }
            }
        }
        Rectangle { anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter }
            width: 2*map.uiScale; height: 6*map.uiScale; color: theme.cyan }
    }

    Repeater {
        model: [
            {x:0.10,y:0.88,n:"SIM01",d:"18"}, {x:0.27,y:0.70,n:"SIM02",d:"12"},
            {x:0.43,y:0.60,n:"SIM03",d:"6"}, {x:0.63,y:0.36,n:"SIM04",d:"9"},
            {x:0.82,y:0.15,n:"SIM05",d:"21"}
        ]
        delegate: Item {
            required property var modelData
            x: modelData.x*map.width-7*map.uiScale; y: modelData.y*map.height-7*map.uiScale
            Rectangle { anchors.centerIn: parent; width: 13*map.uiScale; height: 13*map.uiScale;
                rotation: 45; color: "#D9061018"; border.color: theme.cyan; border.width: 2 }
            Rectangle { x: 12*map.uiScale; y: -11*map.uiScale; width: 48*map.uiScale; height: 25*map.uiScale;
                radius: 2*map.uiScale; color: "#D9061018"; border.color: "#40596A"
                Text { anchors { left: parent.left; leftMargin: 5*map.uiScale; verticalCenter: parent.verticalCenter }
                    text: modelData.n+"  "+modelData.d; color: theme.primary;
                    font.pixelSize: 7.5*map.uiScale; font.bold: true }
            }
        }
    }

    Rectangle {
        anchors { left: parent.left; top: parent.top; margins: 9*map.uiScale }
        width: 116*map.uiScale; height: 52*map.uiScale; radius: 3*map.uiScale
        color: "#E60A151F"; border.color: "#537184"
        Text { x: 8*map.uiScale; y: 6*map.uiScale; text: "ACTIVE LEG • SIM"; color: theme.cyan; font.pixelSize: 7*map.uiScale; font.bold: true }
        Text { x: 8*map.uiScale; y: 20*map.uiScale; text: "SIM03  →  SIM04"; color: theme.primary; font.pixelSize: 11*map.uiScale; font.bold: true }
        Text { x: 8*map.uiScale; y: 37*map.uiScale; text: "BRG 032°   9 NM"; color: theme.secondary; font.pixelSize: 7*map.uiScale; font.bold: true }
    }

    Column {
        anchors { right: parent.right; top: parent.top; topMargin: 48*map.uiScale; rightMargin: 8*map.uiScale }
        spacing: 3*map.uiScale
        Repeater {
            model: [
                {n:"TERRAIN",s:"SIM",c:theme.green}, {n:"AIRSPACE",s:"OFF",c:theme.secondary},
                {n:"TRAFFIC",s:"OFF",c:theme.secondary}, {n:"WEATHER",s:"VMI",c:theme.cyan}
            ]
            delegate: Rectangle {
                required property var modelData
                width: 92*map.uiScale; height: 20*map.uiScale; radius: 2*map.uiScale
                color: "#E60A151F"; border.color: "#40596A"
                Rectangle { x: 6*map.uiScale; anchors.verticalCenter: parent.verticalCenter;
                    width: 5*map.uiScale; height: 5*map.uiScale; radius: width/2; color: modelData.c }
                Text { x: 16*map.uiScale; anchors.verticalCenter: parent.verticalCenter; text: modelData.n;
                    color: theme.primary; font.pixelSize: 7*map.uiScale; font.bold: true }
                Text { anchors { right: parent.right; rightMargin: 6*map.uiScale; verticalCenter: parent.verticalCenter }
                    text: modelData.s; color: modelData.c; font.pixelSize: 7*map.uiScale; font.bold: true }
            }
        }
    }

    Text { x: map.width*0.50 + 7*map.uiScale; y: map.height*0.38; text: "10"; color: "#7698A7"; font.pixelSize: 7*map.uiScale }
    Text { x: map.width*0.50 + 7*map.uiScale; y: map.height*0.22; text: "25"; color: "#7698A7"; font.pixelSize: 7*map.uiScale }
    Text { x: map.width*0.50 + 7*map.uiScale; y: 46*map.uiScale; text: "50 NM"; color: "#7698A7"; font.pixelSize: 7*map.uiScale }

    Rectangle {
        anchors { left: parent.left; bottom: parent.bottom; margins: 9*map.uiScale }
        width: 191*map.uiScale; height: 30*map.uiScale; radius: 3*map.uiScale
        color: "#E60A151F"; border.color: "#40596A"
        Text { x: 7*map.uiScale; y: 5*map.uiScale; text: "GPU VECTOR MAP • SYNTHETIC"; color: theme.cyan; font.pixelSize: 7*map.uiScale; font.bold: true }
        Text { x: 7*map.uiScale; y: 17*map.uiScale; text: "TRACK UP   •   RANGE 50 NM"; color: theme.secondary; font.pixelSize: 7*map.uiScale; font.bold: true }
    }

    Row {
        anchors { right: parent.right; bottom: parent.bottom; margins: 10*map.uiScale }
        spacing: 5*map.uiScale
        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 46*map.uiScale; height: 2*map.uiScale; color: theme.primary }
        Text { text: "10 NM"; color: theme.primary; font.pixelSize: 8*map.uiScale; font.bold: true }
    }
}
