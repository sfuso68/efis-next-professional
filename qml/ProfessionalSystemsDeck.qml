import QtQuick

Rectangle {
    id: deck
    required property var controller
    property real uiScale: 1
    property string engineDataState: "FAIL"
    readonly property bool airframeUsable: ["LIVE","SIM","SIM FORCED","DEGRADED"].indexOf(String(controller.airframeSourceState||""))>=0 && typeof controller.airframeDataAgeMs==="number" && isFinite(controller.airframeDataAgeMs) && controller.airframeDataAgeMs>=0
    readonly property string flapReading: airframeUsable && controller.flapsInstalled===true && controller.flapAngleValid===true && typeof controller.flapAngle==="number" && isFinite(controller.flapAngle) ? Math.round(controller.flapAngle)+"°"+(String(controller.airframeSourceState).indexOf("SIM")===0?" SIM":"") : "—"
    readonly property bool dataUsable: ["LIVE","SIM","SIM FORCED","DEGRADED"].indexOf(String(controller.engineSourceState||""))>=0
    function reading(v,d,valid) { return dataUsable && valid===true && typeof v==="number" && isFinite(v) ? v.toFixed(d) : "—" }
    readonly property var engineLimits: controller.engineLimits||({})
    function limit(name,fallback) { var value=Number(engineLimits[name]); return isFinite(value)&&value>0?value:fallback }
    readonly property string units: controller.unitSystem || "AVIATION"
    readonly property bool metricUnits: units === "METRIC"
    readonly property bool imperialUnits: units === "IMPERIAL"
    function temp(v) { return imperialUnits ? v*9/5+32 : v }
    function pressure(v) { return metricUnits ? v*0.0689476 : v }
    function fuel(v) { return metricUnits ? v*3.78541 : v }
    function flow(v) { return metricUnits ? v*3.78541 : v }
    function mapValue(v) { return metricUnits ? v*33.8639 : v }
    readonly property string tempUnit: imperialUnits ? "°F" : "°C"
    Theme { id: theme }
    color: "#050D14"; border.color: theme.borderBright

    Row {
        anchors { fill: parent; margins: 5*deck.uiScale }
        spacing: 5*deck.uiScale

        Rectangle {
            width: 730*deck.uiScale; height: parent.height; color: "#D9071119"; border.color: theme.border; radius: 3*deck.uiScale
            Text { x: 8*deck.uiScale; y: 5*deck.uiScale; text: "ENGINE • " + deck.engineDataState; color: theme.secondary; font.pixelSize: 7*deck.uiScale; font.bold: true }
            Row { anchors { left: parent.left; right: temperatures.left; top: parent.top; bottom: parent.bottom; margins: 5*deck.uiScale; topMargin: 20*deck.uiScale } spacing: 3*deck.uiScale
                Repeater { model: [
                    {n:"RPM",ok:controller.engineRpmValid===true,v:controller.engineRpm,u:"RPM",min:0,max:deck.limit("rpm_max",3000),gs:deck.limit("rpm_max",3000)*.24,ge:deck.limit("rpm_max",3000)*.95,d:0},
                    {n:"MAP",ok:controller.manifoldPressureValid===true,v:deck.mapValue(controller.manifoldPressure),u:deck.metricUnits?"HPA":"IN HG",min:deck.metricUnits?340:10,max:deck.metricUnits?1185:35,gs:deck.metricUnits?508:15,ge:deck.metricUnits?982:29,d:deck.metricUnits?0:1},
                    {n:"FLOW",ok:controller.fuelFlowValid===true,v:deck.flow(controller.fuelFlow),u:deck.metricUnits?"L/H":"GPH",min:0,max:deck.metricUnits?76:20,gs:deck.metricUnits?15:4,ge:deck.metricUnits?61:16,d:1},
                    // Stessa correzione di AdaptiveEmsView.qml, e per la stessa
                    // ragione: il valore e' in °C. Qui pesa di piu', perche'
                    // questo pannello sta sulla pagina di volo.
                    {n:"OIL T",ok:controller.oilTemperatureValid===true,v:deck.temp(controller.oilTemperature),u:deck.tempUnit,min:deck.temp(20),max:deck.temp(deck.limit("oil_temperature_max_c",120)*1.1),gs:deck.temp(50),ge:deck.temp(deck.limit("oil_temperature_max_c",120)*.92),d:0},
                    {n:"OIL P",ok:controller.oilPressureValid===true,v:deck.pressure(controller.oilPressure),u:deck.metricUnits?"BAR":"PSI",min:0,max:deck.pressure(deck.limit("oil_pressure_max_psi",100)),gs:deck.pressure(deck.limit("oil_pressure_min_psi",25)),ge:deck.pressure(deck.limit("oil_pressure_max_psi",100)*.85),d:deck.metricUnits?1:0}
                ]; delegate: EmsGaugeV3 { required property var modelData; width: 108*deck.uiScale; height: parent.height;
                        uiScale: deck.uiScale*.64; label: modelData.n; value: modelData.v; unit: modelData.u;
                        compact:true; valueValid:deck.dataUsable && modelData.ok && isFinite(modelData.v);
                        minimum: modelData.min; maximum: modelData.max; greenStart: modelData.gs; greenEnd: modelData.ge; decimals: modelData.d;
                        majorStep:(modelData.max-modelData.min)/5; minorStep:(modelData.max-modelData.min)/25
                } }
            }
            EngineTemperatureBars { id: temperatures; anchors { right:parent.right; top:parent.top; bottom:parent.bottom; margins:5*deck.uiScale; topMargin:20*deck.uiScale } width:150*deck.uiScale; uiScale:deck.uiScale; compact:true; dataState:String(controller.engineSourceState||"LOST"); chtValues:controller.chtValues; egtValues:controller.egtValues; temperatureUnit:deck.tempUnit; chtMaximumC:deck.limit("cht_max_c",240); egtMaximumC:deck.limit("egt_max_c",920) }
        }

        Rectangle { id:fuelPanel; width:105*deck.uiScale; height:parent.height; color:"#D9071119"; border.color:theme.border; radius:3*deck.uiScale
            Text { x:8*deck.uiScale; y:5*deck.uiScale; text:"FUEL"; color:theme.secondary; font.pixelSize:7*deck.uiScale; font.bold:true }
            Text { anchors { right:parent.right; rightMargin:8*deck.uiScale; top:parent.top; topMargin:4*deck.uiScale } text:deck.reading(deck.fuel(controller.fuelLeft+controller.fuelRight),1,controller.fuelLeftValid===true&&controller.fuelRightValid===true); color:theme.primary; font.pixelSize:13*deck.uiScale; font.bold:true }
            Column { x:7*deck.uiScale; y:29*deck.uiScale; width:parent.width-14*deck.uiScale; spacing:8*deck.uiScale
                Repeater { model:[{n:"L",v:controller.fuelLeft,ok:controller.fuelLeftValid===true},{n:"R",v:controller.fuelRight,ok:controller.fuelRightValid===true}]
                    delegate:Item { required property var modelData; width:parent.width; height:34*deck.uiScale
                        Text { x:0; y:0; text:modelData.n; color:theme.cyan; font.pixelSize:7*deck.uiScale; font.bold:true }
                        Text { anchors { right:parent.right; top:parent.top } text:deck.reading(deck.fuel(Number(modelData.v)),1,modelData.ok); color:"white"; font.pixelSize:8*deck.uiScale; font.bold:true }
                        Rectangle { anchors { left:parent.left; right:parent.right; bottom:parent.bottom } height:9*deck.uiScale; color:"#142630"; border.color:"#60727B"
                            Rectangle { anchors { left:parent.left; top:parent.top; bottom:parent.bottom; margins:1 } visible:deck.dataUsable&&modelData.ok&&isFinite(modelData.v); width:visible?(parent.width-2)*Math.max(0,Math.min(1,Number(modelData.v)/20)):0; color:Number(modelData.v)<4?theme.warning:Number(modelData.v)<7?theme.amber:theme.green }
                            Repeater { model:5; delegate:Rectangle { required property int index; x:index*(parent.width-1)/4; width:1; height:parent.height; color:"#667780" } }
                        }
                    }
                }
            }
            Text { anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:6*deck.uiScale } text:(deck.metricUnits?"L":"GAL")+" USABLE • "+deck.engineDataState; color:theme.cyan; font.pixelSize:6*deck.uiScale; font.bold:true }
        }

        Rectangle { width:68*deck.uiScale; height: parent.height; color: "#D9071119"; border.color: theme.border; radius: 3*deck.uiScale
            Text { x: 8*deck.uiScale; y: 5*deck.uiScale; text: "ELECTRICAL"; color: theme.secondary; font.pixelSize: 7*deck.uiScale; font.bold: true }
            Column { anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 8*deck.uiScale } spacing: 6*deck.uiScale
                Column { width: parent.width; spacing: 0; Text { text: "BUS"; color: theme.secondary; font.pixelSize: 6*deck.uiScale; font.bold:true } Text { id: busValue; text: deck.reading(controller.busVoltage,1,controller.busVoltageValid===true)+" V"; color: theme.cyan; font.pixelSize: 14*deck.uiScale; font.bold: true } }
                Rectangle { width: parent.width; height: 1; color: theme.border }
                Column { width: parent.width; spacing: 0; Text { text: "ALT"; color: theme.secondary; font.pixelSize: 6*deck.uiScale; font.bold:true } Text { id: altValue; text: deck.reading(controller.alternatorCurrent,0,controller.alternatorCurrentValid===true)+" A"; color: theme.cyan; font.pixelSize: 14*deck.uiScale; font.bold: true } }
            }
        }

        Rectangle { width:96*deck.uiScale; height: parent.height; color: "#D9071119"; border.color: theme.border; radius: 3*deck.uiScale
            Text { x: 8*deck.uiScale; y: 5*deck.uiScale; text: "AIRCRAFT SYSTEMS"; color: theme.secondary; font.pixelSize: 7*deck.uiScale; font.bold: true }
            Grid { anchors.centerIn: parent; columns: 2; columnSpacing: 14*deck.uiScale; rowSpacing: 5*deck.uiScale
                Repeater { model: [{n:"PITOT",v:"—"},{n:"ICE",v:"—"},{n:"TRIM",v:"—"},{n:"FLAPS",v:deck.flapReading},{n:"T/O",v:"—"},{n:"REC",v:controller.blackBoxState}]; delegate: Column { required property var modelData; spacing: 2*deck.uiScale
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.n; color: theme.secondary; font.pixelSize: 6*deck.uiScale; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.v; color: modelData.v==="—" || modelData.v==="OFF" ? theme.secondary : modelData.n==="FLAPS" ? theme.cyan : theme.green; font.pixelSize: 9*deck.uiScale; font.bold: true }
                } }
            }
        }
    }
}
