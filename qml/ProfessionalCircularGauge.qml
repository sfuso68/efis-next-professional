import QtQuick
import EfisNext.Native

Item {
    id: gauge
    required property string label
    required property real value
    required property string unit
    required property real minimum
    required property real maximum
    required property real greenStart
    required property real greenEnd
    property int decimals: 0
    property real uiScale: 1
    property real majorStep: (maximum-minimum)/5
    property real minorStep: majorStep/5
    Theme { id: theme }
    CircularGaugeItem { anchors.fill: parent; value: gauge.value; minimum: gauge.minimum; maximum: gauge.maximum; greenStart: gauge.greenStart; greenEnd: gauge.greenEnd; majorStep: gauge.majorStep; minorStep: gauge.minorStep }
    Repeater {
        id: scaleLabels
        model: Math.floor((gauge.maximum-gauge.minimum)/gauge.majorStep)+1
        delegate: Text {
            required property int index
            readonly property real fraction: index / Math.max(1, scaleLabels.count-1)
            readonly property real angle: (-220 + fraction*260) * Math.PI/180
            readonly property real radius: Math.min(gauge.width,gauge.height)*.31
            text: Number(gauge.minimum+index*gauge.majorStep).toFixed(0)
            color: "#E7F0F3"; font.pixelSize: Math.max(7, 8*gauge.uiScale); font.bold: true
            x: gauge.width*.5 + Math.cos(angle)*radius - width/2
            y: gauge.height*.57 + Math.sin(angle)*radius - height/2
        }
    }
    Text { anchors.horizontalCenter: parent.horizontalCenter; y: 31*gauge.uiScale
        text: gauge.label; color: theme.cyan; font.pixelSize: 9*gauge.uiScale; font.bold: true }
    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 15*gauge.uiScale }
        text: Number(gauge.value).toFixed(gauge.decimals); color: theme.primary; font.pixelSize: 17*gauge.uiScale; font.bold: true }
    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 5*gauge.uiScale }
        text: gauge.unit; color: theme.secondary; font.pixelSize: 7*gauge.uiScale; font.bold: true }
}
