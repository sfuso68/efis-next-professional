import QtQuick

Rectangle {
    id: badge
    property string dataState: "LOST"
    property int ageMs: -1
    property real uiScale: 1
    property bool showLive: false
    readonly property bool isLive: dataState === "LIVE"
    readonly property bool shouldShow: showLive || !isLive
    visible: shouldShow
    implicitWidth: Math.max(46 * uiScale, label.implicitWidth + 14 * uiScale)
    implicitHeight: 22 * uiScale
    radius: 3 * uiScale
    color: "#D0061119"
    border.width: Math.max(1, 1.5 * uiScale)
    border.color: tokens.stateColor(dataState)

    HmiTokens { id: tokens }
    Text {
        id: label
        anchors.centerIn: parent
        text: badge.dataState + ((badge.dataState === "STALE" || badge.dataState === "DEGRADED" || badge.dataState === "LOST") && badge.ageMs >= 0
                             ? "  " + (badge.ageMs / 1000).toFixed(1) + "s" : "")
        color: tokens.stateColor(badge.dataState)
        font.pixelSize: Math.max(10, 11 * badge.uiScale)
        font.bold: true
        font.letterSpacing: 0.5 * badge.uiScale
    }
}
