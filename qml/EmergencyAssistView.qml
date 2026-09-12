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
    readonly property color panel: "#E60A1822"

    Rectangle { anchors.fill: parent; color: "#030A10" }
    Rectangle {
        x: 8*root.uiScale; y: 7*root.uiScale; width: parent.width-16*root.uiScale; height: 53*root.uiScale
        radius: 5*root.uiScale; color: "#20150A0D"; border.color: root.red; border.width: 1
        Text { x: 15*root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "EMERGENCY ASSIST"; color: root.red; font.pixelSize: 18*root.uiScale; font.bold: true }
        Text { anchors.centerIn: parent; text: root.controller.emergencyAssistKind; color: "white"; font.pixelSize: 17*root.uiScale; font.bold: true }
        Text { anchors { right: exit.left; rightMargin: 15*root.uiScale; verticalCenter: parent.verticalCenter }
            text: root.controller.emergencyDataQuality + "  •  " + root.controller.emergencyElapsedSeconds + " s";
            color: root.controller.emergencySimulationOnly ? root.red : root.amber; font.pixelSize: 9*root.uiScale; font.bold: true }
        Rectangle { id: exit; anchors { right: parent.right; rightMargin: 8*root.uiScale; verticalCenter: parent.verticalCenter }
            width: 73*root.uiScale; height: 36*root.uiScale; radius: 4*root.uiScale; color: "#251218"; border.color: root.red
            Text { anchors.centerIn: parent; text: "EXIT"; color: "white"; font.pixelSize: 11*root.uiScale; font.bold: true }
            TapHandler { onTapped: { root.controller.cancelEmergencyAssist("pilot exit"); root.closeRequested() } }
        }
    }

    Rectangle {
        visible: root.controller.emergencySimulationOnly
        x: 9*root.uiScale; y: 65*root.uiScale; width: parent.width-18*root.uiScale; height: 27*root.uiScale
        color: "#B0250808"; border.color: root.red
        Text { anchors.centerIn: parent; text: "SIMULATION • TEST DATA • NOT FOR FLIGHT"; color: "white"; font.pixelSize: 10*root.uiScale; font.bold: true; font.letterSpacing: 1.5*root.uiScale }
    }

    Row {
        x: 9*root.uiScale; y: 98*root.uiScale; spacing: 8*root.uiScale
        Repeater {
            model: [
                {id:"ENGINE_POWER_LOSS", label:"ENGINE", match:"ENGINE POWER LOSS"}, {id:"ABNORMAL_ATTITUDE", label:"ATTITUDE", match:"ABNORMAL ATTITUDE"},
                {id:"UNSAFE_AIRSPEED", label:"AIRSPEED", match:"UNSAFE AIRSPEED"}, {id:"ELECTRICAL_FAILURE", label:"ELECTRICAL", match:"ELECTRICAL FAILURE"},
                {id:"SMOKE_OR_FIRE", label:"FIRE / SMOKE", match:"SMOKE / FIRE"}, {id:"WEATHER_OR_TERRAIN", label:"WX / TERRAIN", match:"WEATHER / TERRAIN"},
                {id:"SENSOR_OR_PFD_LOSS", label:"SENSORS", match:"SENSOR / PFD LOSS"}, {id:"PILOT_INCAPACITATION", label:"MEDICAL", match:"PILOT INCAPACITATION"}
            ]
            Rectangle {
                required property var modelData
                width: 117*root.uiScale; height: 31*root.uiScale; radius: 3*root.uiScale
                color: root.controller.emergencyAssistKind===modelData.match ? "#3A32140D" : "#111E27"
                border.color: root.controller.emergencyAssistKind===modelData.match ? root.red : "#526773"
                Text { anchors.centerIn: parent; text: modelData.label; color: "white"; font.pixelSize: 7*root.uiScale; font.bold: true }
                TapHandler { onTapped: root.controller.confirmEmergencyAssist(modelData.id) }
            }
        }
    }

    Rectangle {
        id: flyPanel; x: 9*root.uiScale; y: 137*root.uiScale; width: 310*root.uiScale; height: 326*root.uiScale
        radius: 6*root.uiScale; color: root.panel; border.color: root.red
        Text { x: 13*root.uiScale; y: 10*root.uiScale; text: "1  FLY"; color: root.red; font.pixelSize: 18*root.uiScale; font.bold: true }
        Text { x: 13*root.uiScale; y: 49*root.uiScale; width: parent.width-26*root.uiScale; text: root.controller.emergencyFlyAdvisory; color: "white"; wrapMode: Text.WordWrap; font.pixelSize: 16*root.uiScale; font.bold: true }
        Rectangle { x: 13*root.uiScale; y: 116*root.uiScale; width: parent.width-26*root.uiScale; height: 74*root.uiScale; radius: 4*root.uiScale; color: "#07121A"; border.color: "#526773"
            Text { x: 10*root.uiScale; y: 8*root.uiScale; text: "IAS"; color: root.cyan; font.pixelSize: 8*root.uiScale }
            Text { x: 10*root.uiScale; y: 27*root.uiScale; text: root.controller.airspeedValid ? Math.round(root.controller.airspeed)+" KT" : "INVALID"; color: root.controller.airspeedValid?"white":root.red; font.pixelSize: 21*root.uiScale; font.bold: true }
            Text { x: 130*root.uiScale; y: 8*root.uiScale; text: "BEST GLIDE"; color: root.cyan; font.pixelSize: 8*root.uiScale }
            Text { x: 130*root.uiScale; y: 31*root.uiScale; text: root.controller.emergencyTargetSpeed; color: root.controller.emergencyTargetSpeed==="NOT APPROVED"?root.amber:"white"; font.pixelSize: 15*root.uiScale; font.bold: true }
        }
        Text { x: 13*root.uiScale; y: 203*root.uiScale; text: "DATA GATES"; color: root.cyan; font.pixelSize: 8*root.uiScale; font.bold: true }
        Text { x: 13*root.uiScale; y: 222*root.uiScale; width: parent.width-26*root.uiScale; height: 48*root.uiScale; text: root.controller.emergencyDegradationReasons.slice(0,3).join("\n"); color: root.amber; font.pixelSize: 8*root.uiScale; elide: Text.ElideRight; maximumLineCount: 3 }
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 13*root.uiScale }
            height: 39*root.uiScale; radius: 4*root.uiScale; color: "#17351F"; border.color: root.green
            Text { anchors.centerIn: parent; text: "FLY ACKNOWLEDGED"; color: "white"; font.pixelSize: 10*root.uiScale; font.bold: true }
            TapHandler { onTapped: root.controller.acknowledgeEmergencyFly() }
        }
    }

    Rectangle {
        id: navPanel; x: 327*root.uiScale; y: 137*root.uiScale; width: 370*root.uiScale; height: 326*root.uiScale
        radius: 6*root.uiScale; color: root.panel; border.color: root.cyan
        Text { x: 13*root.uiScale; y: 10*root.uiScale; text: "2  NAVIGATE"; color: root.cyan; font.pixelSize: 18*root.uiScale; font.bold: true }
        Text { x: 13*root.uiScale; y: 43*root.uiScale; width: parent.width-26*root.uiScale; text: root.controller.emergencyNavigationAdvisory; color: "white"; font.pixelSize: 11*root.uiScale; font.bold: true }
        Text { x: 13*root.uiScale; y: 69*root.uiScale; text: "CONSERVATIVE RANGE"; color: "#8FA8B4"; font.pixelSize: 7*root.uiScale }
        Text { x: 151*root.uiScale; y: 65*root.uiScale; text: root.controller.emergencyGlideRange; color: root.controller.emergencyGlideRange.indexOf("NOT")>=0?root.amber:root.cyan; font.pixelSize: 12*root.uiScale; font.bold: true }
        Column { x: 13*root.uiScale; y: 96*root.uiScale; spacing: 5*root.uiScale
            Repeater { model: root.controller.emergencyRankedAirports.slice(0,3)
                Rectangle { required property var modelData; width: 344*root.uiScale; height: 55*root.uiScale; radius: 4*root.uiScale; color: "#091621"; border.color: modelData.assessment==="REACHABLE"?root.green:(modelData.assessment==="SIM_ESTIMATE"?root.cyan:"#6F4F35")
                    Text { x: 9*root.uiScale; y: 6*root.uiScale; text: modelData.icao+"  "+modelData.name; color: "white"; font.pixelSize: 10*root.uiScale; font.bold: true }
                    Text { x: 9*root.uiScale; y: 29*root.uiScale; text: Number(modelData.distance_nm).toFixed(1)+" NM  •  "+modelData.assessment; color: modelData.assessment==="EXCLUDED"?root.amber:root.cyan; font.pixelSize: 8*root.uiScale; font.bold: true }
                    Text { anchors { right:parent.right; rightMargin:8*root.uiScale; bottom:parent.bottom; bottomMargin:7*root.uiScale } text:modelData.reason; color:"#9BACB4"; font.pixelSize:6*root.uiScale }
                }
            }
        }
        Text { visible: root.controller.emergencyRankedAirports.length===0; anchors.centerIn: parent; text: "NO REACHABILITY CLAIM\nMISSING VALIDATED AGL / TERRAIN / WIND / POLAR"; horizontalAlignment: Text.AlignHCenter; color: root.amber; font.pixelSize: 10*root.uiScale; font.bold: true }
    }

    Rectangle {
        x: 705*root.uiScale; y: 137*root.uiScale; width: 310*root.uiScale; height: 326*root.uiScale
        radius: 6*root.uiScale; color: root.panel; border.color: root.amber
        Text { x: 13*root.uiScale; y: 10*root.uiScale; text: "3  COMMUNICATE / MANAGE"; color: root.amber; font.pixelSize: 14*root.uiScale; font.bold: true }
        Text { x: 13*root.uiScale; y: 45*root.uiScale; width: parent.width-26*root.uiScale; text: root.controller.emergencyCommunicationDraft; color: "white"; font.pixelSize: 10*root.uiScale; wrapMode: Text.WordWrap }
        Rectangle { x:13*root.uiScale; y:111*root.uiScale; width:parent.width-26*root.uiScale; height:91*root.uiScale; radius:4*root.uiScale; color:"#091621"; border.color:root.controller.emergencyProcedureAvailable?root.green:root.amber
            Text { x:9*root.uiScale; y:7*root.uiScale; text:root.controller.emergencyProcedureAvailable?(root.controller.emergencySimulationOnly?"SIM TEST PROCEDURE":"APPROVED PROCEDURE"):"CHECKLIST BLOCKED"; color:root.controller.emergencyProcedureAvailable?root.green:root.amber; font.pixelSize:9*root.uiScale; font.bold:true }
            Text { x:9*root.uiScale; y:31*root.uiScale; width:parent.width-18*root.uiScale; text:root.controller.emergencyCurrentStep.text || (root.controller.emergencyProcedureAvailable ? "Available after FLY acknowledgement." : "No POH/AFM procedure approved for this installed aircraft."); color:"white"; font.pixelSize:9*root.uiScale; wrapMode:Text.WordWrap }
        }
        Row { x:13*root.uiScale; y:212*root.uiScale; spacing:7*root.uiScale
            Repeater { model:[{t:"DONE",a:"done"},{t:"SKIP",a:"skip"},{t:"REPEAT",a:"repeat"}]
                Rectangle { required property var modelData; width:88*root.uiScale; height:36*root.uiScale; radius:4*root.uiScale; color:"#13232D"; border.color:"#58717E"
                    Text { anchors.centerIn:parent; text:modelData.t; color:"white"; font.pixelSize:8*root.uiScale; font.bold:true }
                    TapHandler { onTapped:{ if(modelData.a==="done")root.controller.completeEmergencyStep(); else if(modelData.a==="skip")root.controller.skipEmergencyStep(); else root.controller.repeatEmergencyCue() } }
                }
            }
        }
        Text { x:13*root.uiScale; y:263*root.uiScale; width:parent.width-26*root.uiScale; text:"EVENT LOG  "+root.controller.emergencyEventSinkState+"\nBLACKBOX  "+root.controller.blackBoxState+"\nVOICE  CUE QUEUED / AUDIO ADAPTER PENDING"; color:"#9FB0B8"; font.pixelSize:8*root.uiScale; lineHeight:1.35; elide:Text.ElideRight; maximumLineCount:3 }
    }

    Rectangle { x:9*root.uiScale; y:472*root.uiScale; width:1006*root.uiScale; height:119*root.uiScale; radius:6*root.uiScale; color:"#0A151E"; border.color:"#445965"
        Text { x:13*root.uiScale; y:9*root.uiScale; text:"STATE  "+root.controller.emergencyAssistState+"  •  "+root.controller.emergencyAssistHeadline; color:"white"; font.pixelSize:12*root.uiScale; font.bold:true }
        Text { x:13*root.uiScale; y:39*root.uiScale; width:700*root.uiScale; text:"NO FLIGHT-CONTROL AUTHORITY • NO PIXHAWK COMMANDS • PILOT CONFIRMATION REQUIRED"; color:root.red; font.pixelSize:9*root.uiScale; font.bold:true }
        Rectangle { visible:root.controller.emergencyAssistState==="SUSPECTED" || root.controller.emergencyAssistKind==="ENGINE POWER LOSS"; x:722*root.uiScale; y:51*root.uiScale; width:126*root.uiScale; height:46*root.uiScale; radius:4*root.uiScale; color:root.controller.emergencyAssistState==="SUSPECTED"?"#382613":"#18351D"; border.color:root.controller.emergencyAssistState==="SUSPECTED"?root.amber:root.green
            Text{anchors.centerIn:parent;text:root.controller.emergencyAssistState==="SUSPECTED"?"DISMISS\nSUSPICION":"POWER\nRECOVERED";horizontalAlignment:Text.AlignHCenter;color:"white";font.pixelSize:8*root.uiScale;font.bold:true}
            TapHandler{onTapped:{if(root.controller.emergencyAssistState==="SUSPECTED"){root.controller.dismissEmergencySuspicion();root.closeRequested()}else root.controller.reportEmergencyPowerRecovered()}}
        }
        Rectangle { visible:root.controller.emergencyAssistState==="POWER_RECOVERED"; x:858*root.uiScale; y:51*root.uiScale; width:130*root.uiScale; height:46*root.uiScale; radius:4*root.uiScale; color:"#202B34"; border.color:"#71848D"
            Text{anchors.centerIn:parent;text:"CONTINUE\nMONITORING";horizontalAlignment:Text.AlignHCenter;color:"white";font.pixelSize:8*root.uiScale;font.bold:true}
            TapHandler{onTapped:root.controller.continueEmergencyMonitoring()}
        }
    }
}
