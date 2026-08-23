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
            width: 770*deck.uiScale; height: parent.height; color: "#D9071119"; border.color: theme.border; radius: 3*deck.uiScale
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
                ]; delegate: EmsGaugeV3 { required property var modelData; width: 116*deck.uiScale; height: parent.height;
                        uiScale: deck.uiScale*.64; label: modelData.n; value: modelData.v; unit: modelData.u;
                        compact:true;
                        minimum: modelData.min; maximum: modelData.max; greenStart: modelData.gs; greenEnd: modelData.ge; decimals: modelData.d;
                        majorStep:(modelData.max-modelData.min)/5; minorStep:(modelData.max-modelData.min)/25
                } }
            }
            EngineTemperatureBars { id: temperatures; anchors { right:parent.right; top:parent.top; bottom:parent.bottom; margins:5*deck.uiScale; topMargin:20*deck.uiScale } width:155*deck.uiScale; uiScale:deck.uiScale; compact:true; chtValues:controller.chtValues; egtValues:controller.egtValues }
        }

        Rectangle { width: 68*deck.uiScale; height: parent.height; color: "#D9071119"; border.color: theme.border; radius: 3*deck.uiScale
            Text { x: 8*deck.uiScale; y: 5*deck.uiScale; text: "FUEL"; color: theme.secondary; font.pixelSize: 7*deck.uiScale; font.bold: true }
            Text { anchors.centerIn: parent; y: -9*deck.uiScale; text: (controller.fuelLeft+controller.fuelRight).toFixed(1); color: theme.primary; font.pixelSize: 20*deck.uiScale; font.bold: true }
            Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 9*deck.uiScale } text: "GAL USABLE • " + deck.engineDataState; color: theme.cyan; font.pixelSize: 7*deck.uiScale; font.bold: true }
            Rectangle { x: 10*deck.uiScale; y: 28*deck.uiScale; width: 11*deck.uiScale; height: 54*deck.uiScale; color: "#162630"; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: parent.height*.72; color: theme.green } }
            Rectangle { anchors { right: parent.right; rightMargin: 10*deck.uiScale } y: 28*deck.uiScale; width: 11*deck.uiScale; height: 54*deck.uiScale; color: "#162630"; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: parent.height*.72; color: theme.green } }
        }

        Rectangle { width: 58*deck.uiScale; height: parent.height; color: "#D9071119"; border.color: theme.border; radius: 3*deck.uiScale
            Text { x: 8*deck.uiScale; y: 5*deck.uiScale; text: "ELECTRICAL"; color: theme.secondary; font.pixelSize: 7*deck.uiScale; font.bold: true }
            Column { anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 8*deck.uiScale } spacing: 6*deck.uiScale
                Column { width: parent.width; spacing: 0; Text { text: "BUS"; color: theme.secondary; font.pixelSize: 6*deck.uiScale; font.bold:true } Text { id: busValue; text: controller.busVoltage.toFixed(1)+" V"; color: theme.cyan; font.pixelSize: 14*deck.uiScale; font.bold: true } }
                Rectangle { width: parent.width; height: 1; color: theme.border }
                Column { width: parent.width; spacing: 0; Text { text: "ALT"; color: theme.secondary; font.pixelSize: 6*deck.uiScale; font.bold:true } Text { id: altValue; text: "+"+Math.round(controller.alternatorCurrent)+" A"; color: theme.cyan; font.pixelSize: 14*deck.uiScale; font.bold: true } }
            }
        }

        Rectangle { width: 98*deck.uiScale; height: parent.height; color: "#D9071119"; border.color: theme.border; radius: 3*deck.uiScale
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
