import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    required property var controller
    property real uiScale: Math.min(width/1024, height/600)
    signal closeRequested()
    z: 300; color: "#F2051018"; border.color: "#28D9F4"
    readonly property color cyan: "#28D9F4"
    readonly property color green: "#52DE36"
    readonly property color amber: "#FFC72B"
    readonly property color panel: "#E20A1821"
    readonly property var scenarios: [
        {id:"mission-full-flight", title:"SYNTHETIC MISSION", note:"Treviso → Belluno • kinematic"},
        {id:"mission-engine-emergency", title:"ENGINE EMERGENCY", note:"Synthetic oil pressure + CHT"},
        {id:"mission-electrical-failure", title:"ELECTRICAL FAILURE", note:"Synthetic alternator and bus"},
        {id:"mission-sensor-degradation", title:"SENSOR DEGRADATION", note:"Synthetic IAS, ALT and GNSS"}
    ]

    Rectangle { anchors { left:parent.left; right:parent.right; top:parent.top } height:58*root.uiScale; color:"#07131C"; border.color:"#40535E"
        Text { x:18*root.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"EFIS NEXT  •  SYSTEM TEST CONSOLE"; color:"white"; font.pixelSize:18*root.uiScale; font.bold:true }
        Rectangle { anchors { right:parent.right; rightMargin:14*root.uiScale; verticalCenter:parent.verticalCenter } width:110*root.uiScale; height:34*root.uiScale; color:"#142630"; border.color:root.cyan
            Text { anchors.centerIn:parent; text:"CLOSE"; color:root.cyan; font.pixelSize:10*root.uiScale; font.bold:true }
            TapHandler { onTapped:root.closeRequested() }
        }
    }
    Text { x:18*root.uiScale; y:70*root.uiScale; text:"SIMULATION — NOT FOR FLIGHT • NO AUTOPILOT OUTPUT"; color:root.amber; font.pixelSize:10*root.uiScale; font.bold:true }

    Column { x:18*root.uiScale; y:104*root.uiScale; width:300*root.uiScale; spacing:8*root.uiScale
        Repeater { model:root.scenarios
            delegate:Rectangle { required property var modelData; objectName:"scenario-"+modelData.id; width:300*root.uiScale; height:75*root.uiScale; color:controller.systemSimulationScenario===modelData.id&&controller.systemSimulationActive?"#153746":root.panel; border.color:controller.systemSimulationScenario===modelData.id&&controller.systemSimulationActive?root.cyan:"#40535E"
                Text { x:12*root.uiScale; y:12*root.uiScale; text:modelData.title; color:"white"; font.pixelSize:12*root.uiScale; font.bold:true }
                Text { x:12*root.uiScale; y:41*root.uiScale; text:modelData.note; color:"#9EADB4"; font.pixelSize:8*root.uiScale }
                TapHandler { onTapped:controller.startSystemSimulation(modelData.id) }
            }
        }
    }

    Rectangle { x:336*root.uiScale; y:104*root.uiScale; width:670*root.uiScale; height:370*root.uiScale; color:root.panel; border.color:"#40535E"
        Text { x:18*root.uiScale; y:16*root.uiScale; text:controller.systemSimulationScenario.toUpperCase(); color:root.cyan; font.pixelSize:13*root.uiScale; font.bold:true }
        Text { x:18*root.uiScale; y:48*root.uiScale; text:controller.systemSimulationPhase; color:"white"; font.pixelSize:30*root.uiScale; font.bold:true }
        Text { objectName:"missionTimelineText"; anchors { right:parent.right; rightMargin:18*root.uiScale; top:parent.top; topMargin:18*root.uiScale } text:Math.round(controller.systemSimulationProgress*controller.systemSimulationDurationSeconds)+" / "+Math.round(controller.systemSimulationDurationSeconds)+" s"; color:root.amber; font.pixelSize:14*root.uiScale; font.bold:true }

        Rectangle { x:18*root.uiScale; y:102*root.uiScale; width:634*root.uiScale; height:20*root.uiScale; color:"#071018"; border.color:"#60717A"
            Rectangle { width:parent.width*controller.systemSimulationProgress; height:parent.height; color:root.cyan }
            TapHandler { onTapped:function(point){ controller.seekSystemSimulation(point.position.x/parent.width) } }
        }
        Row { x:18*root.uiScale; y:144*root.uiScale; spacing:10*root.uiScale
            Repeater { model:[{t:controller.systemSimulationRunning?"PAUSE":"START / RESUME",a:"toggle"},{t:"STOP",a:"stop"},{t:"0.5×",a:"0.5"},{t:"1×",a:"1"},{t:"5×",a:"5"},{t:"20×",a:"20"}]
                delegate:Rectangle { required property var modelData; width:modelData.a==="toggle"?140*root.uiScale:82*root.uiScale; height:42*root.uiScale; color:modelData.a==="toggle"?"#12402A":"#142630"; border.color:modelData.a==="stop"?root.amber:root.cyan
                    Text { anchors.centerIn:parent; text:modelData.t; color:"white"; font.pixelSize:9*root.uiScale; font.bold:true }
                    TapHandler { onTapped:{ if(modelData.a==="toggle")controller.toggleSystemSimulation(); else if(modelData.a==="stop")controller.stopSystemSimulation(); else controller.setSystemSimulationSpeed(Number(modelData.a)); } }
                }
            }
        }
        Grid { x:18*root.uiScale; y:218*root.uiScale; columns:4; spacing:8*root.uiScale
            Repeater { model:[{k:"IAS",v:controller.airspeed.toFixed(0)+" KT"},{k:"ALT",v:controller.altitude.toFixed(0)+" FT"},{k:"RPM",v:controller.engineRpm.toFixed(0)},{k:"BUS",v:controller.busVoltage.toFixed(1)+" V"},{k:"GEAR",v:controller.gearState},{k:"FLAPS",v:controller.flapAngleValid===true&&isFinite(controller.flapAngle)?controller.flapAngle.toFixed(0)+"°":"—"},{k:"FAULT",v:controller.engineFaultMode},{k:"SPEED",v:controller.systemSimulationSpeed.toFixed(2)+"×"}]
                delegate:Rectangle { required property var modelData; width:152*root.uiScale; height:58*root.uiScale; color:"#0B1922"; border.color:"#304550"
                    Text { x:8*root.uiScale; y:7*root.uiScale; text:modelData.k; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
                    Text { x:8*root.uiScale; y:27*root.uiScale; width:parent.width-16*root.uiScale; text:modelData.v; color:"white"; font.pixelSize:11*root.uiScale; font.bold:true; elide:Text.ElideRight }
                }
            }
        }
        Text { x:18*root.uiScale; anchors { bottom:parent.bottom; bottomMargin:12*root.uiScale } text:"LAST EVENT  •  "+controller.systemSimulationLastEvent; color:root.amber; font.pixelSize:9*root.uiScale; font.bold:true }
    }
    Rectangle { anchors { left:parent.left; right:parent.right; bottom:parent.bottom } height:48*root.uiScale; color:"#07131C"; border.color:"#40535E"
        Text { x:18*root.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"All generated samples retain simulated provenance. Engine-hour authority remains isolated."; color:"#AAB6BC"; font.pixelSize:8*root.uiScale }
        Text { anchors { right:parent.right; rightMargin:18*root.uiScale; verticalCenter:parent.verticalCenter } text:controller.systemSimulationActive?(controller.systemSimulationRunning?"RUNNING":"PAUSED"):"STANDBY"; color:controller.systemSimulationRunning?root.green:root.amber; font.pixelSize:11*root.uiScale; font.bold:true }
    }
}
