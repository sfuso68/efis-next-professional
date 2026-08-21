import QtQuick

Rectangle {
    id: mfd
    property real uiScale: 1
    property real airspeed: 0
    property real altitude: 0
    property real groundSpeed: 0
    property real heading: 0
    property real verticalSpeed: 0
    property int healthLevel: 0
    property bool navigationValid: true

    color: "#071019"

    Text { x: 20 * mfd.uiScale; y: 14 * mfd.uiScale; text: "MULTI-FUNCTION DISPLAY"; color: "#F4F7FA"; font.pixelSize: 22 * mfd.uiScale; font.bold: true }
    Text { anchors.right: parent.right; anchors.rightMargin: 20 * mfd.uiScale; y: 18 * mfd.uiScale; text: "SYSTEMS • NAV • VIDEO"; color: "#00C8F0"; font.pixelSize: 14 * mfd.uiScale; font.bold: true }

    Grid {
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom; margins: 20 * mfd.uiScale; topMargin: 58 * mfd.uiScale }
        columns: 3; spacing: 10 * mfd.uiScale
        Repeater {
            model: [
                { title: "FLIGHT", line1: "IAS  " + Math.round(mfd.airspeed) + " KT", line2: "ALT  " + Math.round(mfd.altitude) + " FT", state: "LIVE", color: "#58D330" },
                { title: "NAVIGATION", line1: "HDG  " + Math.round(mfd.heading) + "°", line2: "GS   " + Math.round(mfd.groundSpeed) + " KT", state: mfd.navigationValid ? "VALID" : "FAIL", color: mfd.navigationValid ? "#58D330" : "#FF3B30" },
                { title: "AIR DATA", line1: "V/S  " + Math.round(mfd.verticalSpeed) + " FPM", line2: "BARO 1013 HPA", state: "SIM SOURCE", color: "#00C8F0" },
                { title: "MAP", line1: "NAV DATABASE", line2: "INTERFACE RESERVED", state: "READY FOR DATA", color: "#FFB000" },
                { title: "FPV / VIDEO", line1: "CAM 1  CAM 2", line2: "CAM 3  CAM 4", state: "PIPELINE RESERVED", color: "#FFB000" },
                { title: "PLATFORM", line1: "CORE  REC  DISPLAY", line2: "HEALTH LEVEL " + mfd.healthLevel, state: mfd.healthLevel >= 2 ? "CHECK" : "NOMINAL", color: mfd.healthLevel >= 2 ? "#FF3B30" : "#58D330" }
            ]
            delegate: Rectangle {
                required property var modelData
                width: (mfd.width - 60 * mfd.uiScale) / 3
                height: (mfd.height - 90 * mfd.uiScale) / 2
                color: "#0B1722"; border.color: "#334A59"; radius: 4 * mfd.uiScale
                Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 4 * mfd.uiScale; color: modelData.color }
                Text { x: 14 * mfd.uiScale; y: 16 * mfd.uiScale; text: modelData.title; color: "#9AAEBD"; font.pixelSize: 13 * mfd.uiScale; font.bold: true }
                Text { x: 14 * mfd.uiScale; y: 51 * mfd.uiScale; text: modelData.line1 + "\n" + modelData.line2; color: "#F4F7FA"; font.pixelSize: 19 * mfd.uiScale; font.bold: true; lineHeight: 1.45 }
                Text { x: 14 * mfd.uiScale; anchors.bottom: parent.bottom; anchors.bottomMargin: 13 * mfd.uiScale; text: modelData.state; color: modelData.color; font.pixelSize: 12 * mfd.uiScale; font.bold: true }
            }
        }
    }
}
