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
    readonly property var engineLimits: controller.engineLimits||({})
    readonly property var engineCapabilities: controller.engineCapabilities||({})
    readonly property bool mixtureControlAvailable: engineCapabilities.mixture_control===true
    function engineLimit(name,fallback) { var value=Number(engineLimits[name]); return isFinite(value)&&value>0?value:fallback }
    readonly property real oilPressureMinimum: engineLimit("oil_pressure_min_psi",25)
    readonly property real oilPressureMaximum: engineLimit("oil_pressure_max_psi",100)
    readonly property real oilTemperatureMaximum: engineLimit("oil_temperature_max_c",120)
    readonly property real chtMaximum: engineLimit("cht_max_c",230)
    readonly property real egtMaximum: engineLimit("egt_max_c",920)
    readonly property real rpmMaximum: engineLimit("rpm_max",3000)
    property string selectedPanel: ""
    property bool setupVisible: false
    property int setupSection: 0
    property string activeNavigation: "ENGINE"
    function closePanel() { selectedPanel=""; activeNavigation="ENGINE" }
    function openSetup(section) { setupSection=section; activeNavigation=section===3?"ELECTRICAL":section===4?"MAINT":"SYSTEM"; setupVisible=true }
    function selectNavigation(name) {
        activeNavigation=name
        if(name==="ENGINE")selectedPanel=""
        else if(name==="INDICATIONS")selectedPanel="TREND"
        else if(name==="ELECTRICAL"){selectedPanel="";openSetup(3)}
        else if(name==="FUEL")selectedPanel="FUEL"
        else if(name==="SYSTEM")selectedPanel="SYSTEM"
        else if(name==="MAINT"){selectedPanel="";openSetup(4)}
    }
    property var rpmTrend: []
    property var oilTrend: []
    property var chtTrend: []
    property var busTrend: []
    property bool checklistFuelConfirmed: false
    property bool checklistAirframeConfirmed: false
    property bool leanActive: false
    property var leanPeakValues: []
    property var leanPeakDetected: []
    property int leanFirstCylinder: -1
    readonly property real fuelUsedGallons: Number(controller.fuelUsedGallons)
    readonly property real fuelReserveGallons: Number(controller.fuelReserveGallons)
    readonly property real fuelMeasuredGallons: Math.max(0,controller.fuelLeft+controller.fuelRight)
    readonly property real fuelTotalGallons: Math.max(0,Number(controller.fuelRemainingGallons))
    readonly property real fuelUsableGallons: Math.max(0,fuelTotalGallons-fuelReserveGallons)
    readonly property real fuelEnduranceHours: fuelUsableGallons/Math.max(.1,controller.fuelFlow)
    readonly property real fuelReserveMinutes: fuelReserveGallons/Math.max(.1,controller.fuelFlow)*60
    readonly property real fuelImbalanceGallons: Math.abs(controller.fuelLeft-controller.fuelRight)
    readonly property int fuelCalibrationLeftCount: (controller.fuelCalibrationLeft||[]).length
    readonly property int fuelCalibrationRightCount: (controller.fuelCalibrationRight||[]).length
    readonly property bool fuelCalibrationReady: fuelCalibrationLeftCount===5&&fuelCalibrationRightCount===5
    readonly property real fuelDiscrepancyGallons: Math.abs(fuelMeasuredGallons-fuelTotalGallons)
    readonly property string fuelLeakState: String(controller.fuelLeakState||"INITIALIZING")
    readonly property bool fuelLeakAlarm: fuelLeakState==="POSSIBLE LEAK"||fuelLeakState==="FUEL LEAK WARNING"
    function maximumValue(values) { var result=0; for(var i=0;i<values.length;i++)result=Math.max(result,Number(values[i])); return result }
    readonly property real maximumCht: maximumValue(controller.chtValues||[])
    readonly property string casLevel: controller.engineRpm>800 && controller.oilPressure<oilPressureMinimum ? "WARNING" :
                                       maximumCht>chtMaximum || controller.oilTemperature>oilTemperatureMaximum ? "WARNING" :
                                       fuelTotalGallons<=fuelReserveGallons*.5 || fuelLeakAlarm || (fuelCalibrationReady&&fuelDiscrepancyGallons>3) ? "WARNING" :
                                       controller.busVoltage<12.5 || fuelTotalGallons<=fuelReserveGallons ? "CAUTION" :
                                       fuelImbalanceGallons>3 ? "ADVISORY" : "NORMAL"
    readonly property string casHeadline: controller.engineRpm>800 && controller.oilPressure<oilPressureMinimum ? "OIL PRESSURE LOW" :
                                          maximumCht>chtMaximum ? "CHT HIGH" :
                                          controller.oilTemperature>oilTemperatureMaximum ? "OIL TEMPERATURE HIGH" :
                                          controller.busVoltage<12.5 ? "BUS VOLTAGE LOW" :
                                          fuelTotalGallons<=fuelReserveGallons*.5 ? "FUEL QUANTITY CRITICAL" :
                                          fuelLeakAlarm ? fuelLeakState :
                                          fuelCalibrationReady&&fuelDiscrepancyGallons>3 ? "FUEL QUANTITY DISAGREE" :
                                          fuelTotalGallons<=fuelReserveGallons ? "FINAL RESERVE" :
                                          fuelImbalanceGallons>3 ? "FUEL IMBALANCE" : "SYSTEM NORMAL"
    readonly property string casAction: controller.engineRpm>800 && controller.oilPressure<oilPressureMinimum ? "REDUCE POWER • LAND AS SOON AS PRACTICABLE" :
                                        maximumCht>chtMaximum ? "INCREASE COOLING • REDUCE POWER" :
                                        controller.oilTemperature>oilTemperatureMaximum ? "REDUCE POWER • MONITOR PRESSURE" :
                                        controller.busVoltage<12.5 ? "CHECK ALTERNATOR AND ELECTRICAL LOAD" :
                                        fuelTotalGallons<=fuelReserveGallons*.5 ? "LAND AS SOON AS POSSIBLE" :
                                        fuelLeakAlarm ? "CHECK TANKS • SELECT FUEL • LAND SOON" :
                                        fuelCalibrationReady&&fuelDiscrepancyGallons>3 ? "CHECK FOR LEAK OR SENSOR FAULT" :
                                        fuelTotalGallons<=fuelReserveGallons ? "VERIFY ENDURANCE AND DIVERT" :
                                        fuelImbalanceGallons>3 ? "SELECT FULLER TANK" : "ENGINE AND AIRFRAME PARAMETERS IN RANGE"
    readonly property color casColor: casLevel==="WARNING" ? "#FF3933" : casLevel==="CAUTION" ? amber : casLevel==="ADVISORY" ? cyan : green
    property var casEvents: []
    property string lastCasHeadline: "SYSTEM NORMAL"
    property bool casAcknowledged: true
    function updateCasEvents() {
        if(casHeadline===lastCasHeadline)return
        var updated=[]
        for(var i=0;i<casEvents.length;i++){
            var event=casEvents[i]
            updated.push({time:event.time,level:event.level,message:event.message,action:event.action,status:event.status==="ACTIVE"?"CLEARED":event.status})
        }
        if(casLevel!=="NORMAL"){
            updated.unshift({time:Qt.formatTime(new Date(),"hh:mm:ss"),level:casLevel,message:casHeadline,action:casAction,status:"ACTIVE"})
            casAcknowledged=false
        } else casAcknowledged=true
        if(updated.length>12)updated=updated.slice(0,12)
        casEvents=updated; lastCasHeadline=casHeadline
    }
    function resetLean() {
        var values=controller.egtValues||[]; var peaks=[]; var detected=[]
        for(var i=0;i<values.length;i++){peaks.push(Number(values[i]));detected.push(false)}
        leanPeakValues=peaks; leanPeakDetected=detected; leanFirstCylinder=-1
    }
    function startLean() { resetLean(); leanActive=true }
    function updateLean() {
        if(!leanActive)return
        var values=controller.egtValues||[]; var peaks=leanPeakValues.slice(0); var detected=leanPeakDetected.slice(0)
        for(var i=0;i<values.length;i++){
            var value=Number(values[i]); if(i>=peaks.length){peaks.push(value);detected.push(false)}
            if(!detected[i] && value>peaks[i])peaks[i]=value
            if(!detected[i] && peaks[i]-value>=8){detected[i]=true;if(leanFirstCylinder<0)leanFirstCylinder=i}
        }
        leanPeakValues=peaks; leanPeakDetected=detected
    }
    readonly property bool checklistEngineOk: controller.engineRpm < 400 || (controller.oilPressure >= oilPressureMinimum && controller.oilPressure <= oilPressureMaximum && controller.oilTemperature >= 50 && controller.oilTemperature <= oilTemperatureMaximum)
    readonly property bool checklistFuelOk: checklistFuelConfirmed && (controller.fuelLeft + controller.fuelRight) >= 8
    readonly property bool checklistElectricalOk: controller.busVoltage >= 13.2 && controller.busVoltage <= 14.8 && controller.alternatorCurrent >= 0
    readonly property bool checklistAirframeOk: checklistAirframeConfirmed && (!controller.landingGearInstalled || controller.gearState === "DOWN & LOCKED")
    readonly property bool checklistComplete: checklistEngineOk && checklistFuelOk && checklistElectricalOk && checklistAirframeOk
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
            root.updateLean()
            root.updateCasEvents()
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
            TapHandler { longPressThreshold:1.2; onLongPressed:root.openSetup(0) }
        }
    }

    Rectangle { id: nav; anchors { left: parent.left; top: header.bottom; bottom: actions.top }
        width: 96*root.uiScale; color: "#B00A151D"; border.color: root.line; radius: 5*root.uiScale
        Column { anchors.fill: parent
            Repeater { model: [{k:"engine",n:"ENGINE"},{k:"indications",n:"INDICATIONS"},{k:"electrical",n:"ELECTRICAL"},{k:"fuel",n:"FUEL"},{k:"system",n:"SYSTEM"},{k:"checklist",n:"MAINT"}]
                delegate: Rectangle { required property var modelData; width: nav.width; height: nav.height/6; color: root.activeNavigation === modelData.n ? "#172832" : "transparent"; border.color: "#35454D"
                    Rectangle { visible: root.activeNavigation === modelData.n; width: 4*root.uiScale; height: parent.height; color: root.cyan }
                    EmsVectorIcon { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 9*root.uiScale }
         width: 27*root.uiScale; height: 27*root.uiScale; kind: modelData.k; strokeColor: root.activeNavigation === modelData.n ? root.cyan : "#89969C" }
                    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 8*root.uiScale }
        text: modelData.n; color: root.activeNavigation === modelData.n ? root.cyan : "#A7B0B5"; font.pixelSize: 8*root.uiScale }
                    TapHandler { onTapped:root.selectNavigation(modelData.n) }
                } }
        } }

    Item { id: body; anchors { left: nav.right; right: parent.right; top: header.bottom; bottom: summary.top; leftMargin: 8*root.uiScale; rightMargin: 8*root.uiScale; topMargin: 5*root.uiScale } }
    Item { id: gauges; anchors { left: body.left; top: body.top; bottom: body.bottom }
        width: body.width*.63 }
    Row { id: primary; anchors { left: gauges.left; right: gauges.right; top: gauges.top }
        height: gauges.height*.59; spacing: 7*root.uiScale
        EmsGaugeV3 { width: (parent.width-parent.spacing)/2; height: parent.height; uiScale: root.uiScale; dominant: true; label: "RPM"; value: controller.engineRpm; unit: "RPM"; minimum: 0; maximum: root.rpmMaximum; greenStart: root.rpmMaximum*.24; greenEnd: root.rpmMaximum*.95; majorStep: root.rpmMaximum>4000?1000:500; minorStep: root.rpmMaximum>4000?200:100 }
        EmsGaugeV3 { width: (parent.width-parent.spacing)/2; height: parent.height; uiScale: root.uiScale; dominant: true; label: "MAP"; value: root.mapValue(controller.manifoldPressure); unit: root.mapUnit; minimum: 0; maximum: root.metricUnits?1350:40; greenStart: root.metricUnits?170:5; greenEnd: root.metricUnits?1150:34; majorStep: root.metricUnits?200:5; minorStep:root.metricUnits?40:1; decimals: root.metricUnits?0:1 }
    }
    Row { anchors { left: gauges.left; right: gauges.right; top: primary.bottom; bottom: gauges.bottom; topMargin: -3*root.uiScale }
        spacing: 5*root.uiScale
        EmsGaugeV3 { width: (parent.width-2*parent.spacing)/3; height: parent.height; uiScale: root.uiScale; label: "OIL PRESS"; value:root.pressure(controller.oilPressure); unit:root.pressureUnit; minimum:0; maximum:root.pressure(root.oilPressureMaximum); greenStart:root.pressure(root.oilPressureMinimum); greenEnd:root.pressure(root.oilPressureMaximum*.85); majorStep:root.pressure(root.oilPressureMaximum)/5; minorStep:root.pressure(root.oilPressureMaximum)/25; decimals:root.metricUnits?1:0 }
        EmsGaugeV3 { width: (parent.width-2*parent.spacing)/3; height: parent.height; uiScale: root.uiScale; label: "OIL TEMP"; value:root.temp(controller.oilTemperature); unit:root.tempUnit; minimum:root.temp(20); maximum:root.temp(root.oilTemperatureMaximum*1.1); greenStart:root.temp(50); greenEnd:root.temp(root.oilTemperatureMaximum*.92); majorStep:(root.temp(root.oilTemperatureMaximum*1.1)-root.temp(20))/5; minorStep:(root.temp(root.oilTemperatureMaximum*1.1)-root.temp(20))/25 }
        EmsGaugeV3 { width: (parent.width-2*parent.spacing)/3; height: parent.height; uiScale: root.uiScale; label: "FUEL FLOW"; value:root.flow(controller.fuelFlow); unit:root.flowUnit; minimum:0; maximum:root.metricUnits?115:30; greenStart:root.metricUnits?11:3; greenEnd:root.metricUnits?87:23; majorStep:root.metricUnits?20:5; minorStep:root.metricUnits?4:1; decimals:1 }
    }

    Item { id: rightColumn; anchors { left: gauges.right; right: body.right; top: body.top; bottom: body.bottom; leftMargin: 8*root.uiScale } }
    Rectangle { id: cas; anchors { left: rightColumn.left; right: rightColumn.right; top: rightColumn.top }
        height: rightColumn.height*.42; color: root.panel; border.color: root.line; radius: 5*root.uiScale
        Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 5*root.uiScale }
        text: "CAS MESSAGES"; color: "#E7ECEE"; font.pixelSize: 10*root.uiScale }
        Rectangle { id:casBanner; x: 1; y: 25*root.uiScale; width: parent.width-2; height: 34*root.uiScale; color: root.casLevel==="WARNING"?"#45100F":"#07151D"; border.color: root.casColor
            SequentialAnimation on opacity { running:root.casLevel==="WARNING"&&!root.casAcknowledged; loops:Animation.Infinite; NumberAnimation { to:.55; duration:450 } NumberAnimation { to:1; duration:450 } }
            Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:4*root.uiScale } text:(root.casLevel==="NORMAL"?"✓   ":"⚠   ")+root.casHeadline; color:root.casColor; font.pixelSize:11*root.uiScale; font.bold:true }
            Text { anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:3*root.uiScale } text:root.casAction; color:"#D9E3E6"; font.pixelSize:5.5*root.uiScale; font.bold:root.casLevel!=="NORMAL" }
            TapHandler { onTapped:{root.selectedPanel="CAS";root.activeNavigation="INDICATIONS"} }
        }
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

    EngineTemperatureBars { anchors { left:rightColumn.left; right:rightColumn.right; top:cas.bottom; bottom:rightColumn.bottom; topMargin:7*root.uiScale } uiScale:root.uiScale; compact:false; chtValues:controller.chtValues; egtValues:controller.egtValues; temperatureUnit:root.tempUnit; chtMaximumC:root.chtMaximum; egtMaximumC:root.egtMaximum }

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
                EmsVectorIcon { x: 50*root.uiScale; anchors.verticalCenter: parent.verticalCenter; width: 27*root.uiScale; height: 27*root.uiScale; kind: modelData.k; strokeColor: modelData.n === "LEAN" ? (root.mixtureControlAvailable?root.green:"#66777E") : root.cyan }
                Text { anchors.centerIn: parent; text:modelData.n==="LEAN"&&!root.mixtureControlAvailable?"LEAN N/A":modelData.n; color:modelData.n==="LEAN"&&!root.mixtureControlAvailable?"#7D8B91":"#F0F3F4"; font.pixelSize: 14*root.uiScale; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: root.selectedPanel=modelData.n }
            } }
    }

    Rectangle { anchors.fill: parent; visible: root.selectedPanel !== ""; z: 50; color: "#A0000508"
        MouseArea { anchors.fill: parent; onClicked: root.closePanel() }
        Rectangle { anchors.centerIn: parent; width: 610*root.uiScale; height: 330*root.uiScale; radius: 8*root.uiScale; color: "#F0081720"; border.width: 2; border.color: root.cyan
            MouseArea { anchors.fill: parent }
            Text { x: 22*root.uiScale; y: 16*root.uiScale; text: root.selectedPanel; color: root.cyan; font.pixelSize: 21*root.uiScale; font.bold: true }
            Text { anchors { right: parent.right; top: parent.top; margins: 17*root.uiScale }
         text: "×"; color: "white"; font.pixelSize: 25*root.uiScale
                MouseArea { anchors.fill: parent; anchors.margins: -10*root.uiScale; onClicked: root.closePanel() } }
            Rectangle { x: 20*root.uiScale; y: 55*root.uiScale; width: parent.width-40*root.uiScale; height: 1; color: root.line }
            Text { visible:root.selectedPanel !== "SYSTEM" && root.selectedPanel !== "TREND" && root.selectedPanel !== "CHECKLIST" && root.selectedPanel !== "LEAN" && root.selectedPanel !== "FUEL" && root.selectedPanel !== "CAS"; x: 25*root.uiScale; y: 76*root.uiScale; width: parent.width-50*root.uiScale; wrapMode: Text.WordWrap
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
            Item { visible:root.selectedPanel === "CHECKLIST"; x:22*root.uiScale; y:67*root.uiScale; width:parent.width-44*root.uiScale; height:225*root.uiScale
                Text { text:"ENGINE / BEFORE TAKE-OFF"; color:"white"; font.pixelSize:12*root.uiScale; font.bold:true }
                Rectangle { anchors { right:parent.right; top:parent.top } width:150*root.uiScale; height:25*root.uiScale; radius:4*root.uiScale; color:root.checklistComplete?"#123D20":"#392C0B"; border.color:root.checklistComplete?root.green:root.amber
                    Text { anchors.centerIn:parent; text:root.checklistComplete?"✓ CHECKLIST COMPLETE":"CHECKLIST OPEN"; color:root.checklistComplete?root.green:root.amber; font.pixelSize:8*root.uiScale; font.bold:true }
                }
                Column { y:34*root.uiScale; width:parent.width; spacing:5*root.uiScale
                    Repeater { model:[
                        {title:"ENGINE PARAMETERS",detail:"Oil pressure / temperature within operating range",ok:root.checklistEngineOk,manual:false},
                        {title:"FUEL QUANTITY",detail:root.fuel(controller.fuelLeft+controller.fuelRight).toFixed(1)+" "+root.fuelUnit+" aboard • touch to confirm",ok:root.checklistFuelOk,manual:true,key:"fuel"},
                        {title:"ELECTRICAL SYSTEM",detail:Number(controller.busVoltage).toFixed(1)+" V • alternator "+Number(controller.alternatorCurrent).toFixed(0)+" A",ok:root.checklistElectricalOk,manual:false},
                        {title:"GEAR / FLAPS / AIRFRAME",detail:"Configuration visually verified • touch to confirm",ok:root.checklistAirframeOk,manual:true,key:"airframe"}]
                        delegate:Rectangle { required property var modelData; width:parent.width; height:38*root.uiScale; radius:4*root.uiScale; color:modelData.ok?"#0C2918":"#17232A"; border.color:modelData.ok?root.green:(modelData.manual?root.amber:root.line)
                            Rectangle { x:9*root.uiScale; anchors.verticalCenter:parent.verticalCenter; width:20*root.uiScale; height:width; radius:width/2; color:modelData.ok?root.green:"transparent"; border.width:2; border.color:modelData.ok?root.green:(modelData.manual?root.amber:"#788890")
                                Text { anchors.centerIn:parent; text:modelData.ok?"✓":"!"; color:modelData.ok?"#041008":(modelData.manual?root.amber:"#AAB6BB"); font.pixelSize:11*root.uiScale; font.bold:true }
                            }
                            Text { x:39*root.uiScale; y:6*root.uiScale; text:modelData.title; color:modelData.ok?root.green:"white"; font.pixelSize:9*root.uiScale; font.bold:true }
                            Text { x:39*root.uiScale; y:21*root.uiScale; text:modelData.detail; color:"#AFC0C7"; font.pixelSize:7*root.uiScale }
                            Text { anchors { right:parent.right; verticalCenter:parent.verticalCenter; rightMargin:12*root.uiScale } text:modelData.manual?"TOUCH":"AUTO"; color:modelData.manual?root.cyan:"#7E9098"; font.pixelSize:7*root.uiScale; font.bold:true }
                            TapHandler { enabled:modelData.manual; onTapped:{if(modelData.key==="fuel")root.checklistFuelConfirmed=!root.checklistFuelConfirmed;else root.checklistAirframeConfirmed=!root.checklistAirframeConfirmed} }
                        }
                    }
                }
                Rectangle { anchors { right:parent.right; bottom:parent.bottom } width:92*root.uiScale; height:22*root.uiScale; radius:3*root.uiScale; color:"#14242C"; border.color:root.line
                    Text { anchors.centerIn:parent; text:"RESET CHECKS"; color:"#D9E3E6"; font.pixelSize:7*root.uiScale; font.bold:true }
                    TapHandler { onTapped:{root.checklistFuelConfirmed=false;root.checklistAirframeConfirmed=false} }
                }
            }
            Item { visible:root.selectedPanel === "LEAN"; x:22*root.uiScale; y:67*root.uiScale; width:parent.width-44*root.uiScale; height:225*root.uiScale
                Text { text:"LEAN ASSIST • EGT PEAK MONITOR"; color:root.mixtureControlAvailable?"white":"#7D8B91"; font.pixelSize:12*root.uiScale; font.bold:true }
                Rectangle { anchors { right:parent.right; top:parent.top } width:112*root.uiScale; height:25*root.uiScale; radius:4*root.uiScale; color:root.mixtureControlAvailable?(root.leanActive?"#123D20":"#18252C"):"#20282C"; border.color:root.mixtureControlAvailable?(root.leanActive?root.green:root.line):"#66777E"
                    Text { anchors.centerIn:parent; text:root.mixtureControlAvailable?(root.leanActive?"● CAPTURING":"STANDBY"):"NOT INSTALLED"; color:root.mixtureControlAvailable?(root.leanActive?root.green:"#A8B5BA"):"#8C999E"; font.pixelSize:8*root.uiScale; font.bold:true }
                }
                Row { visible:root.mixtureControlAvailable; x:0; y:38*root.uiScale; width:parent.width; height:132*root.uiScale; spacing:7*root.uiScale
                    Repeater { model:controller.egtValues||[]
                        delegate:Rectangle { required property int index; required property real modelData; width:(parent.width-(Math.max(1,(controller.egtValues||[]).length)-1)*parent.spacing)/Math.max(1,(controller.egtValues||[]).length); height:parent.height; radius:4*root.uiScale; color:root.leanFirstCylinder===index?"#3A2D08":"#0A1820"; border.width:root.leanFirstCylinder===index?2:1; border.color:root.leanPeakDetected[index]?root.amber:root.line
                            Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:7*root.uiScale } text:"CYL "+(index+1); color:root.leanFirstCylinder===index?root.amber:root.cyan; font.pixelSize:8*root.uiScale; font.bold:true }
                            Text { anchors.centerIn:parent; anchors.verticalCenterOffset:-8*root.uiScale; text:Math.round(root.temp(modelData)); color:"white"; font.pixelSize:19*root.uiScale; font.bold:true }
                            Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.verticalCenter; topMargin:13*root.uiScale } text:root.tempUnit+"  EGT"; color:"#91A2A9"; font.pixelSize:7*root.uiScale }
                            Rectangle { anchors { left:parent.left; right:parent.right; bottom:parent.bottom; margins:7*root.uiScale } height:24*root.uiScale; radius:3*root.uiScale; color:root.leanPeakDetected[index]?"#3A2D08":"#12232B"
                                Text { anchors.centerIn:parent; text:root.leanPeakDetected[index]?(Math.round(root.temp(modelData)-root.temp(root.leanPeakValues[index]))+"° FROM PEAK"):(root.leanActive?"SEEKING PEAK":"—"); color:root.leanPeakDetected[index]?root.amber:"#A9B8BE"; font.pixelSize:6*root.uiScale; font.bold:true }
                            }
                        }
                    }
                }
                Text { visible:root.mixtureControlAvailable; x:0; y:178*root.uiScale; text:root.leanFirstCylinder>=0?"FIRST TO PEAK: CYLINDER "+(root.leanFirstCylinder+1):root.leanActive?"Slowly lean mixture • monitoring EGT rise":"Stabilize cruise power, then press START"; color:root.leanFirstCylinder>=0?root.amber:"#B8C5CA"; font.pixelSize:8*root.uiScale; font.bold:true }
                Column { visible:!root.mixtureControlAvailable; anchors.centerIn:parent; width:parent.width*.78; spacing:12*root.uiScale
                    Text { anchors.horizontalCenter:parent.horizontalCenter; text:"FUNCTION NOT APPLICABLE"; color:root.amber; font.pixelSize:17*root.uiScale; font.bold:true }
                    Text { width:parent.width; horizontalAlignment:Text.AlignHCenter; wrapMode:Text.WordWrap; text:String(controller.engineProfileName)+" profile has no pilot mixture control. EGT remains monitored cylinder by cylinder; Lean Assist is inhibited by configuration."; color:"#C4D0D4"; font.pixelSize:10*root.uiScale; lineHeight:1.35 }
                    Text { anchors.horizontalCenter:parent.horizontalCenter; text:"CAPABILITY SOURCE • ENGINE PROFILE"; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
                }
                Row { visible:root.mixtureControlAvailable; anchors { right:parent.right; bottom:parent.bottom } spacing:8*root.uiScale
                    Rectangle { width:90*root.uiScale; height:24*root.uiScale; radius:3*root.uiScale; color:"#14242C"; border.color:root.line
                        Text { anchors.centerIn:parent; text:"RESET"; color:"#D9E3E6"; font.pixelSize:7*root.uiScale; font.bold:true }
                        TapHandler { onTapped:{root.leanActive=false;root.resetLean()} }
                    }
                    Rectangle { width:112*root.uiScale; height:24*root.uiScale; radius:3*root.uiScale; color:root.leanActive?"#4A1917":"#0D5821"; border.color:root.leanActive?"#FF514A":root.green
                        Text { anchors.centerIn:parent; text:root.leanActive?"STOP CAPTURE":"START LEAN FIND"; color:"white"; font.pixelSize:7*root.uiScale; font.bold:true }
                        TapHandler { onTapped:{if(root.leanActive)root.leanActive=false;else root.startLean()} }
                    }
                }
            }
            Item { visible:root.selectedPanel === "FUEL"; x:22*root.uiScale; y:67*root.uiScale; width:parent.width-44*root.uiScale; height:225*root.uiScale
                Text { text:"FUEL COMPUTER"; color:"white"; font.pixelSize:12*root.uiScale; font.bold:true }
                Text { anchors { right:parent.right; top:parent.top } text:root.fuelLeakAlarm?"⚠ "+root.fuelLeakState:!root.fuelCalibrationReady?"CALIBRATION REQUIRED":root.fuelDiscrepancyGallons>3?"⚠ FUEL DISAGREE":root.fuelImbalanceGallons>3?"⚠ TANK IMBALANCE":"LEAK MONITOR "+root.fuelLeakState; color:(root.fuelLeakAlarm||!root.fuelCalibrationReady||root.fuelDiscrepancyGallons>3||root.fuelImbalanceGallons>3)?root.amber:root.green; font.pixelSize:8*root.uiScale; font.bold:true }
                Row { x:0; y:35*root.uiScale; width:parent.width; height:86*root.uiScale; spacing:7*root.uiScale
                    Repeater { model:[
                        {h:"REMAINING",v:root.fuel(root.fuelTotalGallons).toFixed(1),u:root.fuelUnit,c:root.fuelTotalGallons<=root.fuelReserveGallons?root.amber:"white"},
                        {h:"FLOW",v:root.flow(controller.fuelFlow).toFixed(1),u:root.flowUnit,c:root.cyan},
                        {h:"TO RESERVE",v:controller.fuelFlow>.1?root.fuelEnduranceHours.toFixed(1):"--",u:"HOURS",c:controller.fuelFlow>.1?(root.fuelEnduranceHours<.75?root.amber:root.green):"#899BA3"},
                        {h:"RESERVE TIME",v:controller.fuelFlow>.1?Math.round(root.fuelReserveMinutes):"--",u:"MIN",c:root.amber}]
                        delegate:Rectangle { required property var modelData; width:(parent.width-3*parent.spacing)/4; height:parent.height; radius:4*root.uiScale; color:"#091821"; border.color:root.line
                            Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:9*root.uiScale } text:modelData.h; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
                            Text { anchors.centerIn:parent; anchors.verticalCenterOffset:3*root.uiScale; text:modelData.v; color:modelData.c; font.pixelSize:21*root.uiScale; font.bold:true }
                            Text { anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:8*root.uiScale } text:modelData.u; color:"#9DAEB5"; font.pixelSize:7*root.uiScale }
                        }
                    }
                }
                Rectangle { x:0; y:130*root.uiScale; width:parent.width*.55; height:68*root.uiScale; radius:4*root.uiScale; color:"#091821"; border.color:root.line
                    Text { x:12*root.uiScale; y:8*root.uiScale; text:"TANK QUANTITY"; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
                    Text { anchors { right:parent.right; top:parent.top; rightMargin:10*root.uiScale; topMargin:7*root.uiScale } text:"SET FOB "+root.fuel(root.fuelMeasuredGallons).toFixed(1); color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true; TapHandler { onTapped:controller.setFuelOnBoardGallons(root.fuelMeasuredGallons) } }
                    Repeater { model:[{n:"LEFT",v:controller.fuelLeft},{n:"RIGHT",v:controller.fuelRight}]
                        delegate:Item { required property var modelData; required property int index; x:12*root.uiScale; y:(index+1)*22*root.uiScale; width:parent.width-24*root.uiScale; height:14*root.uiScale
                            readonly property int calibrationCount:index===0?root.fuelCalibrationLeftCount:root.fuelCalibrationRightCount
                            Text { width:62*root.uiScale; text:parent.calibrationCount===5?modelData.n+" CAL OK":modelData.n+" "+(parent.calibrationCount*25)+"%"; color:parent.calibrationCount===5?root.green:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true
                                TapHandler { onTapped:if(parent.parent.calibrationCount<5)controller.captureFuelCalibrationPoint(parent.parent.modelData.n); onLongPressed:controller.clearFuelCalibration(parent.parent.modelData.n) } }
                            Rectangle { x:67*root.uiScale; width:parent.width-124*root.uiScale; height:8*root.uiScale; color:"#1A2A31"; border.color:"#53656D"
                                Rectangle { anchors { left:parent.left; top:parent.top; bottom:parent.bottom; margins:1 } width:(parent.width-2)*Math.max(0,Math.min(1,modelData.v/20)); color:modelData.v<4?"#FF3933":modelData.v<7?root.amber:root.green }
                            }
                            Text { anchors.right:parent.right; text:root.fuel(modelData.v).toFixed(1)+" "+root.fuelUnit; color:"white"; font.pixelSize:7*root.uiScale; font.bold:true }
                        }
                    }
                }
                Rectangle { anchors { right:parent.right; top:parent.top; topMargin:130*root.uiScale } width:parent.width*.42; height:68*root.uiScale; radius:4*root.uiScale; color:"#091821"; border.color:root.line
                    Text { x:12*root.uiScale; y:8*root.uiScale; text:"FINAL RESERVE"; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
                    Text { anchors.centerIn:parent; text:root.fuel(root.fuelReserveGallons).toFixed(1)+" "+root.fuelUnit; color:root.amber; font.pixelSize:18*root.uiScale; font.bold:true }
                    Rectangle { anchors { left:parent.left; verticalCenter:parent.verticalCenter; leftMargin:10*root.uiScale } width:30*root.uiScale; height:26*root.uiScale; radius:3*root.uiScale; color:"#182932"; border.color:root.line; Text { anchors.centerIn:parent; text:"−"; color:"white"; font.pixelSize:16*root.uiScale } TapHandler { onTapped:controller.setFuelReserveGallons(Math.max(0,root.fuelReserveGallons-1)) } }
                    Rectangle { anchors { right:parent.right; verticalCenter:parent.verticalCenter; rightMargin:10*root.uiScale } width:30*root.uiScale; height:26*root.uiScale; radius:3*root.uiScale; color:"#182932"; border.color:root.line; Text { anchors.centerIn:parent; text:"+"; color:"white"; font.pixelSize:16*root.uiScale } TapHandler { onTapped:controller.setFuelReserveGallons(Math.min(root.fuelTotalGallons,root.fuelReserveGallons+1)) } }
                }
                Text { x:0; y:207*root.uiScale; text:"FOB "+root.fuel(controller.fuelOnBoardGallons).toFixed(1)+" • USED "+root.fuel(root.fuelUsedGallons).toFixed(1)+" • LEAK Δ "+Number(controller.fuelLeakExcessGph||0).toFixed(1)+" GPH"; color:root.fuelLeakAlarm?root.amber:"#7F929A"; font.pixelSize:7*root.uiScale }
                Text { anchors { right:parent.right; bottom:parent.bottom } text:"RESET USED"; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true; TapHandler { onTapped:controller.resetFuelUsed() } }
            }
            Item { visible:root.selectedPanel === "CAS"; x:22*root.uiScale; y:67*root.uiScale; width:parent.width-44*root.uiScale; height:225*root.uiScale
                Text { text:"CAS EVENT LOG • CURRENT SESSION"; color:"white"; font.pixelSize:12*root.uiScale; font.bold:true }
                Rectangle { anchors { right:parent.right; top:parent.top } width:118*root.uiScale; height:24*root.uiScale; radius:3*root.uiScale; color:root.casAcknowledged?"#123A2A":"#40270B"; border.color:root.casAcknowledged?root.green:root.amber
                    Text { anchors.centerIn:parent; text:root.casAcknowledged?"ACKNOWLEDGED":"ACKNOWLEDGE"; color:root.casAcknowledged?root.green:root.amber; font.pixelSize:7*root.uiScale; font.bold:true }
                    TapHandler { onTapped:root.casAcknowledged=true }
                }
                Rectangle { x:0; y:33*root.uiScale; width:parent.width; height:25*root.uiScale; color:"#10232C"; border.color:root.line
                    Text { x:8*root.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"TIME"; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
                    Text { x:68*root.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"PRIORITY"; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
                    Text { x:145*root.uiScale; anchors.verticalCenter:parent.verticalCenter; text:"MESSAGE / CREW ACTION"; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
                    Text { anchors { right:parent.right; rightMargin:9*root.uiScale; verticalCenter:parent.verticalCenter } text:"STATE"; color:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
                }
                ListView { x:0; y:60*root.uiScale; width:parent.width; height:137*root.uiScale; clip:true; spacing:2*root.uiScale; model:root.casEvents
                    delegate:Rectangle { required property var modelData; required property int index; width:ListView.view.width; height:43*root.uiScale; color:index%2?"#0A1820":"#0D1D25"; border.color:modelData.status==="ACTIVE"?(modelData.level==="WARNING"?"#FF3933":root.amber):root.line
                        Text { x:8*root.uiScale; y:7*root.uiScale; text:modelData.time; color:"#DCE5E8"; font.pixelSize:7*root.uiScale }
                        Text { x:68*root.uiScale; y:7*root.uiScale; text:modelData.level; color:modelData.level==="WARNING"?"#FF3933":modelData.level==="CAUTION"?root.amber:root.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
                        Text { x:145*root.uiScale; y:6*root.uiScale; text:modelData.message; color:"white"; font.pixelSize:8*root.uiScale; font.bold:true }
                        Text { x:145*root.uiScale; y:23*root.uiScale; width:300*root.uiScale; elide:Text.ElideRight; text:modelData.action; color:"#9FB0B7"; font.pixelSize:6*root.uiScale }
                        Text { anchors { right:parent.right; rightMargin:9*root.uiScale; verticalCenter:parent.verticalCenter } text:modelData.status; color:modelData.status==="ACTIVE"?root.amber:"#7F929A"; font.pixelSize:7*root.uiScale; font.bold:true }
                    }
                    Text { visible:root.casEvents.length===0; anchors.centerIn:parent; text:"NO CAS EVENTS RECORDED THIS SESSION"; color:root.green; font.pixelSize:8*root.uiScale; font.bold:true }
                }
                Text { anchors { left:parent.left; bottom:parent.bottom } text:"Permanent evidence is recorded by BLACKBOX • "+String(controller.blackBoxState||"OFFLINE"); color:"#80939B"; font.pixelSize:7*root.uiScale }
            }
            Text { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 22*root.uiScale }
         text: "TOUCH OUTSIDE TO CLOSE"; horizontalAlignment: Text.AlignHCenter; color: "#899BA3"; font.pixelSize: 8*root.uiScale; font.letterSpacing: 1.4 }
        }
    }

    EmsSetupOverlay { anchors.fill:parent; controller:root.controller; requestedSection:root.setupSection; visible:root.setupVisible; onClosed:{root.setupVisible=false;root.activeNavigation="ENGINE"} }
}
