import QtQuick

Rectangle {
    id: rail
    property real uiScale: 1
    property int healthLevel: 0
    property string alertText: ""
    property int alertSeverity: -1
    property bool flightDirectorActive: false
    property real selectedAltitude: 0
    property string viewName: "PFD FOCUS"
    property string blackBoxState: "OFFLINE"
    property real blackBoxRecords: 0

    // Stato del dato e del nodo sensori. Erano scritti a mano nel modello qui
    // sotto: adesso arrivano dallo stato reale, nel vocabolario obbligatorio
    // della specifica PFD_FOCUS_1024.
    property string dataSourceState: "FAIL"
    property string sensorNodeState: "NOT INSTALLED"

    // Rosso e ambra restano riservati agli allarmi, il magenta al flight
    // director: qui si usano verde per lo stato normale, cyan per un modo
    // scelto, grigio per cio' che non c'e'. Vedi EFIS_UI_DESIGN_SYSTEM.
    function stateColor(state) {
        if (state === "LIVE") return "#62D347";
        if (state === "SIM" || state === "SIM FORCED") return "#19C3E6";
        if (state === "STALE" || state === "DEGRADED") return "#E6A72E";
        if (state === "FAIL" || state === "LOST") return "#FF4136";
        return "#7F929E";
    }

    color: "#08121B"; border.color: "#314654"; border.width: 1
    Row {
        anchors.fill: parent
        Item {
            width: parent.width * 0.20; height: parent.height
            Text { x: 14 * rail.uiScale; y: 5 * rail.uiScale; text: "EFIS <font color='#19C3E6'>NEXT</font>"; textFormat: Text.RichText; color: "#F2F0E9"; font.pixelSize: 20 * rail.uiScale; font.bold: true }
            Text { anchors.right: parent.right; anchors.rightMargin: 10 * rail.uiScale; anchors.bottom: parent.bottom; anchors.bottomMargin: 4 * rail.uiScale; text: rail.viewName; color: "#19C3E6"; font.pixelSize: 8 * rail.uiScale; font.bold: true }
        }
        Repeater {
            model: [
                { tag: "GUIDANCE", value: rail.flightDirectorActive ? "FD ACTIVE" : "FD OFF", color: rail.flightDirectorActive ? "#62D347" : "#E6A72E" },
                { tag: "ALT SELECT", value: Math.round(rail.selectedAltitude) + " FT", color: "#19C3E6" },
                { tag: "DATA SOURCE", value: rail.dataSourceState, color: rail.stateColor(rail.dataSourceState) },
                { tag: "PIXHAWK 6X", value: rail.sensorNodeState, color: rail.stateColor(rail.sensorNodeState) },
                { tag: "BLACK BOX", value: rail.blackBoxState === "RECORDING" ? "REC " + Math.round(rail.blackBoxRecords) : rail.blackBoxState, color: rail.blackBoxState === "RECORDING" ? "#62D347" : rail.blackBoxState === "STOPPED" ? "#19C3E6" : "#E6A72E" }
            ]
            delegate: Item {
                required property var modelData
                width: rail.width * 0.12; height: rail.height
                Rectangle { width: 1; height: parent.height; color: "#314654" }
                Text { anchors.horizontalCenter: parent.horizontalCenter; y: 7 * rail.uiScale; text: modelData.tag; color: "#7F929E"; font.pixelSize: 8 * rail.uiScale; font.bold: true }
                Text { anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 8 * rail.uiScale; text: modelData.value; color: modelData.color; font.pixelSize: 10 * rail.uiScale; font.bold: true }
            }
        }
        Item {
            width: rail.width * 0.20; height: rail.height
            Rectangle { anchors.fill: parent; anchors.margins: 5 * rail.uiScale; radius: 3; color: rail.alertText.length === 0 ? "#10201A" : rail.alertSeverity >= 2 ? "#45161A" : rail.alertSeverity === 1 ? "#3B2C10" : "#0B2933"; border.color: rail.alertText.length === 0 ? "#62D347" : rail.alertSeverity >= 2 ? "#FF4136" : rail.alertSeverity === 1 ? "#FFB02E" : "#19C3E6" }
            Text { anchors.centerIn: parent; text: rail.alertText.length === 0 ? "SYSTEM NOMINAL" : rail.alertText; color: rail.alertText.length === 0 ? "#62D347" : "#F2F0E9"; font.pixelSize: 10 * rail.uiScale; font.bold: true; elide: Text.ElideRight; width: parent.width - 16 * rail.uiScale; horizontalAlignment: Text.AlignHCenter }
        }
    }
}
