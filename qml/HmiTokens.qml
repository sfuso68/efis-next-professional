import QtQuick

QtObject {
    readonly property color background: "#03111C"
    readonly property color surface: "#081923"
    readonly property color foreground: "#F4F8FC"
    readonly property color secondary: "#A7B5BE"
    readonly property color cyan: "#28D7E8"
    readonly property color normal: "#39D353"
    readonly property color caution: "#FFC928"
    readonly property color warning: "#FF3B30"
    readonly property color navigation: "#F238E6"
    readonly property color invalid: "#8A96A3"
    readonly property int minimumTouch: 44
    readonly property int primaryText: 34
    readonly property int secondaryText: 18
    readonly property int minimumMicroText: 8
    readonly property int minimumLabelText: 9
    readonly property int minimumActionText: 12
    readonly property real minimumStroke: 1.5

    function legibleText(base, scale, minimum) {
        return Math.max(minimum, Math.round(base * scale))
    }

    function stateColor(state) {
        if (state === "LIVE") return normal
        if (state === "SIM") return cyan
        if (state === "STALE" || state === "DEGRADED") return caution
        if (state === "LOST" || state === "FAIL") return warning
        return invalid
    }
}
