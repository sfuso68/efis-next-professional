import QtQuick

Item {
    id: root
    required property var controller
    property real uiScale: Math.min(width/1024, height/600)
    readonly property color healthColor: controller.engineHealth >= 2 ? theme.warning : controller.engineHealth === 1 ? theme.amber : theme.green
    Theme { id: theme }
    Rectangle { anchors.fill: parent; color: "#03090E" }

    CommandRail { id: rail; anchors { left: parent.left; right: parent.right; top: parent.top } height: 48*root.uiScale; uiScale: root.uiScale
        viewName: "EFIS NEXT  •  EMS PROFESSIONAL"; healthLevel: controller.engineHealth
        alertText: controller.engineFaultMode === "NONE" ? "ENGINE NORMAL" : controller.engineFaultMode; alertSeverity: controller.engineHealth
        flightDirectorActive: false; selectedAltitude: 0; dataSourceState: controller.dataSourceState; sensorNodeState: controller.sensorNodeState
        blackBoxState: controller.blackBoxState; blackBoxRecords: controller.blackBoxRecords }

    Rectangle { id: sideRail; anchors { left: parent.left; top: rail.bottom; bottom: footer.top } width: 54*root.uiScale; color: "#07131B"; border.color: "#27424F"
        Column { anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 10*root.uiScale } spacing: 8*root.uiScale
            Repeater { model: ["ENG","CYL","FUEL","ELEC","LOG"]; delegate: Rectangle { required property string modelData; width: 42*root.uiScale; height: 42*root.uiScale; radius: 4*root.uiScale
                color: modelData === "ENG" ? "#123746" : "#091B25"; border.color: modelData === "ENG" ? theme.cyan : "#294653"
                Text { anchors.centerIn: parent; text: modelData; color: modelData === "ENG" ? theme.cyan : "#9FB2BB"; font.pixelSize: 8*root.uiScale; font.bold: true } } }
        }
        Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 9*root.uiScale } text: "MENU"; color: theme.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
    }
    Item { id: content; anchors { left: sideRail.right; right: parent.right; top: rail.bottom; bottom: footer.top; margins: 7*root.uiScale } }

    Rectangle { id: instruments; anchors { left: content.left; right: cas.left; top: content.top; rightMargin: 7*root.uiScale } height: 230*root.uiScale; radius: 5*root.uiScale; color: "#C9071118"; border.color: "#31515F"
        Text { x: 10*root.uiScale; y: 6*root.uiScale; text: "PRIMARY ENGINE"; color: theme.cyan; font.pixelSize: 8*root.uiScale; font.bold: true }
        Row { anchors { fill: parent; leftMargin: 8*root.uiScale; rightMargin: 8*root.uiScale; topMargin: 22*root.uiScale; bottomMargin: 5*root.uiScale } spacing: 2*root.uiScale
            Repeater { model: [
                {n:"RPM",v:controller.engineRpm,u:"RPM",min:0,max:3000,gs:800,ge:2600,maj:500,minr:100,d:0,big:true},
                {n:"MANIFOLD",v:controller.manifoldPressure,u:"IN HG",min:10,max:35,gs:15,ge:29,maj:5,minr:1,d:1,big:true},
                {n:"OIL PRESS",v:controller.oilPressure,u:"PSI",min:0,max:100,gs:50,ge:90,maj:20,minr:5,d:0,big:false},
                {n:"OIL TEMP",v:controller.oilTemperature,u:"°C",min:20,max:130,gs:60,ge:105,maj:20,minr:5,d:0,big:false},
                {n:"FUEL FLOW",v:controller.fuelFlow,u:"GPH",min:0,max:20,gs:4,ge:16,maj:5,minr:1,d:1,big:false}]
                delegate: Item { required property var modelData; width: modelData.big ? 151*root.uiScale : 124*root.uiScale; height: parent.height
                    ProfessionalCircularGauge { anchors.fill: parent; uiScale: root.uiScale; label: modelData.n; value: modelData.v; unit: modelData.u; minimum: modelData.min; maximum: modelData.max
                        greenStart: modelData.gs; greenEnd: modelData.ge; majorStep: modelData.maj; minorStep: modelData.minr; decimals: modelData.d } } }
        }
    }

    Rectangle { id: cas; anchors { right: content.right; top: content.top } width: 231*root.uiScale; height: 230*root.uiScale; radius: 5*root.uiScale; color: "#C9071118"; border.color: root.healthColor
        Text { x: 9*root.uiScale; y: 7*root.uiScale; text: "CAS / AIRFRAME"; color: theme.secondary; font.pixelSize: 8*root.uiScale; font.bold: true }
        Rectangle { x: 9*root.uiScale; y: 26*root.uiScale; width: parent.width-18*root.uiScale; height: 34*root.uiScale; radius: 3*root.uiScale; color: "#0B1A21"; border.color: root.healthColor
            Text { anchors.centerIn: parent; text: controller.engineFaultMode === "NONE" ? "NO ACTIVE ALERTS" : controller.engineFaultMode; color: root.healthColor; font.pixelSize: 10*root.uiScale; font.bold: true } }
        Item { id: gear; visible: controller.landingGearInstalled && controller.retractableGear; x: 9*root.uiScale; y: 67*root.uiScale; width: parent.width-18*root.uiScale; height: 67*root.uiScale
            Text { text: "LANDING GEAR"; color: theme.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
            Row { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 16*root.uiScale } spacing: 18*root.uiScale
                Repeater { model: [{n:"NOSE",ok:controller.gearNoseLocked},{n:"LEFT",ok:controller.gearLeftLocked},{n:"RIGHT",ok:controller.gearRightLocked}]
                    delegate: Column { required property var modelData; spacing: 2*root.uiScale
                        Rectangle { width: 29*root.uiScale; height: 23*root.uiScale; radius: 12*root.uiScale; color: modelData.ok ? "#123B22" : "#491416"; border.width: 2; border.color: modelData.ok ? theme.green : theme.warning
                            Text { anchors.centerIn: parent; text: modelData.ok ? "▼" : "!"; color: modelData.ok ? theme.green : theme.warning; font.pixelSize: 13*root.uiScale; font.bold: true } }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.n; color: "#C8D4D9"; font.pixelSize: 6*root.uiScale; font.bold: true } } }
            }
            Text { anchors { right: parent.right; bottom: parent.bottom } text: controller.gearState; color: theme.green; font.pixelSize: 7*root.uiScale; font.bold: true }
        }
        Item { id: flaps; visible: controller.flapsInstalled; x: 9*root.uiScale; y: gear.visible ? 139*root.uiScale : 72*root.uiScale; width: parent.width-18*root.uiScale; height: 82*root.uiScale
            Text { text: "FLAPS"; color: theme.cyan; font.pixelSize: 7*root.uiScale; font.bold: true }
            Text { anchors { right: parent.right; top: parent.top; topMargin: -5*root.uiScale } text: Math.round(controller.flapAngle)+"°"; color: theme.primary; font.pixelSize: 25*root.uiScale; font.bold: true }
            Rectangle { x: 2*root.uiScale; y: 35*root.uiScale; width: parent.width-4*root.uiScale; height: 5*root.uiScale; radius: 2; color: "#29424D"
                Rectangle { width: parent.width*Math.min(1,controller.flapAngle/30); height: parent.height; radius: 2; color: theme.cyan }
                Rectangle { x: parent.width*Math.min(1,controller.flapCommandedAngle/30)-2*root.uiScale; y: -5*root.uiScale; width: 4*root.uiScale; height: 15*root.uiScale; color: "#FFFFFF" } }
            Repeater { model: controller.flapDetents; delegate: Item { required property real modelData; x: 2*root.uiScale+(parent.width-4*root.uiScale)*modelData/30; y: 43*root.uiScale
                Rectangle { width: 1; height: 8*root.uiScale; color: "#AFC2CB" }
                Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 9*root.uiScale } text: modelData+"°"; color: "#C8D4D9"; font.pixelSize: 6*root.uiScale; font.bold: true } } }
        }
    }

    Rectangle { id: cylinders; anchors { left: content.left; top: instruments.bottom; topMargin: 7*root.uiScale; bottom: content.bottom } width: 515*root.uiScale; radius: 5*root.uiScale; color: "#C9071118"; border.color: "#31515F"
        Text { x: 10*root.uiScale; y: 7*root.uiScale; text: "CYLINDER ANALYSIS  •  CHT / EGT"; color: theme.cyan; font.pixelSize: 8*root.uiScale; font.bold: true }
        Text { anchors { right: parent.right; top: parent.top; margins: 8*root.uiScale } text: "LEAN ASSIST READY"; color: theme.green; font.pixelSize: 7*root.uiScale; font.bold: true }
        Row { anchors { fill: parent; leftMargin: 10*root.uiScale; rightMargin: 10*root.uiScale; topMargin: 29*root.uiScale; bottomMargin: 9*root.uiScale } spacing: 7*root.uiScale
            Repeater { model: 4; delegate: Rectangle { required property int index; width: 117*root.uiScale; height: parent.height; radius: 3*root.uiScale; color: "#08151C"; border.color: "#294653"
                Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 4*root.uiScale } text: "CYL "+(index+1); color: theme.primary; font.pixelSize: 8*root.uiScale; font.bold: true }
                Row { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 22*root.uiScale; bottom: parent.bottom; bottomMargin: 24*root.uiScale } spacing: 13*root.uiScale
                    Repeater { model: [{n:"CHT",v:Number(controller.chtValues[index]),min:40,max:300,c:theme.green},{n:"EGT",v:Number(controller.egtValues[index]),min:400,max:1100,c:theme.cyan}]
                        delegate: Item { required property var modelData; width: 35*root.uiScale; height: parent.height
                            Rectangle { anchors.fill: parent; color: "#122630"; border.color: "#3A5865"
                                Repeater { model: 5; delegate: Rectangle { required property int index; y: index*parent.height/4; width: parent.width; height: 1; color: "#415B66" } }
                                Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 3*root.uiScale } height: Math.max(3,(modelData.v-modelData.min)/(modelData.max-modelData.min)*(parent.height-6*root.uiScale)); color: modelData.c } }
                            Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.bottom; topMargin: 2*root.uiScale } text: modelData.n+" "+Math.round(modelData.v); color: modelData.c; font.pixelSize: 6*root.uiScale; font.bold: true } } }
                } } }
        }
    }

    Rectangle { anchors { left: cylinders.right; leftMargin: 7*root.uiScale; right: content.right; top: cylinders.top; bottom: content.bottom } radius: 5*root.uiScale; color: "#C9071118"; border.color: "#31515F"
        Text { x: 10*root.uiScale; y: 7*root.uiScale; text: "FUEL / ELECTRICAL / RECORDING"; color: theme.cyan; font.pixelSize: 8*root.uiScale; font.bold: true }
        Grid { x: 12*root.uiScale; y: 33*root.uiScale; columns: 4; columnSpacing: 25*root.uiScale; rowSpacing: 4*root.uiScale
            Repeater { model: ["FUEL L","FUEL R","BUS","ALT"]; delegate: Text { required property string modelData; text: modelData; color: theme.secondary; font.pixelSize: 7*root.uiScale; font.bold: true } }
            Repeater { model: [controller.fuelLeft.toFixed(1)+" GAL",controller.fuelRight.toFixed(1)+" GAL",controller.busVoltage.toFixed(1)+" V","+"+controller.alternatorCurrent.toFixed(0)+" A"]; delegate: Text { required property string modelData; text: modelData; color: theme.primary; font.pixelSize: 13*root.uiScale; font.bold: true } } }
        Rectangle { x: 12*root.uiScale; y: 87*root.uiScale; width: parent.width-24*root.uiScale; height: 58*root.uiScale; color: "#08151C"; border.color: "#294653"
            Repeater { model: 4; delegate: Rectangle { required property int index; x: index*parent.width/4; width: 1; height: parent.height; color: "#263F4A" } }
            Rectangle { x: 5*root.uiScale; y: 34*root.uiScale; width: parent.width-10*root.uiScale; height: 3*root.uiScale; color: theme.green; rotation: -3 }
            Rectangle { x: 5*root.uiScale; y: 20*root.uiScale; width: parent.width-10*root.uiScale; height: 2*root.uiScale; color: theme.cyan; rotation: 2 }
            Text { x: 5*root.uiScale; y: 3*root.uiScale; text: "ENGINE TREND  •  60 S"; color: theme.secondary; font.pixelSize: 6*root.uiScale; font.bold: true } }
        Text { x: 12*root.uiScale; y: 158*root.uiScale; text: "BLACKBOX  "+controller.blackBoxState+"  •  "+controller.blackBoxRecords+" RECORDS"; color: controller.blackBoxState === "RECORDING" ? theme.green : theme.amber; font.pixelSize: 8*root.uiScale; font.bold: true }
        Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12*root.uiScale } height: 34*root.uiScale; radius: 3*root.uiScale; color: "#0C202A"; border.color: "#31515F"
            Text { anchors.centerIn: parent; text: "FAULT TEST  •  PRESS TO ADVANCE"; color: theme.cyan; font.pixelSize: 8*root.uiScale; font.bold: true }
            MouseArea { anchors.fill: parent; onClicked: controller.cycleEngineFault() } }
    }
    Rectangle { id: footer; anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 32*root.uiScale; color: "#07131B"; border.color: "#27424F"
        Text { anchors.centerIn: parent; text: "EMS CORE V2  •  PROFILE-CONTROLLED AIRFRAME  •  SIMULATION / NON-CERTIFIED"; color: theme.cyan; font.pixelSize: 8*root.uiScale; font.bold: true } }
}
