import QtQuick

Item {
    id: gauge
    required property string label
    required property real value
    required property string unit
    required property real minimum
    required property real maximum
    required property real greenStart
    required property real greenEnd
    property real majorStep: (maximum - minimum) / 6
    property real minorStep: majorStep / 5
    property int decimals: 0
    property real uiScale: 1
    property bool dominant: false
    property bool compact: false
    property string dataState: "LIVE"
    property int dataAgeMs: -1
    property bool limitsVerified: true
    readonly property bool usable: dataState !== "LOST" && dataState !== "FAIL"
    readonly property bool degraded: dataState === "STALE" || dataState === "DEGRADED"

    HmiTokens { id: tokens }
    EmsGaugeV3 {
        anchors.fill: parent
        label: gauge.label
        value: gauge.value
        unit: gauge.unit
        minimum: gauge.minimum
        maximum: gauge.maximum
        greenStart: gauge.greenStart
        greenEnd: gauge.greenEnd
        majorStep: gauge.majorStep
        minorStep: gauge.minorStep
        decimals: gauge.decimals
        uiScale: gauge.uiScale
        dominant: gauge.dominant
        compact: gauge.compact
        normalRangesVisible: gauge.usable && !gauge.degraded
        opacity: gauge.usable ? (gauge.degraded ? 0.72 : 1.0) : 0.34
    }
    Rectangle {
        anchors.fill: parent
        visible: gauge.degraded || !gauge.usable
        color: "transparent"
        border.color: gauge.degraded ? tokens.caution : tokens.warning
        border.width: Math.max(2, 2 * gauge.uiScale)
        radius: width / 2
    }
    HmiDataStateBadge {
        anchors { right: parent.right; top: parent.top; margins: 5 * gauge.uiScale }
        dataState: gauge.dataState
        ageMs: gauge.dataAgeMs
        uiScale: gauge.uiScale
    }
    Rectangle {
        visible: !gauge.limitsVerified
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 2 * gauge.uiScale }
        width: limitLabel.implicitWidth + 12 * gauge.uiScale
        height: Math.max(16, 18 * gauge.uiScale)
        radius: 2 * gauge.uiScale
        color: "#E0061119"
        border.color: tokens.caution
        Text {
            id: limitLabel
            anchors.centerIn: parent
            text: "LIMITS UNVERIFIED"
            color: tokens.caution
            font.pixelSize: Math.max(8, 9 * gauge.uiScale)
            font.bold: true
        }
    }
}
