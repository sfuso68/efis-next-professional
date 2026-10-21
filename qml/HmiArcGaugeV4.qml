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
    property bool valueValid: true
    readonly property bool usable: ["LIVE", "SIM", "SIM FORCED", "DEGRADED"].indexOf(dataState) >= 0 && valueValid && isFinite(value)
    readonly property bool degraded: dataState === "STALE" || dataState === "DEGRADED"

    HmiTokens { id: tokens }
    EmsGaugeV3 {
        anchors.fill: parent
        label: gauge.label
        value: gauge.value
        valueValid: gauge.usable
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
        anchors { left: parent.left; top: parent.top; margins: 7 * gauge.uiScale }
        width: Math.max(15, 17 * gauge.uiScale)
        height: width
        radius: width / 2
        color: "#C0251B00"
        border.color: tokens.caution
        Text {
            anchors.centerIn: parent
            text: "!"
            color: tokens.caution
            font.pixelSize: Math.max(9, 11 * gauge.uiScale)
            font.bold: true
        }
    }
}
