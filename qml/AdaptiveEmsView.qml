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
        text: modelData.n; color: modelData.n === "ENGINE" ? root.cyan : "#A7B0B5"; font.pixelSize: 8*root.uiScale } } }
        } }

    Item { id: body; anchors { left: nav.right; right: parent.right; top: header.bottom; bottom: summary.top; leftMargin: 8*root.uiScale; rightMargin: 8*root.uiScale; topMargin: 5*root.uiScale } }
    Item { id: gauges; anchors { left: body.left; top: body.top; bottom: body.bottom }
        width: body.width*.63 }
    Row { id: primary; anchors { left: gauges.left; right: gauges.right; top: gauges.top }
        height: gauges.height*.59; spacing: 7*root.uiScale
        EmsGaugeV3 { width: (parent.width-parent.spacing)/2; height: parent.height; uiScale: root.uiScale; dominant: true; label: "RPM"; value: controller.engineRpm; unit: "RPM"; minimum: 0; maximum: 3000; greenStart: 800; greenEnd: 2600; majorStep: 500; minorStep: 100 }
        EmsGaugeV3 { width: (parent.width-parent.spacing)/2; height: parent.height; uiScale: root.uiScale; dominant: true; label: "MAP"; value: controller.manifoldPressure; unit: "IN HG"; minimum: 0; maximum: 40; greenStart: 5; greenEnd: 34; majorStep: 5; minorStep: 1; decimals: 1 }
    }
    Row { anchors { left: gauges.left; right: gauges.right; top: primary.bottom; bottom: gauges.bottom; topMargin: -3*root.uiScale }
        spacing: 5*root.uiScale
        EmsGaugeV3 { width: (parent.width-2*parent.spacing)/3; height: parent.height; uiScale: root.uiScale; label: "OIL PRESS"; value: controller.oilPressure; unit: "PSI"; minimum: 0; maximum: 100; greenStart: 25; greenEnd: 85; majorStep: 20; minorStep: 4 }
        EmsGaugeV3 { width: (parent.width-2*parent.spacing)/3; height: parent.height; uiScale: root.uiScale; label: "OIL TEMP"; value: controller.oilTemperature; unit: "°C"; minimum: 20; maximum: 140; greenStart: 50; greenEnd: 110; majorStep: 20; minorStep: 4 }
        EmsGaugeV3 { width: (parent.width-2*parent.spacing)/3; height: parent.height; uiScale: root.uiScale; label: "FUEL FLOW"; value: controller.fuelFlow; unit: "GPH"; minimum: 0; maximum: 30; greenStart: 3; greenEnd: 23; majorStep: 5; minorStep: 1; decimals: 1 }
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

    EngineTemperatureBars { anchors { left:rightColumn.left; right:rightColumn.right; top:cas.bottom; bottom:rightColumn.bottom; topMargin:7*root.uiScale } uiScale:root.uiScale; compact:false; chtValues:controller.chtValues; egtValues:controller.egtValues }

    Rectangle { id: summary; anchors { left: nav.right; right: parent.right; bottom: actions.top; leftMargin: 8*root.uiScale; rightMargin: 8*root.uiScale; bottomMargin: 7*root.uiScale }
        height: 72*root.uiScale; color: root.panel; border.color: root.line; radius: 5*root.uiScale
        Row { anchors.fill: parent
            Repeater { model: [
                {h:"FUEL  "+(controller.fuelLeft+controller.fuelRight).toFixed(1)+" GAL",v:"L  "+controller.fuelLeft.toFixed(1)+"     R  "+controller.fuelRight.toFixed(1)+" GAL",c:root.green},
                {h:"ENDURANCE",v:((controller.fuelLeft+controller.fuelRight)/Math.max(.1,controller.fuelFlow)).toFixed(1)+" HR",c:root.cyan},
                {h:"BUS",v:controller.busVoltage.toFixed(1)+" V",c:"white"},
                {h:"ALT",v:"+"+controller.alternatorCurrent.toFixed(0)+" A",c:"white"}]
                delegate: Item { required property var modelData; width: parent.width/4; height: parent.height
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
            Text { x: 25*root.uiScale; y: 76*root.uiScale; width: parent.width-50*root.uiScale; wrapMode: Text.WordWrap
                text: root.selectedPanel === "LEAN" ? "LEAN ASSIST\nMonitoraggio comparato EGT per cilindro. Identificazione automatica del primo cilindro al picco e memorizzazione della variazione dal picco." :
                      root.selectedPanel === "FUEL" ? "FUEL COMPUTER\nCarburante misurato: "+(controller.fuelLeft+controller.fuelRight).toFixed(1)+" GAL\nFlusso: "+controller.fuelFlow.toFixed(1)+" GPH\nAutonomia stimata: "+((controller.fuelLeft+controller.fuelRight)/Math.max(.1,controller.fuelFlow)).toFixed(1)+" HR" :
                      root.selectedPanel === "TREND" ? "ENGINE TREND\nRPM, pressioni, temperature e parametri elettrici saranno rappresentati su finestre selezionabili da 1, 5, 15 e 60 minuti." :
                      "ENGINE CHECKLIST\n□ Parametri motore nel campo verde\n□ Carburante e autonomia verificati\n□ Alternatore e tensione verificati\n□ Carrello e flap configurati"
                color: "#E8EEF0"; font.pixelSize: 15*root.uiScale; lineHeight: 1.5 }
            Text { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 22*root.uiScale }
         text: "TOUCH OUTSIDE TO CLOSE"; horizontalAlignment: Text.AlignHCenter; color: "#899BA3"; font.pixelSize: 8*root.uiScale; font.letterSpacing: 1.4 }
        }
    }

    EmsSetupOverlay { anchors.fill:parent; controller:root.controller; visible:root.setupVisible; onClosed:root.setupVisible=false }
}
