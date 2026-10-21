import QtQuick

Item {
    id: root
    property real uiScale: 1
    property bool radarEnabled: true
    property bool interactive: true
    property bool compactLayout: false
    property real radarOpacity: 0.58
    property string regionName: "VENETO"
    property int refreshMinutes: 5
    property real badgeTopMargin: 6
    property var weatherProvider: typeof weather !== "undefined" ? weather : null
    readonly property var provider: weatherProvider || ({})
    readonly property string reportedState: String(provider.radarState || "NO DATA")
    readonly property bool simulationActive: provider.simulation === true
    readonly property bool ageKnown: typeof provider.radarAgeMinutes === "number"
                                    && isFinite(provider.radarAgeMinutes) && provider.radarAgeMinutes >= 0
    // The service owns freshness thresholds. Never substitute download/observation
    // times here or infer a valid state from a decoded image alone.
    readonly property bool trustedDisplayState: ((reportedState === "CURRENT" || reportedState === "STALE")
                                                 && !simulationActive && ageKnown)
                                                || (reportedState === "SIMULATED" && simulationActive)
    readonly property bool available: radarEnabled && radar.status === Image.Ready && trustedDisplayState
    readonly property bool loading: radarEnabled && trustedDisplayState && radar.status === Image.Loading
    readonly property string failureState: {
        if ((reportedState === "SIMULATED") !== simulationActive
                && (reportedState === "SIMULATED" || reportedState === "CURRENT" || reportedState === "STALE"))
            return "MODE MISMATCH"
        if ((reportedState === "CURRENT" || reportedState === "STALE") && !ageKnown) return "AGE UNKNOWN"
        if (reportedState === "CLOCK_SKEW") return "CLOCK SKEW"
        return ["EXPIRED", "CLOCK SKEW", "NO DATA"].indexOf(reportedState) >= 0 ? reportedState : "INVALID"
    }
    readonly property string displayState: !radarEnabled ? "OFF" : !trustedDisplayState ? failureState
                                            : loading ? "LOADING" : available ? reportedState
                                            : radar.status === Image.Error ? "IMAGE ERROR" : "NO DATA"
    readonly property color statusColor: !radarEnabled ? "#80939B"
                                         : available && reportedState === "CURRENT" ? "#00D7E8" : "#E0A52B"
    readonly property string statusLine: displayState === "OFF" || displayState === "LOADING" ? displayState
        : available ? (simulationActive ? "SIMULATED • TRAINING"
                                       : displayState+" • CACHE "+Math.floor(provider.radarAgeMinutes)+" MIN")
        : displayState === "NO DATA" ? "NO DATA • CHECK SOURCE" : displayState+" • NO DATA"
    readonly property string failureMessage: displayState === "EXPIRED" ? "WX EXPIRED\nREFRESH WEATHER"
        : displayState === "CLOCK SKEW" ? "WX CLOCK SKEW\nCHECK CLOCK"
        : "WX "+displayState+"\nCHECK WEATHER SOURCE"

    clip: true

    Image {
        id: radar
        anchors.fill: parent
        visible: root.available
        source: root.radarEnabled && root.trustedDisplayState ? (root.provider.radarSource || "") : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        opacity: root.radarOpacity
    }

    Repeater {
        model: root.available && root.simulationActive ? [{x:.48,y:.39},{x:.61,y:.52},{x:.24,y:.24}] : []
        delegate: Text { required property var modelData; x:(modelData.x+((root.provider.simulationPhase%10)/250))*root.width; y:modelData.y*root.height; text:"ϟ"; color:"#FFF36A"; font.pixelSize:15*root.uiScale; font.bold:true; style:Text.Outline; styleColor:"#A0000000" }
    }

    Rectangle {
        id: statusBadge
        anchors { left: parent.left; top: parent.top; leftMargin: 6*root.uiScale; topMargin: root.badgeTopMargin*root.uiScale }
        width: (root.compactLayout ? root.width/root.uiScale - 12 : 132)*root.uiScale
        height: (root.compactLayout ? 44 : 34)*root.uiScale; radius: 3*root.uiScale
        color: "#D9040B11"; border.color: root.statusColor
        Column {
            anchors { fill: parent; margins: 5*root.uiScale }
            spacing: 1*root.uiScale
            Text { text: "WX "+String(root.provider.radarProduct || "—")+" • "+(root.simulationActive ? "SIM" : String(root.provider.radarArea || "NO AREA")); color: root.statusColor; font.pixelSize: 7*root.uiScale; font.bold: true }
            Text { text: root.statusLine; color: root.available && root.reportedState === "CURRENT" ? "#E8F3F7" : root.statusColor; font.pixelSize: 6*root.uiScale; font.bold: true }
        }
    }

    Rectangle {
        visible: root.radarEnabled && !root.available && !root.loading && !root.compactLayout
        anchors { horizontalCenter: parent.horizontalCenter; top: statusBadge.bottom; topMargin: 6*root.uiScale }
        width: 154*root.uiScale; height: 34*root.uiScale; radius: 3*root.uiScale
        color: "#E8071118"; border.color: "#E0A52B"
        Text { anchors.centerIn: parent; text: root.failureMessage; horizontalAlignment: Text.AlignHCenter; color: "#E8B84A"; font.pixelSize: 7*root.uiScale; font.bold: true }
    }

    Text {
        anchors { right: parent.right; bottom: parent.bottom; margins: 5*root.uiScale }
        text: "SUPPLEMENTARY • © RADAR-DPC CC-BY-SA"
        color: "#DCEAF0"; style: Text.Outline; styleColor: "#80000000"
        font.pixelSize: 5.5*root.uiScale; font.bold: true
    }

    Rectangle {
        visible: root.available && root.simulationActive && !root.compactLayout && root.width > 300*root.uiScale
        anchors { left: parent.left; bottom: parent.bottom; leftMargin: 7*root.uiScale; bottomMargin: 7*root.uiScale }
        width: 226*root.uiScale; height: 29*root.uiScale; radius: 3*root.uiScale
        color: "#E9071118"; border.color: "#6A7D86"
        Row { anchors { fill: parent; margins: 5*root.uiScale }
            spacing: 5*root.uiScale
            Text { text:"TRAINING"; color:"#E8B84A"; font.pixelSize:6*root.uiScale; font.bold:true }
            Repeater { model:[{c:"#25C85A",t:"LIGHT"},{c:"#E4CE23",t:"MOD"},{c:"#E83C31",t:"HEAVY"}]
                delegate: Row { required property var modelData; spacing:2*root.uiScale
                    Rectangle { width:7*root.uiScale; height:7*root.uiScale; radius:width/2; color:modelData.c }
                    Text { text:modelData.t; color:"#DCEAF0"; font.pixelSize:5.5*root.uiScale; font.bold:true }
                }
            }
            Text { text:"→ E 18 KT"; color:"#00E5F2"; font.pixelSize:5.5*root.uiScale; font.bold:true }
        }
    }

    TapHandler { enabled: root.interactive; onTapped: root.radarEnabled = !root.radarEnabled; onDoubleTapped: if(typeof root.provider.cycleProduct === "function") root.provider.cycleProduct() }
}
