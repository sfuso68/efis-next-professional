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
    }

    Rectangle { id: nav; anchors { left: parent.left; top: header.bottom; bottom: actions.top }
        width: 96*root.uiScale; color: "#B00A151D"; border.color: root.line; radius: 5*root.uiScale
        Column { anchors.fill: parent
            Repeater { model: [{i:"⚙",n:"ENGINE"},{i:"▥",n:"INDICATIONS"},{i:"⌁",n:"ELECTRICAL"},{i:"▣",n:"FUEL"},{i:"⚙",n:"SYSTEM"},{i:"☑",n:"MAINT"}]
                delegate: Rectangle { required property var modelData; width: nav.width; height: nav.height/6; color: modelData.n === "ENGINE" ? "#172832" : "transparent"; border.color: "#35454D"
                    Rectangle { visible: modelData.n === "ENGINE"; width: 4*root.uiScale; height: parent.height; color: root.cyan }
                    Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 10*root.uiScale }
        text: modelData.i; color: modelData.n === "ENGINE" ? root.cyan : "#89969C"; font.pixelSize: 22*root.uiScale }
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
            Text { anchors.centerIn: parent; text: "✈"; rotation: 90; color: "#E5EAEC"; font.pixelSize: 32*root.uiScale }
            Row { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 20*root.uiScale }
        spacing: 18*root.uiScale
                Repeater { model: ["NOSE","LEFT","RIGHT"]; delegate: Column { required property string modelData; spacing: 2
                    Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 15*root.uiScale; height: width; radius: width/2; color: "#092416"; border.width: 2; border.color: root.green }
                    Text { text: modelData; color: "#E1E7EA"; font.pixelSize: 6*root.uiScale } } } }
            Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 3*root.uiScale }
        text: controller.gearState; color: root.green; font.pixelSize: 8*root.uiScale; font.bold: true }
        }
        Item { visible: controller.flapsInstalled; x: parent.width/2+6*root.uiScale; y: 83*root.uiScale; width: parent.width/2-12*root.uiScale; height: parent.height-85*root.uiScale
            Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top }
        text: "FLAPS"; color: root.cyan; font.pixelSize: 10*root.uiScale }
            Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 26*root.uiScale }
        text: Math.round(controller.flapAngle)+"°"; color: "white"; font.pixelSize: 28*root.uiScale; font.bold: true }
            Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 25*root.uiScale }
        height: 2; color: "#AAB7BD"
                Rectangle { x: parent.width*controller.flapAngle/30-4*root.uiScale; y: -7*root.uiScale; width: 8*root.uiScale; height: 10*root.uiScale; color: root.cyan; rotation: 45 } }
            Row { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: 10*root.uiScale }
                Repeater { model: controller.flapDetents; delegate: Text { required property real modelData; width: parent.width/5; text: modelData+"°"; color: "#E2E8EA"; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 6*root.uiScale } } }
            Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
        text: "FLAPS "+Math.round(controller.flapAngle)+"°"; color: root.green; font.pixelSize: 8*root.uiScale } }
    }

    Rectangle { anchors { left: rightColumn.left; right: rightColumn.right; top: cas.bottom; bottom: rightColumn.bottom; topMargin: 7*root.uiScale }
        color: root.panel; border.color: root.line; radius: 5*root.uiScale
        Text { x: 10*root.uiScale; y: 5*root.uiScale; text: "CHT / EGT"; color: root.cyan; font.pixelSize: 9*root.uiScale; font.bold: true }
        Row { anchors { fill: parent; topMargin: 24*root.uiScale; leftMargin: 31*root.uiScale; rightMargin: 8*root.uiScale; bottomMargin: 7*root.uiScale }
        spacing: 4*root.uiScale
            Repeater { model: 4; delegate: Item { required property int index; width: (parent.width-3*parent.spacing)/4; height: parent.height
                Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top }
        text: "CYL "+(index+1); color: "#E5EAEC"; font.pixelSize: 7*root.uiScale }
                Column { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 18*root.uiScale; bottom: parent.bottom }
        spacing: 7*root.uiScale
                    Repeater { model: [{n:"CHT",v:Number(controller.chtValues[index]),lo:100,hi:500,c:root.green},{n:"EGT",v:Number(controller.egtValues[index]),lo:300,hi:900,c:root.green}]
                        delegate: Item { required property var modelData; width: 54*root.uiScale; height: (parent.height-parent.spacing)/2
                            Rectangle { x: 18*root.uiScale; width: 11*root.uiScale; height: parent.height-14*root.uiScale; color: "#27343A"; border.color: "#65737A"
                                Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 2 }
        height: Math.max(2,(modelData.v-modelData.lo)/(modelData.hi-modelData.lo)*(parent.height-4)); color: modelData.c } }
                            Repeater { model: 5; delegate: Rectangle { required property int index; x: 11*root.uiScale; y: index*(parent.height-15*root.uiScale)/4; width: 6*root.uiScale; height: 1; color: "#D1D9DC" } }
                            Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
        text: Math.round(modelData.v); color: root.green; font.pixelSize: 8*root.uiScale; font.bold: true } } }
                } } }
        }
        Text { x: 5*root.uiScale; y: parent.height*.32; text: "CHT\n°C"; color: root.cyan; font.pixelSize: 7*root.uiScale; horizontalAlignment: Text.AlignHCenter }
        Text { x: 5*root.uiScale; y: parent.height*.70; text: "EGT\n°C"; color: root.cyan; font.pixelSize: 7*root.uiScale; horizontalAlignment: Text.AlignHCenter }
    }

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
        Repeater { model: [{i:"◒",n:"LEAN"},{i:"▣",n:"FUEL"},{i:"⌁",n:"TREND"},{i:"☑",n:"CHECKLIST"}]
            delegate: Rectangle { required property var modelData; width: (parent.width-3*parent.spacing)/4; height: parent.height; radius: 5*root.uiScale; color: "#101D25"; border.color: root.line
                Text { x: 58*root.uiScale; anchors.verticalCenter: parent.verticalCenter; text: modelData.i; color: modelData.n === "LEAN" ? root.green : root.cyan; font.pixelSize: 25*root.uiScale }
                Text { anchors.centerIn: parent; text: modelData.n; color: "#F0F3F4"; font.pixelSize: 14*root.uiScale; font.bold: true } } }
    }
}
