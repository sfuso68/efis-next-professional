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
    Theme { id: theme }
    CircularGaugeItem { anchors.fill: parent; value: gauge.value; minimum: gauge.minimum; maximum: gauge.maximum; greenStart: gauge.greenStart; greenEnd: gauge.greenEnd }
    Text { anchors.horizontalCenter: parent.horizontalCenter; y: 24*gauge.uiScale
        text: gauge.label; color: theme.cyan; font.pixelSize: 7*gauge.uiScale; font.bold: true }
    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 15*gauge.uiScale }
        text: Number(gauge.value).toFixed(gauge.decimals); color: theme.primary; font.pixelSize: 13*gauge.uiScale; font.bold: true }
    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 5*gauge.uiScale }
        text: gauge.unit; color: theme.secondary; font.pixelSize: 5*gauge.uiScale; font.bold: true }
    Text { anchors { left: parent.left; bottom: parent.bottom; leftMargin: 7*gauge.uiScale; bottomMargin: 12*gauge.uiScale }
        text: Number(gauge.minimum).toFixed(0); color: "#8398A3"; font.pixelSize: 5*gauge.uiScale; font.bold: true }
    Text { anchors { right: parent.right; bottom: parent.bottom; rightMargin: 7*gauge.uiScale; bottomMargin: 12*gauge.uiScale }
        text: Number(gauge.maximum).toFixed(0); color: "#8398A3"; font.pixelSize: 5*gauge.uiScale; font.bold: true }
}
