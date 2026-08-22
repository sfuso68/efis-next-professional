import QtQuick

Rectangle {
    id: setup
    required property var controller
    property real uiScale: Math.min(width/1024, height/600)
    property int section: 0
    signal closed()
    z: 200; color: "#F0051018"; border.color: "#28D9F4"
    readonly property color cyan: "#28D9F4"
    readonly property color green: "#52DE36"
    readonly property color amber: "#FFC72B"
    readonly property color line: "#40535E"
    readonly property color panel: "#E00A1821"
    function numberOr(name) { return typeof controller[name] === "number" ? controller[name] : 0 }
    function listOr(name) { var v=controller[name]; return v && typeof v.length === "number" ? v : [] }

    Rectangle { id: top; anchors { left:parent.left; right:parent.right; top:parent.top } height:54*setup.uiScale; color:"#07131C"; border.color:setup.line
        Text { x:18*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"EFIS"; color:"white"; font.pixelSize:18*setup.uiScale; font.bold:true }
        Text { x:62*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"NEXT"; color:setup.cyan; font.pixelSize:18*setup.uiScale; font.bold:true }
        Text { anchors.centerIn:parent; text:"EMS SETUP"; color:"white"; font.pixelSize:20*setup.uiScale; font.bold:true; font.letterSpacing:1.5 }
        Rectangle { anchors { right:parent.right; rightMargin:14*setup.uiScale; verticalCenter:parent.verticalCenter } width:108*setup.uiScale; height:34*setup.uiScale; color:"#142630"; border.color:setup.cyan
            Text { anchors.centerIn:parent; text:"CLOSE"; color:setup.cyan; font.pixelSize:10*setup.uiScale; font.bold:true }
            TapHandler { onTapped:setup.closed() }
        }
    }
    Rectangle { id:rail; anchors { left:parent.left; top:top.bottom; bottom:footer.top } width:184*setup.uiScale; color:"#09151D"; border.color:setup.line
        Column { anchors.fill:parent
            Repeater { model:["ENGINE","HOURS","SENSORS","ELECTRICAL","LOG & SERVICE"]
                delegate:Rectangle { required property string modelData; required property int index; width:rail.width; height:rail.height/5; color:setup.section===index?"#15303C":"transparent"; border.color:setup.line
                    Rectangle { visible:setup.section===index; width:5*setup.uiScale; height:parent.height; color:setup.cyan }
                    Text { x:22*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:modelData; color:setup.section===index?setup.cyan:"#AAB6BC"; font.pixelSize:11*setup.uiScale; font.bold:true }
                    TapHandler { onTapped:setup.section=index }
                }
            }
        }
    }
    Item { id:content; anchors { left:rail.right; right:parent.right; top:top.bottom; bottom:footer.top; margins:14*setup.uiScale }
        Text { text:["ENGINE INSTALLATION","RUNTIME COUNTERS","PROBE CONFIGURATION","ELECTRICAL ARCHITECTURE","LOGGING & MAINTENANCE"][setup.section]; color:"white"; font.pixelSize:18*setup.uiScale; font.bold:true }
        Text { y:29*setup.uiScale; text:"INDICATIVE until verified against aircraft and engine documentation"; color:setup.amber; font.pixelSize:8*setup.uiScale }

        Grid { visible:setup.section===0; y:58*setup.uiScale; columns:2; spacing:12*setup.uiScale
            Repeater { model:[
                {k:"AIRFRAME",v:"ASSO V JOLLY",s:"PIONEER 300 EQUIVALENT"},
                {k:"ENGINE",v:(controller.engineProfileName||"ROTAX 912 UL"),s:"4 CYL • 80 HP • 5800 RPM"},
                {k:"ENGINE SERIAL",v:(controller.engineSerialNumber||"TO BE ENTERED"),s:"REQUIRED BEFORE ACTIVATION"},
                {k:"CONFIGURATION",v:"OWNER BUILD",s:"DOCUMENT CHECK REQUIRED"},
                {k:"FUEL",v:"2 × 40 L",s:"LEVEL SENSORS TO CONFIRM"},
                {k:"AIRFRAME SYSTEMS",v:"RETRACT GEAR • FLAPS",s:"0 / 10 / 15 / 20 / 30°"}]
                delegate:Rectangle { required property var modelData; width:385*setup.uiScale; height:88*setup.uiScale; color:setup.panel; border.color:setup.line
                    Text { x:12*setup.uiScale; y:9*setup.uiScale; text:modelData.k; color:setup.cyan; font.pixelSize:8*setup.uiScale; font.bold:true }
                    Text { x:12*setup.uiScale; y:32*setup.uiScale; text:modelData.v; color:"white"; font.pixelSize:17*setup.uiScale; font.bold:true }
                    Text { x:12*setup.uiScale; y:65*setup.uiScale; text:modelData.s; color:"#9EADB4"; font.pixelSize:7*setup.uiScale }
                }
            }
        }
        Item { visible:setup.section===1; y:58*setup.uiScale; width:parent.width; height:parent.height-58*setup.uiScale
            Row { spacing:10*setup.uiScale
                Repeater { model:[{k:"MASTER",v:setup.numberOr("masterHours"),c:setup.cyan},{k:"ENGINE RUN",v:setup.numberOr("engineHours"),c:setup.green},{k:"TACH",v:setup.numberOr("tachHours"),c:setup.green},{k:"FLIGHT",v:setup.numberOr("flightHours"),c:setup.cyan}]
                    delegate:Rectangle { required property var modelData; width:190*setup.uiScale; height:126*setup.uiScale; color:setup.panel; border.color:modelData.c
                        Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:16*setup.uiScale } text:modelData.k; color:modelData.c; font.pixelSize:10*setup.uiScale; font.bold:true }
                        Text { anchors.centerIn:parent; text:modelData.v.toFixed(2); color:"white"; font.pixelSize:29*setup.uiScale; font.bold:true }
                        Text { anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:12*setup.uiScale } text:"HOURS"; color:"#AAB6BC"; font.pixelSize:8*setup.uiScale }
                    }
                }
            }
            Rectangle { y:150*setup.uiScale; width:790*setup.uiScale; height:92*setup.uiScale; color:setup.panel; border.color:setup.line
                Text { x:16*setup.uiScale; y:14*setup.uiScale; text:"ENGINE STATE"; color:setup.cyan; font.pixelSize:9*setup.uiScale; font.bold:true }
                Text { x:16*setup.uiScale; y:43*setup.uiScale; text:controller.engineRunning?"RUNNING":"STOPPED"; color:controller.engineRunning?setup.green:"#AAB6BC"; font.pixelSize:20*setup.uiScale; font.bold:true }
                Text { x:225*setup.uiScale; y:18*setup.uiScale; text:"RUN THRESHOLD  500 RPM\nTACH REFERENCE  5000 RPM\nPersistence: TEST MODE"; color:setup.amber; font.pixelSize:9*setup.uiScale; lineHeight:1.35 }
            }
        }
        Rectangle { visible:setup.section===2; y:58*setup.uiScale; width:parent.width; height:parent.height-58*setup.uiScale; color:setup.panel; border.color:setup.line
            ListView { anchors { fill:parent; margins:10*setup.uiScale } clip:true; spacing:3*setup.uiScale; model:setup.listOr("engineSensorConfiguration")
                delegate:Rectangle { required property var modelData; required property int index; width:ListView.view.width; height:38*setup.uiScale; color:index%2?"#0B1922":"#10222C"
                    Text { x:10*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:185*setup.uiScale; text:modelData.signal; color:"white"; font.pixelSize:10*setup.uiScale; font.bold:true }
                    Text { x:220*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"QTY  "+modelData.quantity; color:setup.cyan; font.pixelSize:9*setup.uiScale }
                    Text { x:330*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:modelData.type; color:String(modelData.type).indexOf("TO_BE")===0?setup.amber:setup.green; font.pixelSize:9*setup.uiScale }
                    Text { anchors { right:parent.right; rightMargin:12*setup.uiScale; verticalCenter:parent.verticalCenter } text:modelData.required?"REQUIRED":"OPTIONAL"; color:modelData.required?setup.green:"#AAB6BC"; font.pixelSize:8*setup.uiScale; font.bold:true }
                }
            }
        }
        Grid { visible:setup.section===3; y:58*setup.uiScale; columns:2; spacing:8*setup.uiScale
            Repeater { model:setup.listOr("electricalLoads"); delegate:Rectangle { required property var modelData; width:390*setup.uiScale; height:58*setup.uiScale; color:setup.panel; border.color:modelData.critical?setup.amber:setup.line
                Rectangle { x:10*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:12*setup.uiScale; height:12*setup.uiScale; radius:6*setup.uiScale; color:modelData.installed?setup.green:"#59646A" }
                Text { x:34*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:modelData.label; color:"white"; font.pixelSize:10*setup.uiScale; font.bold:true }
                Text { anchors { right:parent.right; rightMargin:12*setup.uiScale; verticalCenter:parent.verticalCenter } text:modelData.bus+(modelData.critical?" • ESSENTIAL":""); color:modelData.critical?setup.amber:setup.cyan; font.pixelSize:8*setup.uiScale }
            } }
        }
        Grid { visible:setup.section===4; y:58*setup.uiScale; columns:2; spacing:12*setup.uiScale
            Repeater { model:[{k:"BLACKBOX",v:(controller.blackBoxState||"OFFLINE")},{k:"LOCAL RECORDS",v:String(controller.blackBoxRecords||0)},{k:"NEXT SERVICE",v:"TO BE SET"},{k:"CONFIG AUDIT",v:"ACTIVE"},{k:"EXPORT",v:"CSV / PDF"},{k:"RETENTION",v:"500 HOURS"}]
                delegate:Rectangle { required property var modelData; width:385*setup.uiScale; height:88*setup.uiScale; color:setup.panel; border.color:setup.line
                    Text { x:12*setup.uiScale; y:10*setup.uiScale; text:modelData.k; color:setup.cyan; font.pixelSize:8*setup.uiScale; font.bold:true }
                    Text { x:12*setup.uiScale; y:35*setup.uiScale; text:modelData.v; color:"white"; font.pixelSize:17*setup.uiScale; font.bold:true }
                }
            }
        }
    }
    Rectangle { id:footer; anchors { left:parent.left; right:parent.right; bottom:parent.bottom } height:42*setup.uiScale; color:"#07131C"; border.color:setup.line
        Text { x:14*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"SETUP MODE • aircraft on ground • changes require verification"; color:setup.amber; font.pixelSize:8*setup.uiScale }
        Text { anchors { right:parent.right; rightMargin:14*setup.uiScale; verticalCenter:parent.verticalCenter } text:"PROFILE: ASSO-V-JOLLY-912-UL"; color:setup.cyan; font.pixelSize:8*setup.uiScale; font.bold:true }
    }
}
