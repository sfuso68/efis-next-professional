pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root
    required property var controller
    property real uiScale: Math.min(width/1024, height/600)
    signal closeRequested()
    readonly property color red: "#FF3B30"
    readonly property color amber: "#F5B642"
    readonly property color cyan: "#35D5E5"
    readonly property color green: "#60D447"
    readonly property color panel: "#EB091721"

    Rectangle { anchors.fill: parent; color: "#030A10" }

    Rectangle {
        x: 7*root.uiScale; y: 6*root.uiScale
        width: parent.width-14*root.uiScale; height: 50*root.uiScale
        radius: 5*root.uiScale; color: "#F00A131B"; border.color: root.red
        Rectangle { width: 5*root.uiScale; height: parent.height; color: root.red }
        Text { x: 17*root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "EMERGENCY ASSIST"; color: root.red; font.pixelSize: 17*root.uiScale; font.bold: true }
        Text { anchors.centerIn: parent; text: root.controller.emergencyAssistKind; color: "white"; font.pixelSize: 16*root.uiScale; font.bold: true }
        Text {
            anchors { right: exit.left; rightMargin: 13*root.uiScale; verticalCenter: parent.verticalCenter }
            text: root.controller.emergencyDataQuality+"  •  T+"+root.controller.emergencyElapsedSeconds+" s"
            color: root.controller.emergencySimulationOnly ? root.red : root.amber
            font.pixelSize: 8*root.uiScale; font.bold: true
        }
        Rectangle {
            id: exit
            anchors { right: parent.right; rightMargin: 7*root.uiScale; verticalCenter: parent.verticalCenter }
            width: 70*root.uiScale; height: 34*root.uiScale; radius: 4*root.uiScale
            color: "#351015"; border.color: root.red
            Text { anchors.centerIn: parent; text: "EXIT"; color: "white"; font.pixelSize: 10*root.uiScale; font.bold: true }
            TapHandler { onTapped: { root.controller.cancelEmergencyAssist("pilot exit"); root.closeRequested() } }
        }
    }

    Rectangle {
        x: 8*root.uiScale; y: 60*root.uiScale
        width: parent.width-16*root.uiScale; height: 25*root.uiScale
        color: root.controller.emergencySimulationOnly ? "#C82A0808" : "#C714241B"
        border.color: root.controller.emergencySimulationOnly ? root.red : root.green
        Text {
            anchors.centerIn: parent
            text: root.controller.emergencySimulationOnly ? "SIMULATION • TEST DATA • NOT FOR FLIGHT" : "ADVISORY ONLY • PILOT RETAINS CONTROL"
            color: "white"; font.pixelSize: 9*root.uiScale; font.bold: true; font.letterSpacing: 1.2*root.uiScale
        }
    }

    Row {
        x: 8*root.uiScale; y: 89*root.uiScale; spacing: 4*root.uiScale
        Repeater {
            model: [
                {id:"ENGINE_POWER_LOSS", label:"ENGINE", match:"ENGINE POWER LOSS"},
                {id:"ABNORMAL_ATTITUDE", label:"ATTITUDE", match:"ABNORMAL ATTITUDE"},
                {id:"UNSAFE_AIRSPEED", label:"AIRSPEED", match:"UNSAFE AIRSPEED"},
                {id:"ELECTRICAL_FAILURE", label:"ELECTRICAL", match:"ELECTRICAL FAILURE"},
                {id:"SMOKE_OR_FIRE", label:"FIRE / SMOKE", match:"SMOKE / FIRE"},
                {id:"WEATHER_OR_TERRAIN", label:"WX / TERRAIN", match:"WEATHER / TERRAIN"},
                {id:"SENSOR_OR_PFD_LOSS", label:"SENSORS", match:"SENSOR / PFD LOSS"},
                {id:"PILOT_INCAPACITATION", label:"MEDICAL", match:"PILOT INCAPACITATION"}
            ]
            Rectangle {
                id: scenarioButton
                required property var modelData
                width: 121*root.uiScale; height: 31*root.uiScale; radius: 3*root.uiScale
                color: root.controller.emergencyAssistKind===scenarioButton.modelData.match ? "#5A35100C" : "#D5101D26"
                border.color: root.controller.emergencyAssistKind===scenarioButton.modelData.match ? root.red : "#435A68"
                border.width: root.controller.emergencyAssistKind===scenarioButton.modelData.match ? 2*root.uiScale : 1*root.uiScale
                Text { anchors.centerIn: parent; text: scenarioButton.modelData.label; color: "white"; font.pixelSize: 7*root.uiScale; font.bold: true }
                TapHandler { onTapped: root.controller.confirmEmergencyAssist(scenarioButton.modelData.id) }
            }
        }
    }

    Rectangle {
        id: flyPanel
        x: 8*root.uiScale; y: 125*root.uiScale
        width: 242*root.uiScale; height: 378*root.uiScale
        radius: 5*root.uiScale; color: root.panel; border.color: "#71403E"
        Text { x: 12*root.uiScale; y: 9*root.uiScale; text: "1  FLY"; color: root.red; font.pixelSize: 17*root.uiScale; font.bold: true }
        Text {
            x: 12*root.uiScale; y: 39*root.uiScale; width: parent.width-24*root.uiScale; height: 48*root.uiScale
            text: root.controller.emergencyFlyAdvisory; color: "white"; wrapMode: Text.WordWrap
            maximumLineCount: 3; elide: Text.ElideRight; font.pixelSize: 12*root.uiScale; font.bold: true
        }
        Row {
            x: 12*root.uiScale; y: 94*root.uiScale; spacing: 7*root.uiScale
            Rectangle {
                width: 105*root.uiScale; height: 62*root.uiScale; radius: 4*root.uiScale; color: "#B407121A"; border.color: root.controller.airspeedValid ? root.cyan : root.red
                Text { x: 8*root.uiScale; y: 6*root.uiScale; text: "IAS"; color: root.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
                Text { anchors.centerIn: parent; text: root.controller.airspeedValid ? Math.round(root.controller.airspeed)+" KT" : "INVALID"; color: root.controller.airspeedValid ? "white" : root.red; font.pixelSize: 17*root.uiScale; font.bold: true }
            }
            Rectangle {
                width: 105*root.uiScale; height: 62*root.uiScale; radius: 4*root.uiScale; color: "#B407121A"; border.color: root.controller.emergencyTargetSpeed==="NOT APPROVED" ? root.amber : root.green
                Text { x: 8*root.uiScale; y: 6*root.uiScale; text: "TARGET"; color: root.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
                Text { anchors.centerIn: parent; text: root.controller.emergencyTargetSpeed; color: root.controller.emergencyTargetSpeed==="NOT APPROVED" ? root.amber : "white"; font.pixelSize: root.controller.emergencyTargetSpeed==="NOT APPROVED" ? 9*root.uiScale : 17*root.uiScale; font.bold: true }
            }
        }
        Row {
            x: 12*root.uiScale; y: 163*root.uiScale; spacing: 7*root.uiScale
            Rectangle {
                width: 105*root.uiScale; height: 53*root.uiScale; radius: 4*root.uiScale; color: "#9A07121A"
                Text { x: 8*root.uiScale; y: 5*root.uiScale; text: "ALTITUDE"; color: root.cyan; font.pixelSize: 7*root.uiScale }
                Text { anchors { left: parent.left; leftMargin: 8*root.uiScale; bottom: parent.bottom; bottomMargin: 7*root.uiScale } text: root.controller.altitudeValid ? Math.round(root.controller.altitude)+" FT" : "---"; color: root.controller.altitudeValid ? "white" : root.red; font.pixelSize: 14*root.uiScale; font.bold: true }
            }
            Rectangle {
                width: 105*root.uiScale; height: 53*root.uiScale; radius: 4*root.uiScale; color: "#9A07121A"
                Text { x: 8*root.uiScale; y: 5*root.uiScale; text: "VERTICAL SPEED"; color: root.cyan; font.pixelSize: 7*root.uiScale }
                Text { anchors { left: parent.left; leftMargin: 8*root.uiScale; bottom: parent.bottom; bottomMargin: 7*root.uiScale } text: root.controller.verticalSpeedValid ? Math.round(root.controller.verticalSpeed)+" FPM" : "---"; color: root.controller.verticalSpeedValid ? "white" : root.red; font.pixelSize: 13*root.uiScale; font.bold: true }
            }
        }
        Rectangle {
            x: 12*root.uiScale; y: 223*root.uiScale; width: parent.width-24*root.uiScale; height: 38*root.uiScale; radius: 4*root.uiScale
            color: "#9A07121A"; border.color: root.controller.attitudeValid ? "#48616E" : root.red
            Text { anchors.centerIn: parent; text: root.controller.attitudeValid ? "ATT  R "+Number(root.controller.roll).toFixed(0)+"°   P "+Number(root.controller.pitch).toFixed(0)+"°" : "ATTITUDE INVALID"; color: root.controller.attitudeValid ? "white" : root.red; font.pixelSize: 10*root.uiScale; font.bold: true }
        }
        Text { x: 12*root.uiScale; y: 270*root.uiScale; text: "DATA GATES"; color: root.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
        Text {
            x: 12*root.uiScale; y: 287*root.uiScale; width: parent.width-24*root.uiScale; height: 38*root.uiScale
            text: root.controller.emergencyDegradationReasons.length>0 ? root.controller.emergencyDegradationReasons.slice(0,3).join(" • ") : "PRIMARY INPUTS AVAILABLE"
            color: root.controller.emergencyDegradationReasons.length>0 ? root.amber : root.green
            wrapMode: Text.WordWrap; maximumLineCount: 3; elide: Text.ElideRight; font.pixelSize: 7*root.uiScale; font.bold: true
        }
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12*root.uiScale }
            height: 39*root.uiScale; radius: 4*root.uiScale; color: "#17351F"; border.color: root.green
            Text { anchors.centerIn: parent; text: "FLY ACKNOWLEDGED"; color: "white"; font.pixelSize: 10*root.uiScale; font.bold: true }
            TapHandler { onTapped: root.controller.acknowledgeEmergencyFly() }
        }
    }

    Rectangle {
        id: centerPanel
        x: 257*root.uiScale; y: 125*root.uiScale
        width: 487*root.uiScale; height: 378*root.uiScale
        radius: 5*root.uiScale; color: root.panel; border.color: "#355A68"
        EmergencyFlightSupport { x: 6*root.uiScale; y: 6*root.uiScale; width: parent.width-12*root.uiScale; height: 252*root.uiScale; uiScale: root.uiScale; controller: root.controller }
        Text { x: 10*root.uiScale; y: 265*root.uiScale; text: "2  NAVIGATE"; color: root.cyan; font.pixelSize: 12*root.uiScale; font.bold: true }
        Text { x: 113*root.uiScale; y: 268*root.uiScale; width: 245*root.uiScale; text: root.controller.emergencyNavigationAdvisory; color: "white"; font.pixelSize: 8*root.uiScale; font.bold: true; elide: Text.ElideRight }
        Text { anchors { right: parent.right; rightMargin: 10*root.uiScale } y: 267*root.uiScale; text: root.controller.emergencyGlideRange; color: root.controller.emergencyGlideRange.indexOf("NOT")>=0 ? root.amber : root.cyan; font.pixelSize: 9*root.uiScale; font.bold: true }
        Column {
            x: 9*root.uiScale; y: 290*root.uiScale; spacing: 3*root.uiScale
            Repeater {
                model: root.controller.emergencyRankedAirports.slice(0,3)
                Rectangle {
                    id: airportRow
                    required property var modelData
                    width: 469*root.uiScale; height: 25*root.uiScale; radius: 3*root.uiScale
                    color: "#A407121A"; border.color: airportRow.modelData.assessment==="REACHABLE" ? root.green : (airportRow.modelData.assessment==="SIM_ESTIMATE" ? root.cyan : "#73553B")
                    Text { x: 8*root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: airportRow.modelData.icao+"  "+airportRow.modelData.name; color: "white"; font.pixelSize: 8*root.uiScale; font.bold: true }
                    Text { anchors { horizontalCenter: parent.horizontalCenter; verticalCenter: parent.verticalCenter } text: Number(airportRow.modelData.distance_nm).toFixed(1)+" NM  •  "+Math.round(airportRow.modelData.bearing_deg)+"°"; color: root.cyan; font.pixelSize: 8*root.uiScale; font.bold: true }
                    Text { anchors { right: parent.right; rightMargin: 8*root.uiScale; verticalCenter: parent.verticalCenter } text: airportRow.modelData.assessment; color: airportRow.modelData.assessment==="EXCLUDED" ? root.amber : root.green; font.pixelSize: 7*root.uiScale; font.bold: true }
                }
            }
        }
        Text {
            visible: root.controller.emergencyRankedAirports.length===0
            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 27*root.uiScale }
            text: "NO REACHABILITY CLAIM • AGL / TERRAIN / WIND / POLAR NOT VALIDATED"
            color: root.amber; font.pixelSize: 8*root.uiScale; font.bold: true
        }
    }

    Rectangle {
        id: managePanel
        x: 751*root.uiScale; y: 125*root.uiScale
        width: 265*root.uiScale; height: 378*root.uiScale
        radius: 5*root.uiScale; color: root.panel; border.color: "#6A583A"
        Text { x: 11*root.uiScale; y: 9*root.uiScale; text: "3  COMMUNICATE / MANAGE"; color: root.amber; font.pixelSize: 12*root.uiScale; font.bold: true }
        Rectangle {
            x: 10*root.uiScale; y: 35*root.uiScale; width: parent.width-20*root.uiScale; height: 57*root.uiScale; radius: 4*root.uiScale; color: "#9A07121A"
            Text { x: 8*root.uiScale; y: 5*root.uiScale; text: "RADIO DRAFT • VERIFY BEFORE USE"; color: root.amber; font.pixelSize: 7*root.uiScale; font.bold: true }
            Text { x: 8*root.uiScale; y: 21*root.uiScale; width: parent.width-16*root.uiScale; height: 30*root.uiScale; text: root.controller.emergencyCommunicationDraft; color: "white"; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; font.pixelSize: 8*root.uiScale }
        }
        Rectangle {
            x: 10*root.uiScale; y: 98*root.uiScale; width: parent.width-20*root.uiScale; height: 122*root.uiScale; radius: 4*root.uiScale
            color: "#B307121A"; border.color: root.controller.emergencyProcedureAvailable ? root.green : root.amber
            Text { x: 9*root.uiScale; y: 7*root.uiScale; text: root.controller.emergencyProcedureAvailable ? (root.controller.emergencySimulationOnly ? "SIM TEST PROCEDURE" : "APPROVED PROCEDURE") : "CHECKLIST BLOCKED"; color: root.controller.emergencyProcedureAvailable ? root.green : root.amber; font.pixelSize: 8*root.uiScale; font.bold: true }
            Text { x: 9*root.uiScale; y: 27*root.uiScale; width: parent.width-18*root.uiScale; text: root.controller.emergencyCurrentStep.phase || "MANAGE"; color: root.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
            Text { x: 9*root.uiScale; y: 45*root.uiScale; width: parent.width-18*root.uiScale; height: 66*root.uiScale; text: root.controller.emergencyCurrentStep.text || (root.controller.emergencyProcedureAvailable ? "Available after FLY acknowledgement." : "No approved POH/AFM procedure for this installed aircraft."); color: "white"; font.pixelSize: 10*root.uiScale; font.bold: true; wrapMode: Text.WordWrap; maximumLineCount: 4; elide: Text.ElideRight }
        }
        Row {
            x: 10*root.uiScale; y: 227*root.uiScale; spacing: 5*root.uiScale
            Repeater {
                model: [{t:"DONE",a:"done"},{t:"SKIP",a:"skip"},{t:"REPEAT",a:"repeat"}]
                Rectangle {
                    id: actionButton
                    required property var modelData
                    width: 78*root.uiScale; height: 34*root.uiScale; radius: 4*root.uiScale
                    color: actionButton.modelData.a==="done" ? "#17351F" : "#15242D"; border.color: actionButton.modelData.a==="done" ? root.green : "#58717E"
                    Text { anchors.centerIn: parent; text: actionButton.modelData.t; color: "white"; font.pixelSize: 7*root.uiScale; font.bold: true }
                    TapHandler { onTapped: { if(actionButton.modelData.a==="done") root.controller.completeEmergencyStep(); else if(actionButton.modelData.a==="skip") root.controller.skipEmergencyStep(); else root.controller.repeatEmergencyCue() } }
                }
            }
        }
        Rectangle {
            x: 10*root.uiScale; y: 268*root.uiScale; width: parent.width-20*root.uiScale; height: 52*root.uiScale; radius: 4*root.uiScale; color: "#93101B23"
            Text { x: 8*root.uiScale; y: 5*root.uiScale; text: "VOICE CUE"; color: root.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
            Text { x: 8*root.uiScale; y: 20*root.uiScale; width: parent.width-16*root.uiScale; height: 27*root.uiScale; text: root.controller.emergencyVoiceCue; color: "white"; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight; font.pixelSize: 8*root.uiScale; font.bold: true }
        }
        Text {
            x: 11*root.uiScale; y: 330*root.uiScale; width: parent.width-22*root.uiScale
            text: "EVENT  "+root.controller.emergencyEventSinkState+"  •  BLACKBOX  "+root.controller.blackBoxState+"\nAUDIO PLAYBACK ADAPTER PENDING"
            color: "#9FB0B8"; font.pixelSize: 7*root.uiScale; lineHeight: 1.35; elide: Text.ElideRight; maximumLineCount: 2
        }
    }

    Rectangle {
        x: 8*root.uiScale; y: 510*root.uiScale
        width: 1008*root.uiScale; height: 82*root.uiScale; radius: 5*root.uiScale
        color: "#F009141C"; border.color: "#445965"
        Rectangle { width: 5*root.uiScale; height: parent.height; color: root.red }
        Text { x: 14*root.uiScale; y: 10*root.uiScale; width: 660*root.uiScale; text: root.controller.emergencyAssistState+"  •  "+root.controller.emergencyAssistHeadline; color: "white"; font.pixelSize: 12*root.uiScale; font.bold: true; elide: Text.ElideRight }
        Text { x: 14*root.uiScale; y: 38*root.uiScale; width: 660*root.uiScale; text: "ADVISORY ONLY • NO PIXHAWK / SERVO / RADIO COMMANDS • PILOT CONFIRMATION REQUIRED"; color: root.red; font.pixelSize: 8*root.uiScale; font.bold: true }
        Rectangle {
            visible: root.controller.emergencyAssistState==="SUSPECTED" || root.controller.emergencyAssistKind==="ENGINE POWER LOSS"
            x: 720*root.uiScale; y: 14*root.uiScale; width: 132*root.uiScale; height: 53*root.uiScale; radius: 4*root.uiScale
            color: root.controller.emergencyAssistState==="SUSPECTED" ? "#382613" : "#18351D"; border.color: root.controller.emergencyAssistState==="SUSPECTED" ? root.amber : root.green
            Text { anchors.centerIn: parent; text: root.controller.emergencyAssistState==="SUSPECTED" ? "DISMISS\nSUSPICION" : "POWER\nRECOVERED"; horizontalAlignment: Text.AlignHCenter; color: "white"; font.pixelSize: 8*root.uiScale; font.bold: true }
            TapHandler { onTapped: { if(root.controller.emergencyAssistState==="SUSPECTED") { root.controller.dismissEmergencySuspicion(); root.closeRequested() } else root.controller.reportEmergencyPowerRecovered() } }
        }
        Rectangle {
            visible: root.controller.emergencyAssistState==="POWER_RECOVERED"
            x: 862*root.uiScale; y: 14*root.uiScale; width: 130*root.uiScale; height: 53*root.uiScale; radius: 4*root.uiScale
            color: "#202B34"; border.color: "#71848D"
            Text { anchors.centerIn: parent; text: "CONTINUE\nMONITORING"; horizontalAlignment: Text.AlignHCenter; color: "white"; font.pixelSize: 8*root.uiScale; font.bold: true }
            TapHandler { onTapped: root.controller.continueEmergencyMonitoring() }
        }
    }
}
