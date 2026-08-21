import QtQuick

Item {
    id: root
    required property var controller
    property real uiScale: Math.min(width/1024, height/600)
    readonly property color healthColor: controller.engineHealth >= 2 ? theme.warning : controller.engineHealth === 1 ? theme.amber : theme.green
    Theme { id: theme }
    Rectangle { anchors.fill: parent; color: theme.background }
    CommandRail { id: rail; anchors { left: parent.left; right: parent.right; top: parent.top } height: 54*root.uiScale; uiScale: root.uiScale
        viewName: "EMS • ENGINE SYSTEMS"; healthLevel: controller.engineHealth
        alertText: controller.engineDataState === "FAIL" ? "NO ENGINE DATA" : controller.engineFaultMode === "NONE" ? "ENGINE NORMAL" : controller.engineFaultMode; alertSeverity: controller.engineHealth
        flightDirectorActive: false; selectedAltitude: 0; dataSourceState: controller.dataSourceState; sensorNodeState: controller.sensorNodeState; blackBoxState: controller.blackBoxState; blackBoxRecords: controller.blackBoxRecords }
    Item { id: content; anchors { left: parent.left; right: parent.right; top: rail.bottom; bottom: footer.top; margins: 7*root.uiScale } }

    Rectangle { id: gaugesPanel; anchors { left: content.left; right: alerts.left; top: content.top; rightMargin: 7*root.uiScale }
        height: 174*root.uiScale; radius: 4*root.uiScale; color: "#CA08131C"; border.color: theme.borderBright
        Text { x: 10*root.uiScale; y: 6*root.uiScale; text: "PRIMARY ENGINE INSTRUMENTS • " + controller.engineDataState; color: theme.cyan; font.pixelSize: 8*root.uiScale; font.bold: true }
        Row { anchors { fill: parent; margins: 8*root.uiScale; topMargin: 24*root.uiScale } spacing: 5*root.uiScale
            Repeater { model: [
                {n:"RPM",v:controller.engineRpm,u:"RPM",min:0,max:3000,gs:800,ge:2600,d:0},
                {n:"MANIFOLD",v:controller.manifoldPressure,u:"IN HG",min:10,max:35,gs:15,ge:29,d:1},
                {n:"FUEL FLOW",v:controller.fuelFlow,u:"GPH",min:0,max:20,gs:4,ge:16,d:1},
                {n:"OIL TEMP",v:controller.oilTemperature,u:"°F",min:80,max:260,gs:140,ge:220,d:0},
                {n:"OIL PRESS",v:controller.oilPressure,u:"PSI",min:0,max:100,gs:50,ge:90,d:0}
            ]; delegate: Rectangle { required property var modelData; width: 144*root.uiScale; height: parent.height; radius: 4*root.uiScale; color: "#A9050D14"; border.color: "#354F5D"
                    ProfessionalCircularGauge { anchors { fill: parent; margins: 5*root.uiScale } uiScale: root.uiScale*1.35; label: modelData.n; value: modelData.v; unit: modelData.u
                        minimum: modelData.min; maximum: modelData.max; greenStart: modelData.gs; greenEnd: modelData.ge; decimals: modelData.d }
            } }
        }
    }
    Rectangle { id: alerts; anchors { right: content.right; top: content.top } width: 244*root.uiScale; height: 174*root.uiScale; radius: 4*root.uiScale; color: "#CA08131C"; border.color: root.healthColor
        Text { x: 10*root.uiScale; y: 7*root.uiScale; text: "ENGINE STATUS"; color: theme.secondary; font.pixelSize: 8*root.uiScale; font.bold: true }
        Text { x: 10*root.uiScale; y: 27*root.uiScale; text: controller.engineHealth === 0 ? "NORMAL" : controller.engineHealth === 1 ? "CAUTION" : "WARNING"; color: root.healthColor; font.pixelSize: 25*root.uiScale; font.bold: true }
        Rectangle { x: 10*root.uiScale; y: 61*root.uiScale; width: parent.width-20*root.uiScale; height: 1; color: theme.border }
        Text { x: 10*root.uiScale; y: 73*root.uiScale; text: "ACTIVE MESSAGE"; color: theme.secondary; font.pixelSize: 7*root.uiScale; font.bold: true }
        Text { x: 10*root.uiScale; y: 91*root.uiScale; width: parent.width-20*root.uiScale; elide: Text.ElideRight; text: controller.engineDataState === "FAIL" ? "NO CHANNEL IS MEASURING" : controller.engineFaultMode === "NONE" ? "NO ACTIVE ENGINE ALERTS" : controller.engineFaultMode; color: root.healthColor; font.pixelSize: 11*root.uiScale; font.bold: true }
        Rectangle { x: 10*root.uiScale; y: 121*root.uiScale; width: parent.width-20*root.uiScale; height: 38*root.uiScale; color: "#9A0C1A24"; border.color: theme.borderBright
            Text { anchors.centerIn: parent; text: "SIMULATE NEXT FAULT"; color: theme.cyan; font.pixelSize: 8*root.uiScale; font.bold: true }
            MouseArea { anchors.fill: parent; onClicked: controller.cycleEngineFault() } }
    }

    Rectangle { id: cylinders; anchors { left: content.left; top: gaugesPanel.bottom; topMargin: 7*root.uiScale; bottom: content.bottom } width: 498*root.uiScale; radius: 4*root.uiScale; color: "#CA08131C"; border.color: theme.borderBright
        Text { x: 10*root.uiScale; y: 7*root.uiScale; text: "CYLINDER ANALYSIS • CHT / EGT"; color: theme.cyan; font.pixelSize: 8*root.uiScale; font.bold: true }
        Text { anchors { right: parent.right; rightMargin: 10*root.uiScale; top: parent.top; topMargin: 7*root.uiScale } text: "LEAN ASSIST READY"; color: theme.green; font.pixelSize: 7*root.uiScale; font.bold: true }
        Row { anchors { fill: parent; margins: 11*root.uiScale; topMargin: 34*root.uiScale } spacing: 8*root.uiScale
            Repeater { model: 4; delegate: Rectangle { required property int index; width: 111*root.uiScale; height: parent.height; radius: 3*root.uiScale; color: "#A9050D14"; border.color: "#304A58"
                    Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 5*root.uiScale } text: "CYL "+(index+1); color: theme.primary; font.pixelSize: 9*root.uiScale; font.bold: true }
                    Rectangle { x: 17*root.uiScale; y: 28*root.uiScale; width: 28*root.uiScale; height: parent.height-63*root.uiScale; color: "#152630"
                        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 3*root.uiScale } height: Math.max(3,Math.min(parent.height-6*root.uiScale,Number(controller.chtValues[index])/300*(parent.height-6*root.uiScale))); color: Number(controller.chtValues[index])>240 ? theme.amber : theme.green } }
                    Rectangle { x: 66*root.uiScale; y: 28*root.uiScale; width: 28*root.uiScale; height: parent.height-63*root.uiScale; color: "#152630"
                        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 3*root.uiScale } height: Math.max(3,Math.min(parent.height-6*root.uiScale,(Number(controller.egtValues[index])-500)/600*(parent.height-6*root.uiScale))); color: theme.cyan } }
                    Text { x: 8*root.uiScale; anchors.bottom: parent.bottom; anchors.bottomMargin: 6*root.uiScale; text: "CHT "+Math.round(controller.chtValues[index]); color: theme.green; font.pixelSize: 7*root.uiScale; font.bold: true }
                    Text { anchors { right: parent.right; rightMargin: 7*root.uiScale; bottom: parent.bottom; bottomMargin: 6*root.uiScale } text: "EGT "+Math.round(controller.egtValues[index]); color: theme.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
            } }
        }
    }

    Rectangle { id: fuel; anchors { left: cylinders.right; leftMargin: 7*root.uiScale; top: cylinders.top; bottom: content.bottom } width: 190*root.uiScale; radius: 4*root.uiScale; color: "#CA08131C"; border.color: theme.border
        Text { x: 10*root.uiScale; y: 7*root.uiScale; text: "FUEL SYSTEM"; color: theme.secondary; font.pixelSize: 8*root.uiScale; font.bold: true }
        Row { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 34*root.uiScale } spacing: 13*root.uiScale
            Repeater { model: [{n:"LEFT",v:controller.fuelLeft},{n:"RIGHT",v:controller.fuelRight}]; delegate: Item { required property var modelData; width: 68*root.uiScale; height: 125*root.uiScale
                    Rectangle { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top } width: 34*root.uiScale; height: 87*root.uiScale; radius: 4*root.uiScale; color: "#142731"; border.color: theme.borderBright
                        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 4*root.uiScale } height: Math.min(parent.height-8*root.uiScale,modelData.v/20*(parent.height-8*root.uiScale)); color: theme.green } }
                    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 17*root.uiScale } text: modelData.n; color: theme.secondary; font.pixelSize: 7*root.uiScale; font.bold: true }
                    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom } text: Number(modelData.v).toFixed(1); color: theme.primary; font.pixelSize: 13*root.uiScale; font.bold: true }
            } }
        }
        Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 37*root.uiScale } text: "TOTAL  "+(controller.fuelLeft+controller.fuelRight).toFixed(1)+" GAL"; color: theme.cyan; font.pixelSize: 11*root.uiScale; font.bold: true }
        Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 17*root.uiScale } text: "PRESS  "+controller.fuelPressure.toFixed(1)+" PSI"; color: controller.fuelPressure<2 ? theme.warning : theme.green; font.pixelSize: 8*root.uiScale; font.bold: true }
    }

    Rectangle { anchors { left: fuel.right; leftMargin: 7*root.uiScale; right: content.right; top: cylinders.top; bottom: content.bottom } radius: 4*root.uiScale; color: "#CA08131C"; border.color: theme.border
        Text { x: 10*root.uiScale; y: 7*root.uiScale; text: "ELECTRICAL / TREND"; color: theme.secondary; font.pixelSize: 8*root.uiScale; font.bold: true }
        Grid { x: 10*root.uiScale; y: 33*root.uiScale; columns: 2; columnSpacing: 22*root.uiScale; rowSpacing: 3*root.uiScale
            Text { text: "BUS"; color: theme.secondary; font.pixelSize: 7*root.uiScale } Text { text: "ALT"; color: theme.secondary; font.pixelSize: 7*root.uiScale }
            Text { text: controller.busVoltage.toFixed(1)+" V"; color: controller.busVoltage<12.5 ? theme.amber : theme.cyan; font.pixelSize: 17*root.uiScale; font.bold: true }
            Text { text: "+"+controller.alternatorCurrent.toFixed(0)+" A"; color: controller.alternatorCurrent<1 ? theme.amber : theme.cyan; font.pixelSize: 17*root.uiScale; font.bold: true }
        }
        Rectangle { x: 10*root.uiScale; y: 92*root.uiScale; width: parent.width-20*root.uiScale; height: 82*root.uiScale; color: "#0B1821"; border.color: theme.border
            Repeater { model: 4; delegate: Rectangle { required property int index; x: index*parent.width/4; width: 1; height: parent.height; color: "#243B47" } }
            Repeater { model: 3; delegate: Rectangle { required property int index; y: index*parent.height/3; width: parent.width; height: 1; color: "#243B47" } }
            Text { x: 5*root.uiScale; y: 4*root.uiScale; text: "ENGINE TREND • 60 S"; color: theme.secondary; font.pixelSize: 6*root.uiScale; font.bold: true }
            Rectangle { x: 7*root.uiScale; y: 51*root.uiScale; width: parent.width-14*root.uiScale; height: 3*root.uiScale; color: theme.green; rotation: -4 }
            Rectangle { x: 7*root.uiScale; y: 32*root.uiScale; width: parent.width-14*root.uiScale; height: 2*root.uiScale; color: theme.cyan; rotation: 3 }
        }
        Text { anchors { left: parent.left; leftMargin: 10*root.uiScale; bottom: parent.bottom; bottomMargin: 31*root.uiScale } text: "RECORDER"; color: theme.secondary; font.pixelSize: 7*root.uiScale; font.bold: true }
        Text { anchors { right: parent.right; rightMargin: 10*root.uiScale; bottom: parent.bottom; bottomMargin: 29*root.uiScale } text: controller.blackBoxState; color: theme.green; font.pixelSize: 10*root.uiScale; font.bold: true }
        Text { anchors { left: parent.left; leftMargin: 10*root.uiScale; bottom: parent.bottom; bottomMargin: 10*root.uiScale } text: "RECORDS  "+controller.blackBoxRecords; color: theme.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
    }
    Rectangle { id: footer; anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 34*root.uiScale; color: "#D008121B"; border.color: theme.borderBright
        Text { anchors.centerIn: parent; text: "EMS CORE V2 • DETERMINISTIC ENGINE SIMULATOR • DATA NON-CERTIFIED • NOT FOR FLIGHT"; color: theme.cyan; font.pixelSize: 9*root.uiScale; font.bold: true } }
}
