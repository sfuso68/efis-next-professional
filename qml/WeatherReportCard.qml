import QtQuick

Item {
    id: root
    property var report: ({})
    property bool taf: false
    property real uiScale: 1
    readonly property string key: taf ? "taf" : "metar"
    readonly property bool hasReport: typeof report[key]==="string" && report[key].length>0
    readonly property bool hasRawRecord: hasReport || (typeof report[taf?"rawTaf":"rawMetar"]==="string" && report[taf?"rawTaf":"rawMetar"].length>0)
    readonly property string state: hasRawRecord ? String(report[key+"State"] || "NO DATA") : "NO DATA"
    readonly property bool training: state==="SIMULATED" && report.simulated===true
    readonly property bool usable: hasReport && (training || (report.simulated===false && (state==="CURRENT" || state==="STALE")))
    readonly property string reading: usable && typeof report[key]==="string" && report[key].length ? report[key] : "NO USABLE REPORT"
    readonly property bool current: usable && state==="CURRENT" && report.simulated===false
    Text {
        objectName: "weatherReportStatus"
        width:parent.width
        textFormat:Text.PlainText
        text:String(root.report.icao||"----")+" • "+(root.taf?"TAF ":"METAR ")+root.state+
             (typeof root.report[root.key+"AgeMinutes"]==="number"&&root.report[root.key+"AgeMinutes"]>=0?" • "+root.report[root.key+"AgeMinutes"]+" MIN":" • AGE —")
        color:root.current?"#E8F3F7":"#E8B84A"
        font.pixelSize:Math.max(8,8*root.uiScale)
        font.bold:true
        elide:Text.ElideRight
    }
    Text {
        objectName:"weatherReportReading"
        y:16*root.uiScale
        width:parent.width
        height:Math.max(0,parent.height-y)
        textFormat:Text.PlainText
        text:root.reading
        color:root.current?"#E8F3F7":"#E8B84A"
        font.pixelSize:Math.max(8,8*root.uiScale)
        wrapMode:Text.WordWrap
        elide:Text.ElideRight
        maximumLineCount:3
    }
}
