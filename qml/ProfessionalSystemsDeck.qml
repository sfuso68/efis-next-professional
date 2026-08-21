import QtQuick

Rectangle {
    id: deck
    required property var controller
    property real uiScale: 1
    property string engineDataState: "FAIL"
    Theme { id: theme }
    color: "#050D14"; border.color: theme.borderBright

    Row {
        anchors { fill: parent; margins: 5*deck.uiScale }
        spacing: 5*deck.uiScale

        Rectangle {
            width: 710*deck.uiScale; height: parent.height; color: "#D9071119"; border.color: theme.border; radius: 3*deck.uiScale
            Text { x: 8*deck.uiScale; y: 5*deck.uiScale; text: "ENGINE • " + deck.engineDataState; color: theme.secondary; font.pixelSize: 7*deck.uiScale; font.bold: true }
            Row { anchors { left: parent.left; right: temperatures.left; top: parent.top; bottom: parent.bottom; margins: 5*deck.uiScale; topMargin: 20*deck.uiScale } spacing: 3*deck.uiScale
                Repeater { model: [
                    {n:"RPM",v:controller.engineRpm,u:"RPM",min:0,max:3000,gs:800,ge:2600,d:0},
                    {n:"MAP",v:controller.manifoldPressure,u:"IN HG",min:10,max:35,gs:15,ge:29,d:1},
                    {n:"FLOW",v:controller.fuelFlow,u:"GPH",min:0,max:20,gs:4,ge:16,d:1},
                    // Stessa correzione di AdaptiveEmsView.qml, e per la stessa
                    // ragione: il valore e' in °C. Qui pesa di piu', perche'
                    // questo pannello sta sulla pagina di volo.
                    {n:"OIL T",v:controller.oilTemperature,u:"°C",min:20,max:130,gs:60,ge:105,d:0},
                    {n:"OIL P",v:controller.oilPressure,u:"PSI",min:0,max:100,gs:50,ge:90,d:0}
                ]; delegate: ProfessionalCircularGauge { required property var modelData; width: 106*deck.uiScale; height: parent.height;
                        uiScale: deck.uiScale*1.08; label: modelData.n; value: modelData.v; unit: modelData.u;
                        minimum: modelData.min; maximum: modelData.max; greenStart: modelData.gs; greenEnd: modelData.ge; decimals: modelData.d
                } }
            }
            Rectangle { id: temperatures; anchors { right: parent.right; top: parent.top; bottom: parent.bottom; margins: 5*deck.uiScale; topMargin: 20*deck.uiScale } width: 145*deck.uiScale; color: "#B008131C"; border.color: "#263E4B"
                Column { anchors { fill: parent; margins: 3*deck.uiScale } spacing: 2*deck.uiScale
                    Repeater { model: [
                            {n:"CHT",values:controller.chtValues,base:100,span:160,caution:220,warning:240},
                            {n:"EGT",values:controller.egtValues,base:500,span:450,caution:850,warning:920}
                        ]
                        delegate: Item { id: tempRow; required property var modelData; width: parent.width; height: (temperatures.height-10*deck.uiScale)/2
                            Text { x: 1*deck.uiScale; y: 3*deck.uiScale; text: modelData.n; color: modelData.n==="EGT"?theme.cyan:theme.green; font.pixelSize: 7*deck.uiScale; font.bold: true }
                            Row { anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom; leftMargin: 25*deck.uiScale } spacing: 4*deck.uiScale
                                Repeater { model: modelData.values; delegate: Item { required property var modelData; required property int index; width: 20*deck.uiScale; height: parent.height
                                        Rectangle { id: track; anchors { horizontalCenter: parent.horizontalCenter; bottom: cylinder.top; bottomMargin: 2*deck.uiScale } width: 13*deck.uiScale; height: 36*deck.uiScale; color: "#172832"; border.color: "#38505D"
                                            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Math.max(4,Math.min(parent.height,(Number(modelData)-tempRow.modelData.base)/tempRow.modelData.span*parent.height)); color: Number(modelData) >= tempRow.modelData.warning ? theme.warning : Number(modelData) >= tempRow.modelData.caution ? theme.amber : tempRow.modelData.n === "EGT" ? theme.cyan : theme.green
                                                Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                                            }
                                        }
                                        Text { id: cylinder; anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom } text: index+1; color: theme.primary; font.pixelSize: 6*deck.uiScale; font.bold: true }
                                } }
                            }
                        }
                    } }
            }
        }

        Rectangle { width: 90*deck.uiScale; height: parent.height; color: "#D9071119"; border.color: theme.border; radius: 3*deck.uiScale
            Text { x: 8*deck.uiScale; y: 5*deck.uiScale; text: "FUEL"; color: theme.secondary; font.pixelSize: 7*deck.uiScale; font.bold: true }
            Text { anchors.centerIn: parent; y: -9*deck.uiScale; text: (controller.fuelLeft+controller.fuelRight).toFixed(1); color: theme.primary; font.pixelSize: 20*deck.uiScale; font.bold: true }
            Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 9*deck.uiScale } text: "GAL USABLE • " + deck.engineDataState; color: theme.cyan; font.pixelSize: 7*deck.uiScale; font.bold: true }
            Rectangle { x: 10*deck.uiScale; y: 28*deck.uiScale; width: 11*deck.uiScale; height: 54*deck.uiScale; color: "#162630"; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: parent.height*.72; color: theme.green } }
            Rectangle { anchors { right: parent.right; rightMargin: 10*deck.uiScale } y: 28*deck.uiScale; width: 11*deck.uiScale; height: 54*deck.uiScale; color: "#162630"; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: parent.height*.72; color: theme.green } }
        }

        Rectangle { width: 72*deck.uiScale; height: parent.height; color: "#D9071119"; border.color: theme.border; radius: 3*deck.uiScale
            Text { x: 8*deck.uiScale; y: 5*deck.uiScale; text: "ELECTRICAL"; color: theme.secondary; font.pixelSize: 7*deck.uiScale; font.bold: true }
            Column { anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 8*deck.uiScale } spacing: 6*deck.uiScale
                Column { width: parent.width; spacing: 0; Text { text: "BUS"; color: theme.secondary; font.pixelSize: 6*deck.uiScale; font.bold:true } Text { id: busValue; text: controller.busVoltage.toFixed(1)+" V"; color: theme.cyan; font.pixelSize: 14*deck.uiScale; font.bold: true } }
                Rectangle { width: parent.width; height: 1; color: theme.border }
                Column { width: parent.width; spacing: 0; Text { text: "ALT"; color: theme.secondary; font.pixelSize: 6*deck.uiScale; font.bold:true } Text { id: altValue; text: "+"+Math.round(controller.alternatorCurrent)+" A"; color: theme.cyan; font.pixelSize: 14*deck.uiScale; font.bold: true } }
            }
        }

        Rectangle { width: 117*deck.uiScale; height: parent.height; color: "#D9071119"; border.color: theme.border; radius: 3*deck.uiScale
            Text { x: 8*deck.uiScale; y: 5*deck.uiScale; text: "AIRCRAFT SYSTEMS"; color: theme.secondary; font.pixelSize: 7*deck.uiScale; font.bold: true }
            Grid { anchors.centerIn: parent; columns: 2; columnSpacing: 14*deck.uiScale; rowSpacing: 5*deck.uiScale
                Repeater { model: [{n:"PITOT",v:"ON"},{n:"ICE",v:"OFF"},{n:"TRIM",v:"0.1"},{n:"FLAPS",v:"UP"},{n:"T/O",v:"OFF"},{n:"REC",v:controller.blackBoxState}]; delegate: Column { required property var modelData; spacing: 2*deck.uiScale
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.n; color: theme.secondary; font.pixelSize: 6*deck.uiScale; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.v; color: modelData.v==="OFF" ? theme.secondary : theme.green; font.pixelSize: 9*deck.uiScale; font.bold: true }
                } }
            }
        }
    }
}
