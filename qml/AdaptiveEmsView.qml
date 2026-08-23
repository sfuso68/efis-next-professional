import QtQuick

Item {
    id: root
    required property var controller
    property real uiScale: Math.min(width/1024, height/600)
    readonly property color cyan: "#28D9F4"
    readonly property color green: "#52DE36"
    readonly property color amber: "#FFC72B"
    readonly property color panel: "#B006121B"
    readonly property color line: "#55646C"
    readonly property color health: controller.engineHealth >= 2 ? "#FF3933" : controller.engineHealth === 1 ? amber : green
    property string selectedPanel: ""
    property bool setupVisible: false
    property var rpmTrend: []
    property var oilTrend: []
    property var chtTrend: []
    property var busTrend: []
    function appendTrend(series,value) { var copy=series.slice(0); copy.push(value); if(copy.length>120)copy.shift(); return copy }
    readonly property string units: controller.unitSystem || "AVIATION"
    readonly property bool metricUnits: units === "METRIC"
    readonly property bool imperialUnits: units === "IMPERIAL"
    function temp(v) { return imperialUnits ? v*9/5+32 : v }
    function pressure(v) { return metricUnits ? v*0.0689476 : v }
    function fuel(v) { return metricUnits ? v*3.78541 : v }
    function flow(v) { return metricUnits ? v*3.78541 : v }
    function mapValue(v) { return metricUnits ? v*33.8639 : v }
    readonly property string tempUnit: imperialUnits ? "°F" : "°C"
    readonly property string pressureUnit: metricUnits ? "BAR" : "PSI"
    readonly property string fuelUnit: metricUnits ? "L" : "GAL"
    readonly property string flowUnit: metricUnits ? "L/H" : "GPH"
    readonly property string mapUnit: metricUnits ? "HPA" : "IN HG"
    Timer { interval:1000; running:true; repeat:true; onTriggered:{
            root.rpmTrend=root.appendTrend(root.rpmTrend,Number(controller.engineRpm||0)/3000)
            root.oilTrend=root.appendTrend(root.oilTrend,Number(controller.oilPressure||0)/100)
            var maximum=0; var values=controller.chtValues||[]; for(var i=0;i<values.length;i++)maximum=Math.max(maximum,Number(values[i])); root.chtTrend=root.appendTrend(root.chtTrend,maximum/250)
            root.busTrend=root.appendTrend(root.busTrend,Number(controller.busVoltage||0)/16)
            trendCanvas.requestPaint()
        } }

    Rectangle { anchors.fill: parent
        gradient: Gradient { orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "#06121B" }
            GradientStop { position: .55; color: "#020B11" }
            GradientStop { position: 1; color: "#071723" } } }

    Rectangle { id: header; anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 44*root.uiScale; color: "#D0061119"; border.color: root.line
        Text { x: 13*root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "EFIS"; color: "white"; font.pixelSize: 17*root.uiScale; font.bold: true }
        Text { x: 54*root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "NEXT"; color: root.cyan; font.pixelSize: 17*root.uiScale; font.bold: true }
        Rectangle { x: 112*root.uiScale; y: 8*root.uiScale; width: 1; height: 28*root.uiScale; color: root.line }
        Text { x: 129*root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "EMS"; color: root.cyan; font.pixelSize: 15*root.uiScale }
        Text { anchors.centerIn: parent; text: "ENGINE"; color: "white"; font.pixelSize: 20*root.uiScale; font.bold: true; font.letterSpacing: 1.2 }
        Rectangle { anchors { verticalCenter: parent.verticalCenter; right: clock.left; rightMargin: 32*root.uiScale }
        width: 54*root.uiScale; height: 25*root.uiScale; radius: 4; color: "#08751B"
            Text { anchors.centerIn: parent; text: controller.dataSourceState === "SIM" ? "SIM" : "LIVE"; color: "white"; font.pixelSize: 12*root.uiScale } }
        Text { id: clock; anchors { verticalCenter: parent.verticalCenter; right: parent.right; rightMargin: 18*root.uiScale }
        text: Qt.formatTime(new Date(),"hh:mm:ss")+"   LOCAL     24 °C  OAT     1013 HPA  QNH"; color: "#E8ECEE"; font.pixelSize: 11*root.uiScale; font.bold: true }
        Timer { interval: 1000; running: true; repeat: true; onTriggered: clock.text=Qt.formatTime(new Date(),"hh:mm:ss")+"   LOCAL     24 °C  OAT     1013 HPA  QNH" }
        Item { x:0; y:0; width:105*root.uiScale; height:parent.height
            TapHandler { longPressThreshold:1.2; onLongPressed:root.setupVisible=true }
        }
    }

    Rectangle { id: nav; anchors { left: parent.left; top: header.bottom; bottom: actions.top }
        width: 96*root.uiScale; color: "#B00A151D"; border.color: root.line; radius: 5*root.uiScale
        Column { anchors.fill: parent
            Repeater { model: [{k:"engine",n:"ENGINE"},{k:"indications",n:"INDICATIONS"},{k:"electrical",n:"ELECTRICAL"},{k:"fuel",n:"FUEL"},{k:"system",n:"SYSTEM"},{k:"checklist",n:"MAINT"}]
                delegate: Rectangle { required property var modelData; width: nav.width; height: nav.height/6; color: modelData.n === "ENGINE" ? "#172832" : "transparent"; border.color: "#35454D"
                    Rectangle { visible: modelData.n === "ENGINE"; width: 4*root.uiScale; height: parent.height; color: root.cyan }
                    EmsVectorIcon { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 9*root.uiScale }
         width: 27*root.uiScale; height: 27*root.uiScale; kind: modelData.k; strokeColor: modelData.n === "ENGINE" ? root.cyan : "#89969C" }
                    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 8*root.uiScale }
        text: modelData.n; color: modelData.n === "ENGINE" ? root.cyan : "#A7B0B5"; font.pixelSize: 8*root.uiScale }
                    TapHandler { onTapped: { if (modelData.n === "SYSTEM") root.selectedPanel = "SYSTEM" } }
                } }
        } }

    Item { id: body; anchors { left: nav.right; right: parent.right; top: header.bottom; bottom: summary.top; leftMargin: 8*root.uiScale; rightMargin: 8*root.uiScale; topMargin: 5*root.uiScale } }
    Item { id: gauges; anchors { left: body.left; top: body.top; bottom: body.bottom }
        width: body.width*.63 }
    Row { id: primary; anchors { left: gauges.left; right: gauges.right; top: gauges.top }
        height: gauges.height*.59; spacing: 7*root.uiScale
        EmsGaugeV3 { width: (parent.width-parent.spacing)/2; height: parent.height; uiScale: root.uiScale; dominant: true; label: "RPM"; value: controller.engineRpm; unit: "RPM"; minimum: 0; maximum: 3000; greenStart: 800; greenEnd: 2600; majorStep: 500; minorStep: 100 }
        EmsGaugeV3 { width: (parent.width-parent.spacing)/2; height: parent.height; uiScale: root.uiScale; dominant: true; label: "MAP"; value: root.mapValue(controller.manifoldPressure); unit: root.mapUnit; minimum: 0; maximum: root.metricUnits?1350:40; greenStart: root.metricUnits?170:5; greenEnd: root.metricUnits?1150:34; majorStep: root.metricUnits?200:5; minorStep:root.metricUnits?40:1; decimals: root.metricUnits?0:1 }
    }
    Row { anchors { left: gauges.left; right: gauges.right; top: primary.bottom; bottom: gauges.bottom; topMargin: -3*root.uiScale }
        spacing: 5*root.uiScale
        EmsGaugeV3 { width: (parent.width-2*parent.spacing)/3; height: parent.height; uiScale: root.uiScale; label: "OIL PRESS"; value:root.pressure(controller.oilPressure); unit:root.pressureUnit; minimum:0; maximum:root.metricUnits?7:100; greenStart:root.metricUnits?1.7:25; greenEnd:root.metricUnits?5.9:85; majorStep:root.metricUnits?1:20; minorStep:root.metricUnits?.2:4; decimals:root.metricUnits?1:0 }
        EmsGaugeV3 { width: (parent.width-2*parent.spacing)/3; height: parent.height; uiScale: root.uiScale; label: "OIL TEMP"; value:root.temp(controller.oilTemperature); unit:root.tempUnit; minimum:root.imperialUnits?70:20; maximum:root.imperialUnits?285:140; greenStart:root.imperialUnits?120:50; greenEnd:root.imperialUnits?230:110; majorStep:root.imperialUnits?40:20; minorStep:root.imperialUnits?8:4 }
        EmsGaugeV3 { width: (parent.width-2*parent.spacing)/3; height: parent.height; uiScale: root.uiScale; label: "FUEL FLOW"; value:root.flow(controller.fuelFlow); unit:root.flowUnit; minimum:0; maximum:root.metricUnits?115:30; greenStart:root.metricUnits?11:3; greenEnd:root.metricUnits?87:23; majorStep:root.metricUnits?20:5; minorStep:root.metricUnits?4:1; decimals:1 }
    }

    Item { id: rightColumn; anchors { left: gauges.right; right: body.right; top: body.top; bottom: body.bottom; leftMargin: 8*root.uiScale } }
    Rectangle { id: cas; anchors { left: rightColumn.left; right: rightColumn.right; top: rightColumn.top }
        height: rightColumn.height*.42; color: root.panel; border.color: root.line; radius: 5*root.uiScale
        Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 5*root.uiScale }
        text: "CAS MESSAGES"; color: "#E7ECEE"; font.pixelSize: 10*root.uiScale }
        Rectangle { x: 1; y: 25*root.uiScale; width: parent.width-2; height: 34*root.uiScale; color: "#07151D"; border.color: "#374851"
            Text { anchors.centerIn: parent; text: "✓   SYSTEM NORMAL"; color: root.health; font.pixelSize: 14*root.uiScale; font.bold: true } }
        Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 64*root.uiScale }
        text: "AIRFRAME  ·  CONFIGURED"; color: root.cyan; font.pixelSize: 9*root.uiScale; font.letterSpacing: 1.2 }
        Rectangle { x: parent.width/2; y: 82*root.uiScale; width: 1; height: parent.height-83*root.uiScale; color: root.line }
        Item { visible: controller.landingGearInstalled; x: 5*root.uiScale; y: 83*root.uiScale; width: parent.width/2-10*root.uiScale; height: parent.height-85*root.uiScale
            Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top }
        text: "GEAR"; color: root.cyan; font.pixelSize: 10*root.uiScale }
            EmsVectorIcon { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 14*root.uiScale }
         width: 58*root.uiScale; height: Math.min(48*root.uiScale,parent.height*.54); kind: "airplane"; strokeColor: "#E5EAEC" }
            Repeater { model: [
                {n:"N",ok:controller.gearNoseLocked,x:.50,y:.37},
                {n:"L",ok:controller.gearLeftLocked,x:.27,y:.66},
                {n:"R",ok:controller.gearRightLocked,x:.73,y:.66}]
                delegate: Item { required property var modelData; x: parent.width*modelData.x-8*root.uiScale; y: parent.height*modelData.y; width: 16*root.uiScale; height: 24*root.uiScale
                    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 14*root.uiScale; height: width; radius: width/2; color: modelData.ok ? "#0B2C16" : "#421316"; border.width: 2; border.color: modelData.ok ? root.green : "#FF3830"
                        Rectangle { anchors.centerIn: parent; width: 5*root.uiScale; height: width; radius: width/2; color: modelData.ok ? root.green : "#FF3830" } }
                    Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 15*root.uiScale }
         text: modelData.n; color: "#DCE5E8"; font.pixelSize: 6*root.uiScale; font.bold: true }
                } }
            Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 3*root.uiScale }
        text: controller.gearState; color: controller.gearState === "DOWN & LOCKED" ? root.green : "#FF3830"; font.pixelSize: 8*root.uiScale; font.bold: true }
        }
        Item { visible: controller.flapsInstalled; x: parent.width/2+6*root.uiScale; y: 83*root.uiScale; width: parent.width/2-12*root.uiScale; height: parent.height-85*root.uiScale
            Text { anchors { left: parent.left; top: parent.top }
        text: "FLAPS"; color: root.cyan; font.pixelSize: 10*root.uiScale }
            Text { anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 3*root.uiScale }
         text: Math.round(controller.flapAngle)+"°"; color: "white"; font.pixelSize: 27*root.uiScale; font.bold: true }
            Text { anchors { left: parent.left; bottom: parent.bottom; leftMargin: 3*root.uiScale }
         text: "ACTUAL"; color: root.green; font.pixelSize: 6*root.uiScale; font.bold: true }
            Rectangle { id: flapRail; x: parent.width*.59; y: 15*root.uiScale; width: 5*root.uiScale; height: parent.height-22*root.uiScale; radius: 2; color: "#344A54"
                Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top }
         height: parent.height*Math.min(1,controller.flapAngle/30); radius: 2; color: root.cyan }
                Rectangle { x: -5*root.uiScale; y: parent.height*Math.min(1,controller.flapAngle/30)-5*root.uiScale; width: 15*root.uiScale; height: 10*root.uiScale; rotation: 45; color: root.cyan; border.color: "white" }
                Rectangle { x: -8*root.uiScale; y: parent.height*Math.min(1,controller.flapCommandedAngle/30)-1*root.uiScale; width: 21*root.uiScale; height: 2*root.uiScale; color: "white" }
            }
            Repeater { model: controller.flapDetents
                delegate: Item { required property real modelData; x: flapRail.x+11*root.uiScale; y: flapRail.y+flapRail.height*modelData/30-5*root.uiScale; width: parent.width-flapRail.x; height: 11*root.uiScale
                    Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 6*root.uiScale; height: 1; color: "#D7E0E3" }
                    Text { x: 9*root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: modelData+"°"; color: modelData === Math.round(controller.flapAngle) ? root.cyan : "#D7E0E3"; font.pixelSize: 6*root.uiScale; font.bold: modelData === Math.round(controller.flapAngle) }
                } }
        }
    }

    EngineTemperatureBars { anchors { left:rightColumn.left; right:rightColumn.right; top:cas.bottom; bottom:rightColumn.bottom; topMargin:7*root.uiScale } uiScale:root.uiScale; compact:false; chtValues:controller.chtValues; egtValues:controller.egtValues; temperatureUnit:root.tempUnit }

    Rectangle { id: summary; anchors { left: nav.right; right: parent.right; bottom: actions.top; leftMargin: 8*root.uiScale; rightMargin: 8*root.uiScale; bottomMargin: 7*root.uiScale }
        height: 72*root.uiScale; color: root.panel; border.color: root.line; radius: 5*root.uiScale
        Row { anchors.fill: parent
            Item { width: parent.width*.36; height: parent.height
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height*.72; anchors.verticalCenter: parent.verticalCenter; color: root.line }
                Text { x:12*root.uiScale; y:7*root.uiScale; text:"FUEL"; color:root.cyan; font.pixelSize:9*root.uiScale; font.bold:true }
                Text { anchors { right:parent.right; rightMargin:14*root.uiScale; top:parent.top; topMargin:6*root.uiScale }
                    text:root.fuel(controller.fuelLeft+controller.fuelRight).toFixed(1)+" "+root.fuelUnit; color:"white"; font.pixelSize:13*root.uiScale; font.bold:true }
                Row { x:12*root.uiScale; y:31*root.uiScale; width:parent.width-26*root.uiScale; spacing:12*root.uiScale
                    Repeater { model:[{n:"L",v:controller.fuelLeft},{n:"R",v:controller.fuelRight}]
                        delegate:Item { required property var modelData; width:(parent.width-parent.spacing)/2; height:29*root.uiScale
                            Text { text:modelData.n+"  "+root.fuel(modelData.v).toFixed(1); color:"#E8EEF0"; font.pixelSize:8*root.uiScale; font.bold:true }
                            Rectangle { anchors { left:parent.left; right:parent.right; bottom:parent.bottom } height:8*root.uiScale; color:"#142630"; border.color:"#60727B"
                                Rectangle { anchors { left:parent.left; top:parent.top; bottom:parent.bottom; margins:1 }
                                    width:(parent.width-2)*Math.max(0,Math.min(1,modelData.v/20)); color:modelData.v<4?"#FF3933":modelData.v<7?root.amber:root.green }
                            }
                        }
                    }
                }
            }
            Repeater { model: [
                {h:"ENDURANCE",v:((controller.fuelLeft+controller.fuelRight)/Math.max(.1,controller.fuelFlow)).toFixed(1)+" HR",c:root.cyan},
                {h:"BUS",v:controller.busVoltage.toFixed(1)+" V",c:"white"},
                {h:"ALT",v:"+"+controller.alternatorCurrent.toFixed(0)+" A",c:"white"}]
                delegate: Item { required property var modelData; width: parent.width*.64/3; height: parent.height
                    Rectangle { anchors.right: parent.right; width: 1; height: parent.height*.72; anchors.verticalCenter: parent.verticalCenter; color: root.line }
                    Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 9*root.uiScale }
        text: modelData.h; color: root.cyan; font.pixelSize: 9*root.uiScale }
                    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 10*root.uiScale }
        text: modelData.v; color: modelData.c; font.pixelSize: 20*root.uiScale; font.bold: true } } }
        } }
    Row { id: actions; anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 10*root.uiScale }
        height: 48*root.uiScale; spacing: 12*root.uiScale
        Repeater { model: [{k:"lean",n:"LEAN"},{k:"fuel",n:"FUEL"},{k:"trend",n:"TREND"},{k:"checklist",n:"CHECKLIST"}]
            delegate: Rectangle { required property var modelData; width: (parent.width-3*parent.spacing)/4; height: parent.height; radius: 5*root.uiScale; color: "#101D25"; border.color: root.line
                EmsVectorIcon { x: 50*root.uiScale; anchors.verticalCenter: parent.verticalCenter; width: 27*root.uiScale; height: 27*root.uiScale; kind: modelData.k; strokeColor: modelData.n === "LEAN" ? root.green : root.cyan }
                Text { anchors.centerIn: parent; text: modelData.n; color: "#F0F3F4"; font.pixelSize: 14*root.uiScale; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: root.selectedPanel=modelData.n }
            } }
    }

    Rectangle { anchors.fill: parent; visible: root.selectedPanel !== ""; z: 50; color: "#A0000508"
        MouseArea { anchors.fill: parent; onClicked: root.selectedPanel="" }
        Rectangle { anchors.centerIn: parent; width: 610*root.uiScale; height: 330*root.uiScale; radius: 8*root.uiScale; color: "#F0081720"; border.width: 2; border.color: root.cyan
            MouseArea { anchors.fill: parent }
            Text { x: 22*root.uiScale; y: 16*root.uiScale; text: root.selectedPanel; color: root.cyan; font.pixelSize: 21*root.uiScale; font.bold: true }
            Text { anchors { right: parent.right; top: parent.top; margins: 17*root.uiScale }
         text: "×"; color: "white"; font.pixelSize: 25*root.uiScale
                MouseArea { anchors.fill: parent; anchors.margins: -10*root.uiScale; onClicked: root.selectedPanel="" } }
            Rectangle { x: 20*root.uiScale; y: 55*root.uiScale; width: parent.width-40*root.uiScale; height: 1; color: root.line }
            Text { visible:root.selectedPanel !== "SYSTEM" && root.selectedPanel !== "TREND"; x: 25*root.uiScale; y: 76*root.uiScale; width: parent.width-50*root.uiScale; wrapMode: Text.WordWrap
                text: root.selectedPanel === "LEAN" ? "LEAN ASSIST\nMonitoraggio comparato EGT per cilindro. Identificazione automatica del primo cilindro al picco e memorizzazione della variazione dal picco." :
                      root.selectedPanel === "FUEL" ? "FUEL COMPUTER\nCarburante misurato: "+root.fuel(controller.fuelLeft+controller.fuelRight).toFixed(1)+" "+root.fuelUnit+"\nFlusso: "+root.flow(controller.fuelFlow).toFixed(1)+" "+root.flowUnit+"\nAutonomia stimata: "+((controller.fuelLeft+controller.fuelRight)/Math.max(.1,controller.fuelFlow)).toFixed(1)+" HR" :
                      root.selectedPanel === "TREND" ? "ENGINE TREND\nRPM, pressioni, temperature e parametri elettrici saranno rappresentati su finestre selezionabili da 1, 5, 15 e 60 minuti." :
                      "ENGINE CHECKLIST\n□ Parametri motore nel campo verde\n□ Carburante e autonomia verificati\n□ Alternatore e tensione verificati\n□ Carrello e flap configurati"
                color: "#E8EEF0"; font.pixelSize: 15*root.uiScale; lineHeight: 1.5 }
            Item { visible:root.selectedPanel === "SYSTEM"; x:22*root.uiScale; y:68*root.uiScale; width:parent.width-44*root.uiScale; height:225*root.uiScale
                Text { text:"UNITS OF MEASURE"; color:"white"; font.pixelSize:15*root.uiScale; font.bold:true }
                Text { y:26*root.uiScale; text:"Select the presentation standard used by EMS, PFD and Canvas"; color:"#AFC0C7"; font.pixelSize:9*root.uiScale }
                Row { y:55*root.uiScale; width:parent.width; height:112*root.uiScale; spacing:10*root.uiScale
                    Repeater { model:[
                        {id:"AVIATION",title:"AVIATION",detail:"°C  •  PSI  •  IN HG\nGAL  •  GPH"},
                        {id:"METRIC",title:"METRIC",detail:"°C  •  BAR  •  HPA\nL  •  L/H"},
                        {id:"IMPERIAL",title:"IMPERIAL",detail:"°F  •  PSI  •  IN HG\nGAL  •  GPH"}]
                        delegate:Rectangle { required property var modelData; width:(parent.width-2*parent.spacing)/3; height:parent.height; radius:5*root.uiScale
                            color:root.units===modelData.id?"#143542":"#0B1A22"; border.width:root.units===modelData.id?2:1; border.color:root.units===modelData.id?root.cyan:root.line
                            Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:17*root.uiScale } text:modelData.title; color:root.units===modelData.id?root.cyan:"white"; font.pixelSize:13*root.uiScale; font.bold:true }
                            Text { anchors.centerIn:parent; anchors.verticalCenterOffset:12*root.uiScale; text:modelData.detail; horizontalAlignment:Text.AlignHCenter; color:"#D7E2E6"; font.pixelSize:9*root.uiScale; lineHeight:1.5 }
                            Text { anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:8*root.uiScale } text:root.units===modelData.id?"● ACTIVE":"SELECT"; color:root.units===modelData.id?root.green:"#82939A"; font.pixelSize:7*root.uiScale; font.bold:true }
                            TapHandler { onTapped:controller.setUnitSystem(modelData.id) }
                        }
                    }
                }
                Text { y:182*root.uiScale; width:parent.width; wrapMode:Text.WordWrap; text:"Display conversion only: recorded sensor data and certified limits remain stored in canonical units."; color:"#83959D"; font.pixelSize:8*root.uiScale }
            }
            Item { visible:root.selectedPanel === "TREND"; x:22*root.uiScale; y:67*root.uiScale; width:parent.width-44*root.uiScale; height:225*root.uiScale
                Text { text:"ENGINE TREND • 120 SECOND MOVING WINDOW"; color:"white"; font.pixelSize:12*root.uiScale; font.bold:true }
                Row { x:310*root.uiScale; spacing:14*root.uiScale
                    Repeater { model:[{n:"RPM",c:"#28D9F4"},{n:"OIL P",c:"#52DE36"},{n:"CHT MAX",c:"#FFC72B"},{n:"BUS",c:"#D36BFF"}]
                        delegate:Row { required property var modelData; spacing:4*root.uiScale; Rectangle { width:12*root.uiScale; height:3*root.uiScale; anchors.verticalCenter:parent.verticalCenter; color:modelData.c } Text { text:modelData.n; color:"#DCE5E8"; font.pixelSize:7*root.uiScale } }
                    }
                }
                Rectangle { x:0; y:28*root.uiScale; width:parent.width; height:166*root.uiScale; color:"#07141C"; border.color:root.line
                    Repeater { model:5; delegate:Rectangle { required property int index; x:0; y:index*parent.height/4; width:parent.width; height:1; color:"#263944" } }
                    Canvas { id:trendCanvas; anchors.fill:parent
                        function drawSeries(ctx,series,color){if(series.length<2)return;ctx.beginPath();ctx.strokeStyle=color;ctx.lineWidth=2*root.uiScale;for(var i=0;i<series.length;i++){var x=i*(width/(119));var y=height-Math.max(0,Math.min(1,series[i]))*height;if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y)}ctx.stroke()}
                        onPaint:{var ctx=getContext("2d");ctx.clearRect(0,0,width,height);drawSeries(ctx,root.rpmTrend,"#28D9F4");drawSeries(ctx,root.oilTrend,"#52DE36");drawSeries(ctx,root.chtTrend,"#FFC72B");drawSeries(ctx,root.busTrend,"#D36BFF")}
                    }
                }
                Text { y:201*root.uiScale; text:"NOW  RPM "+Math.round(controller.engineRpm)+"  •  OIL "+Number(controller.oilPressure).toFixed(1)+" PSI  •  BUS "+Number(controller.busVoltage).toFixed(1)+" V"; color:root.cyan; font.pixelSize:8*root.uiScale; font.bold:true }
            }
            Text { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 22*root.uiScale }
         text: "TOUCH OUTSIDE TO CLOSE"; horizontalAlignment: Text.AlignHCenter; color: "#899BA3"; font.pixelSize: 8*root.uiScale; font.letterSpacing: 1.4 }
        }
    }

    EmsSetupOverlay { anchors.fill:parent; controller:root.controller; visible:root.setupVisible; onClosed:root.setupVisible=false }
}
