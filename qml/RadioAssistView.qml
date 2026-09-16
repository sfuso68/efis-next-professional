pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root
    required property var controller
    property real uiScale: Math.min(width / 1024, height / 600)
    property string language: "ITA"
    property bool voiceEnabled: false
    property string recognitionState: voiceEnabled ? "STANDBY • HOLD PTT" : "VOICE OFF"
    property string heardText: "NESSUN AUDIO ACQUISITO"
    property string pilotState: "DA CONFERMARE"
    signal closeRequested()

    readonly property color cyan: "#39D7E8"
    readonly property color green: "#5DDB55"
    readonly property color amber: "#F6B940"
    readonly property color red: "#FF4B42"
    readonly property color panel: "#E80A1822"
    readonly property string italianDraft: "Treviso Torre, I-TEST, punto Sierra, duemila piedi, richiesta ingresso nel circuito."
    readonly property string englishDraft: "Treviso Tower, I-TEST, Sierra reporting point, two thousand feet, request circuit joining."

    Rectangle { anchors.fill: parent; color: "#030A10" }

    Rectangle {
        x: 8 * root.uiScale; y: 7 * root.uiScale
        width: parent.width - 16 * root.uiScale; height: 48 * root.uiScale
        radius: 5 * root.uiScale; color: "#F007131C"; border.color: "#355362"
        Rectangle { width: 5 * root.uiScale; height: parent.height; color: root.cyan }
        Text { x: 18 * root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "EFIS NEXT"; color: "white"; font.pixelSize: 16 * root.uiScale; font.bold: true }
        Text { x: 126 * root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "RADIO ASSIST"; color: root.cyan; font.pixelSize: 16 * root.uiScale; font.bold: true }
        Text { anchors.centerIn: parent; text: "LIPH • TREVISO"; color: "white"; font.pixelSize: 14 * root.uiScale; font.bold: true }
        Rectangle {
            anchors { right: closeButton.left; rightMargin: 10 * root.uiScale; verticalCenter: parent.verticalCenter }
            width: 150 * root.uiScale; height: 30 * root.uiScale; radius: 3 * root.uiScale
            color: root.voiceEnabled ? "#143B20" : "#282D32"; border.color: root.voiceEnabled ? root.green : "#66717A"
            Text { anchors.centerIn: parent; text: root.recognitionState; color: root.voiceEnabled ? root.green : "#BEC8CE"; font.pixelSize: 8 * root.uiScale; font.bold: true }
        }
        Rectangle {
            id: closeButton
            anchors { right: parent.right; rightMargin: 7 * root.uiScale; verticalCenter: parent.verticalCenter }
            width: 62 * root.uiScale; height: 30 * root.uiScale; radius: 3 * root.uiScale
            color: "#1A2530"; border.color: "#66717A"
            Text { anchors.centerIn: parent; text: "CLOSE"; color: "white"; font.pixelSize: 8 * root.uiScale; font.bold: true }
            TapHandler { onTapped: root.closeRequested() }
        }
    }

    Rectangle {
        x: 8 * root.uiScale; y: 60 * root.uiScale
        width: parent.width - 16 * root.uiScale; height: 25 * root.uiScale
        color: "#B82A0808"; border.color: root.red
        Text { anchors.centerIn: parent; text: "SIMULATION • PHRASEOLOGY DRAFT • VERIFY AGAINST CURRENT AIP / CLEARANCE • NO RADIO TRANSMISSION"; color: "white"; font.pixelSize: 8 * root.uiScale; font.bold: true; font.letterSpacing: 0.8 * root.uiScale }
    }

    Rectangle {
        x: 8 * root.uiScale; y: 91 * root.uiScale; width: 668 * root.uiScale; height: 114 * root.uiScale
        radius: 5 * root.uiScale; color: root.panel; border.color: "#385463"
        Text { x: 13 * root.uiScale; y: 9 * root.uiScale; text: "ACTIVE STATION"; color: root.cyan; font.pixelSize: 8 * root.uiScale; font.bold: true }
        Text { x: 13 * root.uiScale; y: 28 * root.uiScale; text: "TREVISO TOWER"; color: "white"; font.pixelSize: 25 * root.uiScale; font.bold: true }
        Text { x: 245 * root.uiScale; y: 28 * root.uiScale; text: "118.700"; color: root.green; font.pixelSize: 25 * root.uiScale; font.bold: true }
        Rectangle { x: 405 * root.uiScale; y: 20 * root.uiScale; width: 1 * root.uiScale; height: 72 * root.uiScale; color: "#425A66" }
        Column {
            x: 425 * root.uiScale; y: 18 * root.uiScale; spacing: 3 * root.uiScale
            Text { text: "SOURCE  SIM TRAINING PACKAGE"; color: root.amber; font.pixelSize: 8 * root.uiScale; font.bold: true }
            Text { text: "REVISION  SIM-1"; color: "#D4DEE3"; font.pixelSize: 8 * root.uiScale }
            Text { text: "VALIDITY  NOT OPERATIONAL"; color: root.red; font.pixelSize: 8 * root.uiScale; font.bold: true }
            Text { text: "MODE  PILOT REVIEW REQUIRED"; color: root.cyan; font.pixelSize: 8 * root.uiScale }
        }
        Row {
            x: 13 * root.uiScale; y: 78 * root.uiScale; spacing: 8 * root.uiScale
            Repeater {
                model: ["VFR", "ARRIVAL", "RWY 07", "QNH 1016", "SQUAWK ----"]
                Rectangle {
                    required property string modelData
                    width: (modelData.length > 9 ? 112 : 91) * root.uiScale; height: 25 * root.uiScale
                    radius: 3 * root.uiScale; color: "#152631"; border.color: modelData.indexOf("----") >= 0 ? root.amber : "#496775"
                    Text { anchors.centerIn: parent; text: parent.modelData; color: parent.modelData.indexOf("----") >= 0 ? root.amber : "white"; font.pixelSize: 8 * root.uiScale; font.bold: true }
                }
            }
        }
    }

    Rectangle {
        x: 682 * root.uiScale; y: 91 * root.uiScale; width: 334 * root.uiScale; height: 114 * root.uiScale
        radius: 5 * root.uiScale; color: root.panel; border.color: "#385463"
        Text { x: 13 * root.uiScale; y: 9 * root.uiScale; text: "FLIGHT CONTEXT"; color: root.cyan; font.pixelSize: 8 * root.uiScale; font.bold: true }
        Grid {
            x: 13 * root.uiScale; y: 30 * root.uiScale; columns: 2; columnSpacing: 18 * root.uiScale; rowSpacing: 5 * root.uiScale
            Text { text: "CALLSIGN"; color: "#8FA4AF"; font.pixelSize: 8 * root.uiScale }
            Text { text: "I-TEST"; color: root.amber; font.pixelSize: 10 * root.uiScale; font.bold: true }
            Text { text: "POSITION"; color: "#8FA4AF"; font.pixelSize: 8 * root.uiScale }
            Text { text: "SIERRA • SIM"; color: root.amber; font.pixelSize: 10 * root.uiScale; font.bold: true }
            Text { text: "ALTITUDE"; color: "#8FA4AF"; font.pixelSize: 8 * root.uiScale }
            Text { text: root.controller.altitudeValid ? Math.round(root.controller.altitude) + " FT" : "INVALID"; color: root.controller.altitudeValid ? "white" : root.red; font.pixelSize: 10 * root.uiScale; font.bold: true }
        }
    }

    Rectangle {
        x: 8 * root.uiScale; y: 211 * root.uiScale; width: 668 * root.uiScale; height: 137 * root.uiScale
        radius: 5 * root.uiScale; color: root.panel; border.color: root.pilotState === "CONFERMATO PILOTA" ? root.green : "#385463"
        Text { x: 13 * root.uiScale; y: 10 * root.uiScale; text: "HEARD / PILOT INPUT"; color: root.cyan; font.pixelSize: 8 * root.uiScale; font.bold: true }
        Text { x: 13 * root.uiScale; y: 35 * root.uiScale; width: parent.width - 26 * root.uiScale; text: root.heardText; color: root.heardText.indexOf("NESSUN") === 0 ? "#81939D" : "white"; font.pixelSize: 16 * root.uiScale; font.bold: true; wrapMode: Text.WordWrap }
        Text { x: 13 * root.uiScale; y: 105 * root.uiScale; text: "RECOGNITION: " + root.pilotState; color: root.pilotState === "CONFERMATO PILOTA" ? root.green : root.amber; font.pixelSize: 9 * root.uiScale; font.bold: true }
        Text { anchors { right: parent.right; rightMargin: 13 * root.uiScale; bottom: parent.bottom; bottomMargin: 12 * root.uiScale } text: root.heardText.indexOf("NESSUN") === 0 ? "CONFIDENCE ---" : "CONFIDENCE 0.94 • SIM"; color: root.heardText.indexOf("NESSUN") === 0 ? "#81939D" : root.amber; font.pixelSize: 8 * root.uiScale }
    }

    Rectangle {
        x: 682 * root.uiScale; y: 211 * root.uiScale; width: 334 * root.uiScale; height: 270 * root.uiScale
        radius: 5 * root.uiScale; color: root.panel; border.color: "#385463"
        Text { x: 13 * root.uiScale; y: 10 * root.uiScale; text: "READ-BACK ITEMS"; color: root.cyan; font.pixelSize: 8 * root.uiScale; font.bold: true }
        Column {
            x: 13 * root.uiScale; y: 35 * root.uiScale; spacing: 7 * root.uiScale
            Repeater {
                model: [
                    {label:"RUNWAY", value:"07", state:"REVIEW"},
                    {label:"QNH", value:"1016", state:"REVIEW"},
                    {label:"ALTITUDE", value:"2000 FT", state:"REVIEW"},
                    {label:"SQUAWK", value:"MISSING", state:"BLOCKED"},
                    {label:"FREQUENCY", value:"118.700", state:"VERIFY"}
                ]
                Rectangle {
                    required property var modelData
                    width: 308 * root.uiScale; height: 37 * root.uiScale; radius: 3 * root.uiScale
                    color: "#111F29"; border.color: modelData.state === "BLOCKED" ? root.red : (modelData.state === "VERIFY" ? root.amber : "#3F5D6A")
                    Text { x: 9 * root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: parent.modelData.label; color: "#90A8B3"; font.pixelSize: 8 * root.uiScale; font.bold: true }
                    Text { anchors.centerIn: parent; text: parent.modelData.value; color: parent.modelData.state === "BLOCKED" ? root.red : "white"; font.pixelSize: 12 * root.uiScale; font.bold: true }
                    Text { anchors { right: parent.right; rightMargin: 9 * root.uiScale; verticalCenter: parent.verticalCenter } text: parent.modelData.state; color: parent.modelData.state === "BLOCKED" ? root.red : root.amber; font.pixelSize: 7 * root.uiScale; font.bold: true }
                }
            }
        }
    }

    Rectangle {
        x: 8 * root.uiScale; y: 354 * root.uiScale; width: 668 * root.uiScale; height: 127 * root.uiScale
        radius: 5 * root.uiScale; color: root.panel; border.color: root.cyan
        Text { x: 13 * root.uiScale; y: 9 * root.uiScale; text: "SUGGESTED PHRASE • " + root.language; color: root.cyan; font.pixelSize: 8 * root.uiScale; font.bold: true }
        Text { x: 13 * root.uiScale; y: 31 * root.uiScale; width: parent.width - 26 * root.uiScale; height: 62 * root.uiScale; text: root.language === "ITA" ? root.italianDraft : root.englishDraft; color: "white"; font.pixelSize: 15 * root.uiScale; font.bold: true; wrapMode: Text.WordWrap }
        Text { x: 13 * root.uiScale; y: 101 * root.uiScale; text: "DRAFT ONLY • VERIFY CALLSIGN, STATION, FREQUENCY AND CLEARANCE"; color: root.amber; font.pixelSize: 8 * root.uiScale; font.bold: true }
    }

    Row {
        x: 8 * root.uiScale; y: 489 * root.uiScale; spacing: 8 * root.uiScale
        Rectangle {
            width: 208 * root.uiScale; height: 59 * root.uiScale; radius: 5 * root.uiScale
            color: root.voiceEnabled ? "#153725" : "#15212A"; border.color: root.voiceEnabled ? root.green : "#536773"
            Text { anchors.centerIn: parent; text: root.voiceEnabled ? "VOICE ENABLED" : "ENABLE VOICE"; color: root.voiceEnabled ? root.green : "white"; font.pixelSize: 11 * root.uiScale; font.bold: true }
            TapHandler { onTapped: { root.voiceEnabled = !root.voiceEnabled; root.recognitionState = root.voiceEnabled ? "STANDBY • HOLD PTT" : "VOICE OFF"; root.pilotState = "DA CONFERMARE" } }
        }
        Rectangle {
            id: pttButton
            width: 208 * root.uiScale; height: 59 * root.uiScale; radius: 5 * root.uiScale
            color: pttArea.pressed ? "#154657" : "#112A36"; border.color: root.voiceEnabled ? root.cyan : "#536773"; border.width: 2 * root.uiScale
            Text { anchors.centerIn: parent; text: root.voiceEnabled ? "HOLD TO SPEAK\nPTT" : "PTT DISABLED"; horizontalAlignment: Text.AlignHCenter; color: root.voiceEnabled ? "white" : "#768891"; font.pixelSize: 10 * root.uiScale; font.bold: true }
            MouseArea {
                id: pttArea; anchors.fill: parent; enabled: root.voiceEnabled
                onPressed: { root.recognitionState = "LISTENING • SIM"; root.heardText = "…"; root.pilotState = "IN ASCOLTO" }
                onReleased: { root.recognitionState = "ACCEPTED • SIM"; root.heardText = root.language === "ITA" ? "CONFERMO" : "CONFIRM"; root.pilotState = "CONFERMATO PILOTA" }
                onCanceled: { root.recognitionState = "STANDBY • HOLD PTT"; root.heardText = "NESSUN AUDIO ACQUISITO"; root.pilotState = "DA CONFERMARE" }
            }
        }
        Rectangle {
            width: 136 * root.uiScale; height: 59 * root.uiScale; radius: 5 * root.uiScale
            color: root.language === "ITA" ? "#173D49" : "#15212A"; border.color: root.language === "ITA" ? root.cyan : "#536773"
            Text { anchors.centerIn: parent; text: "ITALIANO"; color: "white"; font.pixelSize: 10 * root.uiScale; font.bold: true }
            TapHandler { onTapped: { root.language = "ITA"; root.pilotState = "DA CONFERMARE" } }
        }
        Rectangle {
            width: 136 * root.uiScale; height: 59 * root.uiScale; radius: 5 * root.uiScale
            color: root.language === "ENG" ? "#173D49" : "#15212A"; border.color: root.language === "ENG" ? root.cyan : "#536773"
            Text { anchors.centerIn: parent; text: "ENGLISH"; color: "white"; font.pixelSize: 10 * root.uiScale; font.bold: true }
            TapHandler { onTapped: { root.language = "ENG"; root.pilotState = "TO BE CONFIRMED" } }
        }
        Rectangle {
            width: 280 * root.uiScale; height: 59 * root.uiScale; radius: 5 * root.uiScale
            color: root.pilotState === "CONFERMATO PILOTA" ? "#163B21" : "#2C2414"; border.color: root.pilotState === "CONFERMATO PILOTA" ? root.green : root.amber
            Text { anchors.centerIn: parent; text: root.pilotState === "CONFERMATO PILOTA" ? "ACK RECORDED • SENSOR NOT VERIFIED" : "TOUCH / VOICE CONFIRMATION PENDING"; color: root.pilotState === "CONFERMATO PILOTA" ? root.green : root.amber; font.pixelSize: 8 * root.uiScale; font.bold: true }
        }
    }

    Rectangle {
        x: 8 * root.uiScale; y: 555 * root.uiScale; width: parent.width - 16 * root.uiScale; height: 37 * root.uiScale
        radius: 4 * root.uiScale; color: "#131B22"; border.color: "#475A64"
        Text { anchors.centerIn: parent; text: "ADVISORY ONLY • PILOT RETAINS CONTROL • NO PIXHAWK / SERVO / RADIO / TRANSPONDER COMMAND PATH"; color: "#C5D0D5"; font.pixelSize: 8 * root.uiScale; font.bold: true; font.letterSpacing: 0.6 * root.uiScale }
    }
}
