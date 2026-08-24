import QtQuick

Rectangle {
    id: setup
    required property var controller
    property real uiScale: Math.min(width/1024, height/600)
    property int section: 0
    property int requestedSection: 0
    property int monitorSensor: -1
    property bool showFuelHistory: false
    onVisibleChanged: if(visible)section=requestedSection
    signal closed()
    z: 200; color: "#F0051018"; border.color: "#28D9F4"
    readonly property color cyan: "#28D9F4"
    readonly property color green: "#52DE36"
    readonly property color amber: "#FFC72B"
    readonly property color line: "#40535E"
    readonly property color panel: "#E00A1821"
    function numberOr(name) { return typeof controller[name] === "number" ? controller[name] : 0 }
    function listOr(name) { var v=controller[name]; return v && typeof v.length === "number" ? v : [] }
    function busState(bus) { var m=String(controller.electricalTestMode||"NORMAL"); if(m==="ESSENTIAL BUS ISOLATED"&&bus==="ESSENTIAL")return "ISOLATED"; if(m==="MAIN BUS LOW"&&(bus==="MAIN"||bus==="AVIONICS"))return "LOW VOLTAGE"; return m==="ALTERNATOR OFFLINE"?"BATTERY SUPPLY":"POWERED" }
    function loadPowered(load) { return load.installed && setup.busState(String(load.bus))!=="ISOLATED" }

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
        Text { y:29*setup.uiScale; text:"LIMIT AUTHORITY • "+String((controller.engineLimits||{}).status||"NOT CONFIGURED")+" • verify against installed-engine documentation"; color:String((controller.engineLimits||{}).status||"").indexOf("VERIFIED")===0?setup.green:setup.amber; font.pixelSize:8*setup.uiScale }

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
                Text { x:225*setup.uiScale; y:12*setup.uiScale; text:"RUN THRESHOLD  500 RPM\nTACH REFERENCE  5000 RPM\n"+(controller.runtimeAuthority||"PFD • DISPLAY MIRROR")+"\nSTORE  "+(controller.runtimePersistenceState||"WAITING FOR EMS NODE")+"  REV "+String(controller.runtimeRevision||0); color:setup.amber; font.pixelSize:8*setup.uiScale; lineHeight:1.25 }
            }
        }
        Rectangle { visible:setup.section===2; y:58*setup.uiScale; width:parent.width; height:parent.height-58*setup.uiScale; color:setup.panel; border.color:setup.line
            Rectangle { anchors { left:parent.left; right:parent.right; top:parent.top } height:38*setup.uiScale; color:"#10232D"; border.color:setup.line
                Text { x:12*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"CALIBRATION WORKSHEET"; color:setup.cyan; font.pixelSize:9*setup.uiScale; font.bold:true }
                Text { anchors.centerIn:parent; text:"REV "+String(controller.calibrationRevision||0); color:"#AAB6BC"; font.pixelSize:8*setup.uiScale }
                Text { anchors { right:parent.right; rightMargin:12*setup.uiScale; verticalCenter:parent.verticalCenter } text:controller.sensorCalibrationReady?"✓ REQUIRED PROBES VERIFIED":"ACTION REQUIRED"; color:controller.sensorCalibrationReady?setup.green:setup.amber; font.pixelSize:9*setup.uiScale; font.bold:true }
            }
            ListView { anchors { left:parent.left; right:parent.right; top:parent.top; bottom:parent.bottom; margins:10*setup.uiScale; topMargin:46*setup.uiScale } clip:true; spacing:3*setup.uiScale; model:setup.listOr("engineSensorConfiguration")
                delegate:Rectangle { required property var modelData; required property int index; width:ListView.view.width; height:38*setup.uiScale; color:index%2?"#0B1922":"#10222C"; border.color:String(modelData.calibration_state)==="VERIFIED"?setup.green:"transparent"
                    Text { x:10*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:140*setup.uiScale; elide:Text.ElideRight; text:String(modelData.signal).replace("_"," ")+"  ›"; color:"white"; font.pixelSize:8*setup.uiScale; font.bold:true
                        TapHandler { onTapped:{setup.monitorSensor=index;controller.startEngineSensorTest(String(modelData.signal),true)} } }
                    Text { x:150*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"×"+modelData.quantity; color:setup.cyan; font.pixelSize:8*setup.uiScale }
                    Rectangle { x:184*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:180*setup.uiScale; height:27*setup.uiScale; radius:3*setup.uiScale; color:"#102B36"; border.color:String(modelData.configured_type).indexOf("TO_BE")===0||String(modelData.configured_type)==="UNSELECTED"?setup.amber:setup.cyan
                        Text { anchors.centerIn:parent; width:parent.width-8*setup.uiScale; horizontalAlignment:Text.AlignHCenter; elide:Text.ElideRight; text:String(modelData.configured_type||modelData.type)+"  ›"; color:"white"; font.pixelSize:7*setup.uiScale; font.bold:true }
                        TapHandler { onTapped:controller.cycleSensorType(index) }
                    }
                    Rectangle { x:372*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:98*setup.uiScale; height:27*setup.uiScale; radius:3*setup.uiScale; color:"#102B36"; border.color:String(modelData.input)==="UNASSIGNED"?setup.amber:setup.cyan
                        Text { anchors.centerIn:parent; text:String(modelData.input||"UNASSIGNED")+"  ›"; color:String(modelData.input)==="UNASSIGNED"?setup.amber:"white"; font.pixelSize:7*setup.uiScale; font.bold:true }
                        TapHandler { onTapped:controller.cycleSensorInput(index) }
                    }
                    Text { x:478*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:modelData.required?"REQ":"OPT"; color:modelData.required?setup.green:"#AAB6BC"; font.pixelSize:7*setup.uiScale; font.bold:true }
                    Rectangle { anchors { right:parent.right; rightMargin:8*setup.uiScale; verticalCenter:parent.verticalCenter } width:190*setup.uiScale; height:27*setup.uiScale; radius:3*setup.uiScale; color:"#142C37"; border.color:String(modelData.calibration_state)==="VERIFIED"?setup.green:setup.amber
                        Text { x:5*setup.uiScale; width:112*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; horizontalAlignment:Text.AlignHCenter; elide:Text.ElideRight; text:String(modelData.calibration_state||"NOT CONFIGURED"); color:String(modelData.calibration_state)==="VERIFIED"?setup.green:setup.amber; font.pixelSize:7*setup.uiScale; font.bold:true
                            TapHandler { onTapped:controller.cycleSensorCalibration(index) } }
                        Rectangle { x:122*setup.uiScale; width:30*setup.uiScale; height:parent.height; color:modelData.zero_raw!==undefined?"#174A38":"#243943"; border.color:setup.cyan
                            Text { anchors.centerIn:parent; text:"Z"; color:"white"; font.pixelSize:8*setup.uiScale; font.bold:true }
                            TapHandler { onTapped:controller.captureSensorPoint(index,"ZERO") } }
                        Rectangle { anchors.right:parent.right; width:30*setup.uiScale; height:parent.height; color:modelData.span_raw!==undefined?"#174A38":"#243943"; border.color:setup.cyan
                            Text { anchors.centerIn:parent; text:"S"; color:"white"; font.pixelSize:8*setup.uiScale; font.bold:true }
                            TapHandler { onTapped:controller.captureSensorPoint(index,"SPAN") } }
                    }
                }
            }
        }
        Rectangle { visible:setup.section===2&&setup.monitorSensor>=0; anchors.centerIn:parent; width:660*setup.uiScale; height:310*setup.uiScale; z:20; radius:7*setup.uiScale; color:"#F006151E"; border.width:2; border.color:setup.cyan
            readonly property var sensor:setup.monitorSensor>=0?setup.listOr("engineSensorConfiguration")[setup.monitorSensor]:({})
            Text { x:18*setup.uiScale; y:14*setup.uiScale; text:"LIVE CHANNEL • "+String(parent.sensor.signal||""); color:"white"; font.pixelSize:16*setup.uiScale; font.bold:true }
            Text { x:18*setup.uiScale; y:42*setup.uiScale; text:String(controller.sensorTestStatus||"IDLE")+"  •  "+String(parent.sensor.input||"UNASSIGNED")+"  •  "+String(parent.sensor.configured_type||""); color:String(controller.sensorTestStatus).indexOf("VALID")>=0?setup.green:setup.amber; font.pixelSize:9*setup.uiScale; font.bold:true }
            Grid { x:18*setup.uiScale; y:78*setup.uiScale; columns:2; spacing:10*setup.uiScale
                Repeater { model:[{k:"RAW INPUT",v:Number(controller.sensorTestRaw||0).toFixed(2),u:"ADC / BUS"},{k:"CONVERTED",v:Number(controller.sensorTestValue||0).toFixed(2),u:"ENGINE UNIT"},{k:"UPDATE RATE",v:Number(controller.sensorTestRateHz||0).toFixed(1),u:"HZ"},{k:"SAMPLE AGE",v:String(controller.sensorTestAgeMs||0),u:"MS"}]
                    delegate:Rectangle { required property var modelData; width:300*setup.uiScale; height:72*setup.uiScale; color:setup.panel; border.color:setup.line
                        Text { x:10*setup.uiScale; y:8*setup.uiScale; text:modelData.k; color:setup.cyan; font.pixelSize:7*setup.uiScale; font.bold:true }
                        Text { x:10*setup.uiScale; y:27*setup.uiScale; text:modelData.v; color:"white"; font.pixelSize:20*setup.uiScale; font.bold:true }
                        Text { anchors { right:parent.right; rightMargin:10*setup.uiScale; bottom:parent.bottom; bottomMargin:9*setup.uiScale } text:modelData.u; color:"#96A7AE"; font.pixelSize:7*setup.uiScale }
                    }
                }
            }
            Text { x:18*setup.uiScale; y:238*setup.uiScale; width:350*setup.uiScale; text:"BENCH data exercises diagnostics only. LIVE verification remains locked until the configured physical provider is present."; wrapMode:Text.WordWrap; color:setup.amber; font.pixelSize:7*setup.uiScale }
            Rectangle { x:382*setup.uiScale; y:232*setup.uiScale; width:150*setup.uiScale; height:38*setup.uiScale; radius:3*setup.uiScale; color:String(controller.sensorTestDiagnostic)==="NORMAL"?"#123A2A":"#4A2118"; border.color:String(controller.sensorTestDiagnostic)==="NORMAL"?setup.green:setup.amber
                Text { anchors.centerIn:parent; width:parent.width-8*setup.uiScale; horizontalAlignment:Text.AlignHCenter; text:"FAULT TEST\n"+String(controller.sensorTestDiagnostic||"NORMAL")+"  ›"; color:"white"; font.pixelSize:7*setup.uiScale; font.bold:true }
                TapHandler { onTapped:controller.cycleSensorTestDiagnostic() }
            }
            Rectangle { anchors { right:parent.right; bottom:parent.bottom; margins:16*setup.uiScale } width:120*setup.uiScale; height:34*setup.uiScale; color:"#15303C"; border.color:setup.cyan
                Text { anchors.centerIn:parent; text:"CLOSE"; color:setup.cyan; font.pixelSize:9*setup.uiScale; font.bold:true }
                TapHandler { onTapped:{controller.stopEngineSensorTest();setup.monitorSensor=-1} }
            }
        }
        Item { visible:setup.section===3; y:58*setup.uiScale; width:parent.width; height:parent.height-58*setup.uiScale
            Rectangle { anchors { left:parent.left; right:parent.right; top:parent.top } height:42*setup.uiScale; color:"#10232D"; border.color:setup.line
                Text { x:12*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"POWER DISTRIBUTION • MAIN / ESSENTIAL / AVIONICS"; color:setup.cyan; font.pixelSize:9*setup.uiScale; font.bold:true }
                Rectangle { anchors { right:parent.right; rightMargin:8*setup.uiScale; verticalCenter:parent.verticalCenter } width:210*setup.uiScale; height:29*setup.uiScale; radius:3*setup.uiScale; color:String(controller.electricalTestMode)==="NORMAL"?"#123A2A":"#4A2118"; border.color:String(controller.electricalTestMode)==="NORMAL"?setup.green:setup.amber
                    Text { anchors.centerIn:parent; text:"BENCH FAULT • "+String(controller.electricalTestMode||"NORMAL")+"  ›"; color:"white"; font.pixelSize:7*setup.uiScale; font.bold:true }
                    TapHandler { onTapped:controller.cycleElectricalTestMode() }
                }
            }
            Row { y:52*setup.uiScale; width:parent.width; height:parent.height-52*setup.uiScale; spacing:10*setup.uiScale
                Column { width:175*setup.uiScale; spacing:7*setup.uiScale
                    Text { text:"SOURCES"; color:setup.cyan; font.pixelSize:8*setup.uiScale; font.bold:true }
                    Repeater { model:setup.listOr("electricalSources"); delegate:Rectangle { required property var modelData; width:parent.width; height:60*setup.uiScale; color:setup.panel; border.color:(String(controller.electricalTestMode)==="ALTERNATOR OFFLINE"&&modelData.type==="ALTERNATOR")||(String(controller.electricalTestMode)==="BATTERY LOW"&&modelData.type==="BATTERY")?setup.amber:setup.green
                        Text { x:9*setup.uiScale; y:8*setup.uiScale; text:modelData.type; color:"white"; font.pixelSize:9*setup.uiScale; font.bold:true }
                        Text { x:9*setup.uiScale; y:31*setup.uiScale; text:!modelData.installed?"NOT INSTALLED":((String(controller.electricalTestMode)==="ALTERNATOR OFFLINE"&&modelData.type==="ALTERNATOR")?"OFFLINE":((String(controller.electricalTestMode)==="BATTERY LOW"&&modelData.type==="BATTERY")?"LOW":"ONLINE")); color:text==="ONLINE"?setup.green:setup.amber; font.pixelSize:7*setup.uiScale; font.bold:true }
                    } }
                }
                Column { width:205*setup.uiScale; spacing:7*setup.uiScale
                    Text { text:"DISTRIBUTION BUSES"; color:setup.cyan; font.pixelSize:8*setup.uiScale; font.bold:true }
                    Repeater { model:setup.listOr("electricalBuses"); delegate:Rectangle { required property var modelData; width:parent.width; height:60*setup.uiScale; color:"#102630"; border.width:2; border.color:setup.busState(String(modelData.id))==="POWERED"?setup.green:setup.amber
                        Text { x:9*setup.uiScale; y:8*setup.uiScale; text:modelData.label; color:"white"; font.pixelSize:9*setup.uiScale; font.bold:true }
                        Text { x:9*setup.uiScale; y:31*setup.uiScale; text:setup.busState(String(modelData.id)); color:text==="POWERED"?setup.green:setup.amber; font.pixelSize:7*setup.uiScale; font.bold:true }
                        Text { anchors { right:parent.right; rightMargin:9*setup.uiScale; verticalCenter:parent.verticalCenter } text:modelData.id==="MAIN"?Number(controller.busVoltage||0).toFixed(1)+" V":"12.8 V"; color:setup.cyan; font.pixelSize:10*setup.uiScale; font.bold:true }
                    } }
                }
                Grid { width:390*setup.uiScale; columns:2; spacing:6*setup.uiScale
                    Repeater { model:setup.listOr("electricalLoads"); delegate:Rectangle { required property var modelData; width:190*setup.uiScale; height:51*setup.uiScale; color:setup.panel; border.color:modelData.critical?setup.amber:setup.line
                        Rectangle { x:8*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:10*setup.uiScale; height:10*setup.uiScale; radius:5*setup.uiScale; color:setup.loadPowered(modelData)?setup.green:"#D83B32" }
                        Text { x:25*setup.uiScale; y:8*setup.uiScale; width:150*setup.uiScale; elide:Text.ElideRight; text:modelData.label; color:"white"; font.pixelSize:8*setup.uiScale; font.bold:true }
                        Text { x:25*setup.uiScale; y:29*setup.uiScale; text:modelData.bus+" • "+(setup.loadPowered(modelData)?"POWERED":"SHED"); color:setup.loadPowered(modelData)?setup.cyan:setup.amber; font.pixelSize:6*setup.uiScale; font.bold:true }
                    } }
                }
            }
        }
        Item { visible:setup.section===4; y:58*setup.uiScale; width:parent.width; height:parent.height-58*setup.uiScale
            Rectangle { anchors { left:parent.left; right:parent.right; top:parent.top } height:46*setup.uiScale; color:"#10232D"; border.color:setup.line
                Text { x:12*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"ROTAX 912 UL • MAINTENANCE PROGRAM"; color:"white"; font.pixelSize:10*setup.uiScale; font.bold:true }
                Text { anchors.centerIn:parent; text:"ENGINE  "+setup.numberOr("engineHours").toFixed(2)+" H"; color:setup.cyan; font.pixelSize:10*setup.uiScale; font.bold:true }
                Text { anchors { right:parent.right; rightMargin:12*setup.uiScale; verticalCenter:parent.verticalCenter } text:String(controller.maintenanceStatus||"PROGRAM REQUIRED"); color:text==="CURRENT"?setup.green:setup.amber; font.pixelSize:9*setup.uiScale; font.bold:true }
                Rectangle { x:555*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:130*setup.uiScale; height:29*setup.uiScale; radius:3*setup.uiScale; color:"#15303C"; border.color:setup.cyan
                    Text { anchors.centerIn:parent; text:"BLACKBOX "+String(controller.centralBlackBoxState||"OFFLINE"); color:String(controller.centralBlackBoxState)==="ONLINE"?setup.green:setup.amber; font.pixelSize:7*setup.uiScale; font.bold:true }
                    TapHandler { onTapped:setup.showFuelHistory=true }
                }
            }
            Grid { y:56*setup.uiScale; columns:2; spacing:8*setup.uiScale
                Repeater { model:setup.listOr("maintenanceTasks"); delegate:Rectangle { required property var modelData; required property int index; width:390*setup.uiScale; height:86*setup.uiScale; color:setup.panel; border.color:modelData.interval_hours>0?setup.line:setup.amber
                    Text { x:10*setup.uiScale; y:8*setup.uiScale; width:215*setup.uiScale; elide:Text.ElideRight; text:modelData.label; color:"white"; font.pixelSize:9*setup.uiScale; font.bold:true }
                    Text { x:10*setup.uiScale; y:31*setup.uiScale; text:modelData.interval_hours>0?("LAST  "+Number(modelData.last_hours).toFixed(1)+" H  •  NEXT  "+(Number(modelData.last_hours)+Number(modelData.interval_hours)).toFixed(1)+" H"):"PROGRAM REQUIRED"; color:modelData.interval_hours>0?setup.cyan:setup.amber; font.pixelSize:7*setup.uiScale; font.bold:true }
                    Text { x:10*setup.uiScale; y:53*setup.uiScale; text:"RECORDED  "+String(modelData.last_utc||"NEVER"); color:"#91A2AA"; font.pixelSize:6*setup.uiScale }
                    Rectangle { x:232*setup.uiScale; y:8*setup.uiScale; width:72*setup.uiScale; height:30*setup.uiScale; radius:3*setup.uiScale; color:"#15303C"; border.color:setup.cyan
                        Text { anchors.centerIn:parent; text:modelData.interval_hours>0?(modelData.interval_hours+" H  ›"):"SET  ›"; color:setup.cyan; font.pixelSize:8*setup.uiScale; font.bold:true }
                        TapHandler { onTapped:controller.cycleMaintenanceInterval(index) }
                    }
                    Rectangle { x:312*setup.uiScale; y:8*setup.uiScale; width:68*setup.uiScale; height:30*setup.uiScale; radius:3*setup.uiScale; color:modelData.interval_hours>0?"#123A2A":"#28343A"; border.color:modelData.interval_hours>0?setup.green:setup.line
                        Text { anchors.centerIn:parent; text:"RECORD"; color:modelData.interval_hours>0?setup.green:"#74838A"; font.pixelSize:7*setup.uiScale; font.bold:true }
                        TapHandler { enabled:modelData.interval_hours>0; onTapped:controller.completeMaintenanceTask(index) }
                    }
                } }
            }
            Text { anchors { left:parent.left; right:parent.right; bottom:parent.bottom; bottomMargin:5*setup.uiScale } text:"Intervals must be confirmed against the applicable Rotax maintenance manual, engine serial number, fuel and operating conditions. BLACKBOX  "+String(controller.blackBoxState||"OFFLINE")+"  •  "+String(controller.blackBoxRecords||0)+" RECORDS"; color:setup.amber; horizontalAlignment:Text.AlignHCenter; font.pixelSize:6*setup.uiScale }
        }
        Rectangle { visible:setup.showFuelHistory; anchors.centerIn:parent; width:700*setup.uiScale; height:350*setup.uiScale; z:30; radius:6*setup.uiScale; color:"#F006151E"; border.width:2; border.color:setup.cyan
            Text { x:16*setup.uiScale; y:14*setup.uiScale; text:"BLACKBOX • FUEL EVENT HISTORY • "+String(controller.centralBlackBoxState||"OFFLINE"); color:String(controller.centralBlackBoxState)==="ONLINE"?"white":setup.amber; font.pixelSize:14*setup.uiScale; font.bold:true }
            Text { anchors { right:parent.right; top:parent.top; margins:16*setup.uiScale } text:"CLOSE"; color:setup.cyan; font.pixelSize:9*setup.uiScale; font.bold:true; TapHandler { onTapped:setup.showFuelHistory=false } }
            Rectangle { x:14*setup.uiScale; y:48*setup.uiScale; width:parent.width-28*setup.uiScale; height:24*setup.uiScale; color:"#10232D"; border.color:setup.line
                Text { x:8*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"SOURCE              UTC                    STATE                 L / R       FLOW    Δ GPH"; color:setup.cyan; font.pixelSize:7*setup.uiScale; font.bold:true }
            }
            ListView { x:14*setup.uiScale; y:74*setup.uiScale; width:parent.width-28*setup.uiScale; height:parent.height-145*setup.uiScale; clip:true; spacing:2*setup.uiScale; model:setup.listOr("centralFuelEvents").length?setup.listOr("centralFuelEvents"):setup.listOr("fuelEventHistory")
                delegate:Rectangle { required property var modelData; required property int index; width:ListView.view.width; height:27*setup.uiScale; color:index%2?"#0B1922":"#10222C"; border.color:String(modelData.state).indexOf("LEAK")>=0?setup.amber:"transparent"
                    Text { x:8*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:105*setup.uiScale; elide:Text.ElideRight; text:String(modelData.source||"LOCAL"); color:setup.cyan; font.pixelSize:7*setup.uiScale; font.bold:true }
                    Text { x:118*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:145*setup.uiScale; text:String(modelData.utc||"").replace("T"," ").replace("Z",""); color:"#B8C5CA"; font.pixelSize:7*setup.uiScale }
                    Text { x:270*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:145*setup.uiScale; text:String(modelData.state||""); color:String(modelData.state).indexOf("LEAK")>=0?setup.amber:setup.green; font.pixelSize:7*setup.uiScale; font.bold:true }
                    Text { x:420*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:85*setup.uiScale; text:Number(modelData.left_gal||0).toFixed(1)+" / "+Number(modelData.right_gal||0).toFixed(1); color:"white"; font.pixelSize:7*setup.uiScale }
                    Text { x:515*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; width:55*setup.uiScale; text:Number(modelData.flow_gph||0).toFixed(1); color:setup.cyan; font.pixelSize:7*setup.uiScale }
                    Text { x:585*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:Number(modelData.excess_gph||0).toFixed(2); color:"white"; font.pixelSize:7*setup.uiScale; font.bold:true }
                }
                Text { visible:parent.count===0; anchors.centerIn:parent; text:"NO FUEL EVENTS RECORDED"; color:setup.green; font.pixelSize:9*setup.uiScale; font.bold:true }
            }
            Row { anchors { left:parent.left; bottom:parent.bottom; margins:14*setup.uiScale } spacing:8*setup.uiScale
                Repeater { model:[{label:"CREATE INCIDENT",action:"CREATE_INCIDENT"},{label:"VERIFY SIGNATURES",action:"VERIFY_SIGNATURES"},{label:"EXPORT USB",action:"EXPORT_USB"}]
                    delegate:Rectangle { required property var modelData; width:145*setup.uiScale; height:34*setup.uiScale; radius:3*setup.uiScale; color:"#15303C"; border.color:modelData.action==="EXPORT_USB"?setup.amber:setup.cyan
                        Text { anchors.centerIn:parent; text:modelData.label; color:modelData.action==="EXPORT_USB"?setup.amber:setup.cyan; font.pixelSize:7*setup.uiScale; font.bold:true }
                        TapHandler { enabled:String(controller.centralBlackBoxState)==="ONLINE"; onTapped:controller.requestCentralBlackBoxAction(modelData.action) }
                    }
                }
            }
            Text { anchors { left:parent.left; bottom:parent.bottom; leftMargin:19*setup.uiScale; bottomMargin:57*setup.uiScale } width:310*setup.uiScale; elide:Text.ElideRight; text:"SIGNED FILES  "+setup.listOr("centralBlackBoxArchives").length+(setup.listOr("centralBlackBoxArchives").length?" • LAST  "+String(setup.listOr("centralBlackBoxArchives")[0].kind)+" • "+String(setup.listOr("centralBlackBoxArchives")[0].signature):""); color:setup.listOr("centralBlackBoxArchives").some(function(item){return String(item.signature)!=="VALID"})?setup.amber:setup.green; font.pixelSize:7*setup.uiScale; font.bold:true }
            Text { anchors { right:parent.right; bottom:parent.bottom; rightMargin:19*setup.uiScale; bottomMargin:57*setup.uiScale } width:330*setup.uiScale; horizontalAlignment:Text.AlignRight; elide:Text.ElideRight; text:String((controller.centralBlackBoxMaintenance||{}).action||"READY")+" • "+String((controller.centralBlackBoxMaintenance||{}).state||"READY")+(String((controller.centralBlackBoxMaintenance||{}).result||"").length?" • "+String(controller.centralBlackBoxMaintenance.result):""); color:String((controller.centralBlackBoxMaintenance||{}).state)==="COMPLETE"?setup.green:setup.amber; font.pixelSize:7*setup.uiScale; font.bold:true }
        }
    }
    Rectangle { id:footer; anchors { left:parent.left; right:parent.right; bottom:parent.bottom } height:42*setup.uiScale; color:"#07131C"; border.color:setup.line
        Text { x:14*setup.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"SETUP MODE • aircraft on ground • changes require verification"; color:setup.amber; font.pixelSize:8*setup.uiScale }
        Text { anchors { right:parent.right; rightMargin:14*setup.uiScale; verticalCenter:parent.verticalCenter } text:"PROFILE: ASSO-V-JOLLY-912-UL"; color:setup.cyan; font.pixelSize:8*setup.uiScale; font.bold:true }
    }
}
