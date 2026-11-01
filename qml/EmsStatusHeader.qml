import QtQuick

// EMS-only status header. No OAT/QNH is fabricated from a display constant.
// These flight-domain values require an explicit valid adapter before display.
Rectangle {
    id: root
    property real uiScale: 1
    property real textScale: 1
    property string dataState: "LOST"
    property int dataAgeMs: -1
    property string clockOverride: ""
    property string localClock: Qt.formatTime(new Date(), "hh:mm:ss")
    signal setupRequested()
    implicitHeight: Math.max(44, 44 * uiScale)
    color: "#D0061119"
    border.color: "#55646C"
    readonly property real gap: Math.max(8, 12 * uiScale)

    HmiTokens { id: tokens }
    Row {
        id: identity
        objectName: "emsHeaderIdentity"
        anchors { left: parent.left; leftMargin: root.gap; verticalCenter: parent.verticalCenter }
        spacing: root.gap
        Row {
            id: brand
            objectName: "emsHeaderBrand"
            Text { text: "EFIS"; color: "white"; font.bold: true
                font.pixelSize: tokens.legibleText(17, root.textScale, 13) }
            Text { text: "NEXT"; color: "#28D9F4"; font.bold: true
                font.pixelSize: tokens.legibleText(17, root.textScale, 13) }
        }
        Rectangle { width: 1; height: brand.height; color: "#55646C" }
        Text { text: "EMS"; color: "#28D9F4"
            font.pixelSize: tokens.legibleText(15, root.textScale, 12) }
    }
    Item {
        objectName: "emsHeaderSetupTarget"
        anchors { left: identity.left; top: parent.top; bottom: parent.bottom }
        width: Math.max(44, identity.width)
        TapHandler { longPressThreshold: 1.2; onLongPressed: root.setupRequested() }
    }
    Text {
        id: clock
        objectName: "emsHeaderClock"
        anchors { verticalCenter: parent.verticalCenter; right: parent.right; rightMargin: root.gap }
        text: (root.clockOverride.length ? root.clockOverride : root.localClock) + "  LOCAL"
        color: "#E8ECEE"
        font.pixelSize: tokens.legibleText(11, root.textScale, 9)
        font.bold: true
    }
    HmiDataStateBadge {
        id: stateBadge
        objectName: "emsHeaderState"
        anchors { verticalCenter: parent.verticalCenter; right: clock.left; rightMargin: root.gap }
        dataState: root.dataState
        ageMs: root.dataAgeMs
        uiScale: root.textScale
        showLive: true
    }
    Text {
        objectName: "emsHeaderTitle"
        anchors { left: identity.right; right: stateBadge.left; verticalCenter: parent.verticalCenter
            leftMargin: root.gap; rightMargin: root.gap }
        text: "ENGINE"
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        color: "white"
        font.pixelSize: tokens.legibleText(20, root.textScale, 16)
        font.bold: true
        font.letterSpacing: 1.2
    }
    Timer {
        interval: 1000
        running: root.clockOverride.length === 0
        repeat: true
        onTriggered: root.localClock = Qt.formatTime(new Date(), "hh:mm:ss")
    }
}
