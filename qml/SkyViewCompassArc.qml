import QtQuick

Item {
    id: compass
    property real heading: 0
    property real selectedCourse: 0
    property real cdiDeviation: 0
    property string waypoint: "----"
    property string dataState: "LIVE"
    property real distanceNm: 0
    property string ete: "--:--"
    property real uiScale: 1
    signal navigationRequested()
    readonly property real radius: width * 0.78
    readonly property real halfAngle: 35

    HmiTokens { id: tokens }
    Canvas {
        id: face
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()
        Connections {
            target: compass
            function onHeadingChanged() { face.requestPaint() }
            function onSelectedCourseChanged() { face.requestPaint() }
        }
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const cx = compass.width / 2
            const cy = compass.height + compass.radius * 0.10
            const r = compass.radius
            const start = (270 - compass.halfAngle) * Math.PI / 180
            const end = (270 + compass.halfAngle) * Math.PI / 180
            ctx.strokeStyle = "#EAF2F5"
            ctx.lineWidth = Math.max(1.5, 1.5 * compass.uiScale)
            ctx.beginPath(); ctx.arc(cx, cy, r, start, end); ctx.stroke()
            ctx.textAlign = "center"; ctx.textBaseline = "middle"
            for (let offset = -35; offset <= 35; offset += 5) {
                const angle = (270 + offset) * Math.PI / 180
                const major = offset % 10 === 0
                const inner = r - (major ? 13 : 8) * compass.uiScale
                ctx.strokeStyle = major ? "#F4F8FC" : "#A7B5BE"
                ctx.lineWidth = major ? 2 * compass.uiScale : compass.uiScale
                ctx.beginPath()
                ctx.moveTo(cx + Math.cos(angle) * inner, cy + Math.sin(angle) * inner)
                ctx.lineTo(cx + Math.cos(angle) * r, cy + Math.sin(angle) * r)
                ctx.stroke()
                if (major) {
                    const value = (Math.round(compass.heading / 10) * 10 + offset + 360) % 360
                    const label = value === 0 ? "N" : value === 90 ? "E" : value === 180 ? "S" : value === 270 ? "W" : ("000" + value).slice(-3)
                    ctx.fillStyle = "#F4F8FC"
                    ctx.font = "600 " + Math.max(10, 12 * compass.uiScale) + "px sans-serif"
                    ctx.fillText(label, cx + Math.cos(angle) * (r - 28 * compass.uiScale), cy + Math.sin(angle) * (r - 28 * compass.uiScale))
                }
            }
        }
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 2 * compass.uiScale
        width: 66 * compass.uiScale; height: 26 * compass.uiScale
        radius: 3 * compass.uiScale
        color: "#D0061119"; border.color: tokens.foreground
        Text { anchors.centerIn: parent; text: ("000" + Math.round(compass.heading)).slice(-3) + "°"; color: tokens.foreground; font.pixelSize: 16 * compass.uiScale; font.bold: true }
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 33 * compass.uiScale
        width: 3 * compass.uiScale; height: Math.max(34 * compass.uiScale, parent.height * 0.34)
        color: tokens.navigation
        transform: Translate { x: compass.cdiDeviation * 18 * compass.uiScale }
    }
    Column {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 7 * compass.uiScale }
        spacing: 2 * compass.uiScale
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: compass.waypoint; color: tokens.navigation; font.pixelSize: 15 * compass.uiScale; font.bold: true }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "DTK " + Math.round(compass.selectedCourse) + "°   " + compass.distanceNm.toFixed(1) + " NM   ETE " + compass.ete; color: tokens.foreground; font.pixelSize: 11 * compass.uiScale }
    }
    HmiDataStateBadge {
        anchors { right: parent.right; top: parent.top }
        dataState: compass.dataState
        uiScale: compass.uiScale
    }
    TapHandler { onTapped: compass.navigationRequested() }
}
