import QtQuick

Rectangle {
    id: root
    width: 1024; height: 600
    color: tokens.background
    HmiTokens { id: tokens }
    Text { x: 24; y: 18; text: "AVIONIX NEXT  •  HMI CONTRACT V1"; color: tokens.foreground; font.pixelSize: 22; font.bold: true }
    Row {
        x: 24; y: 62; spacing: 20
        HmiArcGaugeV4 { width: 280; height: 280; label: "RPM"; value: 5280; unit: "RPM"; minimum: 0; maximum: 6000; greenStart: 1400; greenEnd: 5500; majorStep: 1000; minorStep: 200; dominant: true; dataState: "SIM" }
        HmiArcGaugeV4 { width: 220; height: 220; label: "OIL PRESS"; value: 4.8; unit: "BAR"; minimum: 0; maximum: 7; greenStart: 2; greenEnd: 5.8; majorStep: 1; minorStep: 0.2; decimals: 1; limitsVerified: false }
        Column {
            spacing: 8
            HmiDataStateBadge { dataState: "LIVE"; showLive: true }
            HmiDataStateBadge { dataState: "SIM" }
            HmiDataStateBadge { dataState: "STALE"; ageMs: 640 }
            HmiDataStateBadge { dataState: "LOST"; ageMs: 1800 }
        }
    }
    SkyViewCompassArc {
        x: 70; y: 350; width: 860; height: 230
        heading: 358; selectedCourse: 3; cdiDeviation: -0.25
        waypoint: "LIDB"; distanceNm: 12.4; ete: "06:18"; dataState: "SIM"
    }
}
