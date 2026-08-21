import QtQuick

Rectangle {
    id: root
    required property var controller
    property real uiScale: Math.min(width/1024, height/600)
    property string source: Qt.application.arguments.indexOf("--nav-vor-preview") >= 0 ? "VOR" : "GPS"
    property string receiver: "NAV1"
    property string nav1Active: "---.--"
    property string nav1Standby: "---.--"
    property string nav2Active: "---.--"
    property string nav2Standby: "---.--"
    readonly property var airport: controller.selectedAirport || ({})
    readonly property string ident: airport.icao || "----"
    readonly property var localWx: weather.airportWeather.length ? weather.airportWeather[0] : ({})
    readonly property var activeSigmet: weather.sigmets.length > 0 ? weather.sigmets[0] : null
    readonly property var primaryAlternate: weather.alternates.length > 0 ? weather.alternates[0] : null
    readonly property var secondaryAlternate: weather.alternates.length > 1 ? weather.alternates[1] : null
    property bool showTaf: false
    property bool weatherDetailVisible: Qt.application.arguments.indexOf("--weather-detail-preview") >= 0
    Component.onCompleted: {
        if (Qt.application.arguments.indexOf("--weather-cappi-preview") >= 0) { weather.cycleProduct(); weather.cycleProduct() }
        if (Qt.application.arguments.indexOf("--weather-tsra-preview") >= 0) { weather.nextScenario(); weather.nextScenario(); weather.nextScenario(); weather.nextScenario(); weatherDetailVisible=true }
        if (Qt.application.arguments.indexOf("--weather-route-preview") >= 0) { weather.nextScenario(); weather.nextScenario(); weather.nextScenario(); weather.nextScenario(); weatherDetailVisible=false }
    }
    Timer { interval: 6000; running: visible; repeat: true; onTriggered: root.showTaf = !root.showTaf }
    Theme { id: theme }
    color: theme.background

    CommandRail { id: rail; anchors { left: parent.left; right: parent.right; top: parent.top } height: 54*root.uiScale; uiScale: root.uiScale; viewName: "NAV CONTROL CENTER"; healthLevel: controller.healthLevel; alertText: controller.annunciation; alertSeverity: controller.annunciationSeverity; flightDirectorActive: controller.flightDirectorActive; selectedAltitude: controller.selectedAltitude; dataSourceState: controller.dataSourceState; sensorNodeState: controller.sensorNodeState; blackBoxState: controller.blackBoxState; blackBoxRecords: controller.blackBoxRecords }

    Rectangle { id: sources; anchors { left: parent.left; top: rail.bottom; bottom: parent.bottom; margins: 10*root.uiScale } width: 130*root.uiScale; color: "#09151E"; border.color: theme.borderBright; radius: 4*root.uiScale
        Text { x: 12*root.uiScale; y: 12*root.uiScale; text: "NAV SOURCE"; color: theme.secondary; font.pixelSize: 8*root.uiScale; font.bold: true }
        Column { x: 10*root.uiScale; y: 38*root.uiScale; spacing: 8*root.uiScale
            Repeater { model: [{n:"GPS",s:"ACTIVE"},{n:"VOR",s:"STANDBY"},{n:"NDB",s:"STANDBY"}]; delegate: Rectangle { required property var modelData; width: 110*root.uiScale; height: 58*root.uiScale; radius: 3*root.uiScale; color: root.source===modelData.n ? "#123847" : "#0C1C26"; border.color: root.source===modelData.n ? theme.cyan : theme.border
                    Text { x: 10*root.uiScale; y: 8*root.uiScale; text: modelData.n; color: root.source===modelData.n ? theme.cyan : theme.primary; font.pixelSize: 15*root.uiScale; font.bold: true }
                    Text { x: 10*root.uiScale; y: 34*root.uiScale; text: modelData.s; color: root.source===modelData.n ? theme.green : theme.secondary; font.pixelSize: 7*root.uiScale; font.bold: true }
                    TapHandler { onTapped: root.source=modelData.n }
            } }
        }
        Row { visible: root.source!=="GPS"; x: 10*root.uiScale; y: 226*root.uiScale; spacing: 4*root.uiScale
            Repeater { model: ["NAV1","NAV2"]; delegate: Rectangle { required property string modelData; width: 53*root.uiScale; height: 35*root.uiScale; radius: 3*root.uiScale; color: root.receiver===modelData?"#153947":"#0C1C26"; border.color: root.receiver===modelData?theme.cyan:theme.border
                    Text { anchors.centerIn: parent; text: modelData; color: root.receiver===modelData?theme.cyan:theme.secondary; font.pixelSize: 8*root.uiScale; font.bold: true }
                    TapHandler { onTapped: root.receiver=modelData }
            } }
        }
        Rectangle { x: 10*root.uiScale; anchors.bottom: parent.bottom; anchors.bottomMargin: 12*root.uiScale; width: 110*root.uiScale; height: 92*root.uiScale; color: "#0C1C26"; border.color: theme.border
            Text { x: 9*root.uiScale; y: 8*root.uiScale; text: "RECEIVER"; color: theme.secondary; font.pixelSize: 7*root.uiScale; font.bold: true }
            Text { x: 9*root.uiScale; y: 29*root.uiScale; text: root.source==="GPS" ? "GPS / M10" : root.receiver+"\nNO FACILITY"; color: root.source==="GPS" ? theme.green : theme.amber; font.pixelSize: 10*root.uiScale; font.bold: true }
            Text { x: 9*root.uiScale; anchors.bottom: parent.bottom; anchors.bottomMargin: 8*root.uiScale; text: root.source==="GPS" ? (controller.navigationValid?"NAV VALID":"NAV INVALID") : "SELECT / TUNE"; color: controller.navigationValid&&root.source==="GPS" ? theme.cyan : theme.secondary; font.pixelSize: 7*root.uiScale; font.bold: true }
        }
    }

    Rectangle { id: hsiPanel; anchors { left: sources.right; right: dataPanel.left; top: rail.bottom; bottom: parent.bottom; margins: 10*root.uiScale } color: "#07121A"; border.color: theme.borderBright; radius: 4*root.uiScale
        Image { anchors.fill: parent; anchors.margins: 2*root.uiScale; source: controller.imageryAtlasSource; fillMode: Image.PreserveAspectCrop; opacity: controller.imageryOfflineReady ? 0.48 : 0; asynchronous: true; cache: true }
        Rectangle { anchors.fill: parent; color: "#76071118" }
        WeatherRadarOverlay { anchors.fill: parent; anchors.margins: 2*root.uiScale; uiScale: root.uiScale; radarOpacity: 0.34; badgeTopMargin: 31; z: 0 }
        Text { x: 14*root.uiScale; y: 10*root.uiScale; text: root.source+" • HSI"; color: theme.cyan; font.pixelSize: 9*root.uiScale; font.bold: true }
        Rectangle { z:3; anchors { right:parent.right; top:parent.top; margins:10*root.uiScale } width:190*root.uiScale; height:66*root.uiScale; radius:3*root.uiScale; color:"#E907121A"; border.color:theme.borderBright
            Text { x:7*root.uiScale; y:5*root.uiScale; text:(weather.simulation?"SIM WX • ":"")+(root.showTaf?"TAF • ":"METAR • ")+(root.localWx.icao || "LIPH"); color:weather.simulation?theme.amber:theme.cyan; font.pixelSize:7*root.uiScale; font.bold:true }
            Text { x:7*root.uiScale; y:20*root.uiScale; width:176*root.uiScale; height:28*root.uiScale; text:(root.showTaf?root.localWx.taf:root.localWx.metar) || "NO AVIATION WEATHER DATA"; color:theme.primary; font.pixelSize:6*root.uiScale; wrapMode:Text.Wrap; elide:Text.ElideRight; maximumLineCount:2 }
            Text { x:7*root.uiScale; y:51*root.uiScale; text:(root.localWx.category || "—")+" • "+(root.showTaf?"FORECAST":"OBS")+" • TRAINING"; color:root.localWx.category === "VFR" ? theme.green : theme.amber; font.pixelSize:6*root.uiScale; font.bold:true }
            TapHandler { onTapped:root.weatherDetailVisible=true }
        }
        Item { id: rose; z: 2; anchors.centerIn: parent; width: Math.min(parent.width,parent.height)*.82; height: width
            Repeater { model: 72; delegate: Item { required property int index; anchors.fill: parent; rotation: index*5-controller.heading
                    Rectangle { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top } width: (index%6===0?2:1)*root.uiScale; height: (index%6===0?13:index%2===0?8:4)*root.uiScale; color: index%6===0?theme.primary:"#8098A4" }
                    Text { visible: index%6===0; anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 15*root.uiScale } rotation: controller.heading-index*5; text: index===0?"N":index===18?"E":index===36?"S":index===54?"W":String(index/2); color: theme.primary; font.pixelSize: 9*root.uiScale; font.bold: true }
            } }
            Rectangle { anchors.fill: parent; radius: width/2; color: "transparent"; border.color: "#8DA8B4"; border.width: 1 }
            Item { anchors.centerIn: parent; width: parent.width*.72; height: parent.height*.72; rotation: controller.selectedCourse-controller.heading
                Rectangle { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top } width: 3*root.uiScale; height: parent.height*.43; color: theme.magenta }
                Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: -12*root.uiScale } text: "▲"; color: theme.magenta; font.pixelSize: 14*root.uiScale; font.bold: true }
                Rectangle { anchors.centerIn: parent; width: 3*root.uiScale; height: parent.height*.22; x: controller.navigationValid?0:10*root.uiScale; color: theme.cyan }
                Rectangle { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom } width: 3*root.uiScale; height: parent.height*.33; color: theme.magenta }
            }
            Text { anchors.centerIn: parent; text: "✈"; color: theme.primary; font.pixelSize: 24*root.uiScale; font.bold: true }
            Rectangle { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: -18*root.uiScale } width: 72*root.uiScale; height: 30*root.uiScale; radius: 3*root.uiScale; color: "#E5060C11"; border.color: theme.primary
                Text { anchors.centerIn: parent; text: ("000"+Math.round(controller.heading)).slice(-3)+"°"; color: theme.primary; font.pixelSize: 16*root.uiScale; font.bold: true }
            }
        }
        Rectangle { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 12*root.uiScale } width: parent.width-28*root.uiScale; height: 52*root.uiScale; color: "#0B1B25"; border.color: theme.border
            Row { anchors.centerIn: parent; spacing: 30*root.uiScale
                Repeater { model: [{k:"CRS",v:Math.round(controller.selectedCourse)+"°"},{k:"BRG",v:Math.round(controller.selectedCourse)+"°"},{k:"GS",v:Math.round(controller.groundSpeed)+" KT"},{k:"XTK",v:controller.navigationValid?"0.0 NM":"---"}]; delegate: Column { required property var modelData; spacing: 3*root.uiScale
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.k; color: theme.secondary; font.pixelSize: 7*root.uiScale; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.v; color: theme.primary; font.pixelSize: 12*root.uiScale; font.bold: true }
                } }
            }
        }
        Rectangle { z:4; visible:weather.simulation; anchors { left:parent.left; bottom:parent.bottom; leftMargin:14*root.uiScale; bottomMargin:68*root.uiScale } width:390*root.uiScale; height:34*root.uiScale; radius:3*root.uiScale; color:"#E9071118"; border.color:theme.amber
            Row { anchors { fill:parent; margins:3*root.uiScale }
                spacing:4*root.uiScale
                Rectangle { width:105*root.uiScale; height:28*root.uiScale; color:"#17252D"; border.color:theme.amber; Text{anchors.centerIn:parent;text:weather.radarProduct;color:theme.amber;font.pixelSize:6*root.uiScale;font.bold:true} TapHandler{onTapped:weather.cycleProduct()} }
                Rectangle { width:64*root.uiScale; height:28*root.uiScale; color:"#12313D"; border.color:theme.cyan; Text{anchors.centerIn:parent;text:weather.animationRunning?"❚❚ PAUSE":"▶ PLAY";color:theme.cyan;font.pixelSize:6*root.uiScale;font.bold:true} TapHandler{onTapped:weather.toggleAnimation()} }
                Rectangle { width:76*root.uiScale; height:28*root.uiScale; color:"#17252D"; border.color:weather.routeRisk==="HIGH"?theme.warning:theme.borderBright; Text{anchors.centerIn:parent;text:weather.scenario;color:weather.routeRisk==="HIGH"?theme.warning:theme.primary;font.pixelSize:7*root.uiScale;font.bold:true} TapHandler{onTapped:weather.nextScenario()} }
                Text { anchors.verticalCenter:parent.verticalCenter; text:"ϟ3  T-"+(weather.simulationPhase%12)*3+"s  →18KT"; color:theme.cyan; font.pixelSize:6*root.uiScale; font.bold:true }
            }
        }
        Rectangle { z:4; visible:weather.simulation; anchors { right:parent.right; bottom:parent.bottom; rightMargin:14*root.uiScale; bottomMargin:68*root.uiScale } width:140*root.uiScale; height:34*root.uiScale; radius:3*root.uiScale; color:weather.routeRisk==="HIGH"?"#D52A0D12":"#E9071118"; border.color:weather.routeRisk==="NONE"?theme.green:weather.routeRisk==="CAUTION"?theme.amber:theme.warning
            Text { anchors.centerIn:parent; text:"ROUTE WX • "+weather.routeRisk; color:weather.routeRisk==="NONE"?theme.green:weather.routeRisk==="CAUTION"?theme.amber:theme.warning; font.pixelSize:7*root.uiScale; font.bold:true }
        }
        Rectangle { z:6; visible:weather.simulation && !weather.alertAcknowledged; anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:78*root.uiScale } width:310*root.uiScale; height:48*root.uiScale; radius:3*root.uiScale; color:"#EE3B0B12"; border.color:theme.warning
            Text { x:10*root.uiScale; y:7*root.uiScale; text:weather.advisory+" • HDG "+weather.suggestedHeading+"°"; color:theme.warning; font.pixelSize:8*root.uiScale; font.bold:true }
            Text { x:10*root.uiScale; y:28*root.uiScale; text:(weather.audioEnabled?"VOICE ARMED":"VOICE MUTED")+" • TAP ACK"; color:theme.primary; font.pixelSize:6*root.uiScale; font.bold:true }
            TapHandler { onTapped:weather.acknowledgeAlert() }
        }
    }

    Rectangle { id: dataPanel; anchors { right: parent.right; top: rail.bottom; bottom: parent.bottom; margins: 10*root.uiScale } width: 250*root.uiScale; color: "#09151E"; border.color: theme.borderBright; radius: 4*root.uiScale
        Column { anchors { fill: parent; margins: 12*root.uiScale } spacing: 9*root.uiScale
            Text { text: "ACTIVE DESTINATION"; color: theme.secondary; font.pixelSize: 8*root.uiScale; font.bold: true }
            Text { text: root.source==="GPS"?root.ident:"NO FACILITY"; color: root.source==="GPS"?theme.magenta:theme.amber; font.pixelSize: 24*root.uiScale; font.bold: true }
            Text { width: parent.width; text: root.source==="GPS"?(root.airport.name || "Selected airport"):"Tune or select a radio-navigation facility"; color: theme.primary; font.pixelSize: 9*root.uiScale; wrapMode: Text.WordWrap }
            Rectangle { width: parent.width; height: 1; color: theme.border }
            Repeater { model: [{k:"POSITION",v:controller.latitude.toFixed(4)+"  "+controller.longitude.toFixed(4)},{k:"DESIRED TRACK",v:Math.round(controller.selectedCourse)+"°"},{k:"GROUND SPEED",v:Math.round(controller.groundSpeed)+" KT"},{k:"INTEGRITY",v:root.source==="GPS"?(controller.navigationValid?"VALID • SIM":"INVALID"):"NOT TUNED"}]; delegate: Rectangle { required property var modelData; width: parent.width; height: 52*root.uiScale; color: "#0C1C26"; border.color: theme.border
                    Text { x: 9*root.uiScale; y: 7*root.uiScale; text: modelData.k; color: theme.secondary; font.pixelSize: 7*root.uiScale; font.bold: true }
                    Text { x: 9*root.uiScale; y: 25*root.uiScale; text: modelData.v; color: modelData.k==="INTEGRITY"?(controller.navigationValid&&root.source==="GPS"?theme.green:theme.amber):theme.primary; font.pixelSize: 10*root.uiScale; font.bold: true }
            } }
            Rectangle { width: parent.width; height: 112*root.uiScale; color: "#121A20"; border.color: theme.amber
                Text { x: 9*root.uiScale; y: 7*root.uiScale; text: root.source==="GPS"?"RADIONAV DATABASE":root.receiver+" • ACTIVE / STBY"; color: theme.amber; font.pixelSize: 8*root.uiScale; font.bold: true }
                Text { visible: root.source==="GPS"; x: 9*root.uiScale; y: 27*root.uiScale; width: parent.width-18*root.uiScale; text: "VOR/NDB frequencies require a validated navigation dataset. Current page is simulation-only."; color: theme.primary; font.pixelSize: 8*root.uiScale; wrapMode: Text.WordWrap }
                Row { visible: root.source!=="GPS"; x: 9*root.uiScale; y: 29*root.uiScale; spacing: 8*root.uiScale
                    Column { spacing: 2*root.uiScale; Text { text:"ACTIVE"; color:theme.secondary; font.pixelSize:6*root.uiScale; font.bold:true } Text { text:root.receiver==="NAV1"?root.nav1Active:root.nav2Active; color:theme.green; font.pixelSize:15*root.uiScale; font.bold:true } }
                    Rectangle { width: 36*root.uiScale; height: 34*root.uiScale; radius:3*root.uiScale; color:"#14313D"; border.color:theme.cyan
                        Text { anchors.centerIn:parent; text:"↔"; color:theme.cyan; font.pixelSize:15*root.uiScale; font.bold:true }
                        TapHandler { onTapped: { if(root.receiver==="NAV1"){const value=root.nav1Active;root.nav1Active=root.nav1Standby;root.nav1Standby=value}else{const value=root.nav2Active;root.nav2Active=root.nav2Standby;root.nav2Standby=value} } }
                    }
                    Column { spacing: 2*root.uiScale; Text { text:"STBY"; color:theme.secondary; font.pixelSize:6*root.uiScale; font.bold:true } Text { text:root.receiver==="NAV1"?root.nav1Standby:root.nav2Standby; color:theme.cyan; font.pixelSize:15*root.uiScale; font.bold:true } }
                }
                Text { visible: root.source!=="GPS"; x:9*root.uiScale; anchors.bottom:parent.bottom; anchors.bottomMargin:8*root.uiScale; text:controller.radionavigationDatabaseStatus+" • "+controller.radionavigationFacilities.length+" FAC • "+controller.radionavigationDatabaseVersion; color:controller.radionavigationDatabaseStatus==="OPERATIONAL"?theme.green:theme.amber; font.pixelSize:6*root.uiScale; font.bold:true }
            }
        }
    }
    Rectangle { visible:root.weatherDetailVisible; z:20; anchors { left:sources.right; right:dataPanel.left; top:rail.bottom; bottom:parent.bottom; margins:10*root.uiScale } color:"#FC061118"; border.color:theme.cyan; radius:4*root.uiScale
        Text { x:18*root.uiScale; y:15*root.uiScale; text:"AVIATION WEATHER • "+(weather.simulation?"TRAINING SIMULATION":"SUPPLEMENTARY"); color:weather.simulation?theme.amber:theme.cyan; font.pixelSize:15*root.uiScale; font.bold:true }
        Repeater { model:weather.airportWeather; delegate:Rectangle { required property var modelData; required property int index; x:18*root.uiScale; y:(58+index*175)*root.uiScale; width:parent.width-36*root.uiScale; height:160*root.uiScale; color:"#0B1B25"; border.color:theme.borderBright
                Text{x:12*root.uiScale;y:10*root.uiScale;text:modelData.icao+" • "+modelData.category;color:modelData.category==="VFR"?theme.green:theme.amber;font.pixelSize:13*root.uiScale;font.bold:true}
                Text{x:12*root.uiScale;y:39*root.uiScale;width:parent.width-24*root.uiScale;text:"METAR  "+modelData.metar;color:theme.primary;font.pixelSize:8*root.uiScale;wrapMode:Text.WordWrap}
                Text{x:12*root.uiScale;y:88*root.uiScale;width:parent.width-24*root.uiScale;text:"TAF  "+modelData.taf;color:theme.cyan;font.pixelSize:8*root.uiScale;wrapMode:Text.WordWrap}
            } }
        Rectangle { visible:root.activeSigmet !== null; x:18*root.uiScale; anchors.bottom:parent.bottom; anchors.bottomMargin:16*root.uiScale; width:(parent.width-42*root.uiScale)*.62; height:55*root.uiScale; color:"#391017"; border.color:theme.warning
            Text{x:12*root.uiScale;y:8*root.uiScale;text:root.activeSigmet ? "SIM SIGMET • "+root.activeSigmet.phenomenon+" • "+root.activeSigmet.area : "";color:theme.warning;font.pixelSize:9*root.uiScale;font.bold:true}
            Text{x:12*root.uiScale;y:31*root.uiScale;text:root.activeSigmet ? root.activeSigmet.valid+" • ROUTE RISK "+weather.routeRisk+" • BBX "+weather.weatherEventCount : "";color:theme.primary;font.pixelSize:7*root.uiScale;font.bold:true}
        }
        Rectangle { visible:root.primaryAlternate !== null; anchors { right:parent.right; rightMargin:18*root.uiScale; bottom:parent.bottom; bottomMargin:16*root.uiScale } width:(parent.width-42*root.uiScale)*.36; height:55*root.uiScale; color:"#0B1B25"; border.color:theme.cyan
            Text{x:9*root.uiScale;y:6*root.uiScale;text:"WX ALTERNATES";color:theme.cyan;font.pixelSize:7*root.uiScale;font.bold:true}
            Text{x:9*root.uiScale;y:22*root.uiScale;text:root.primaryAlternate ? root.primaryAlternate.icao+" "+root.primaryAlternate.distance+" "+root.primaryAlternate.category : "";color:root.primaryAlternate && root.primaryAlternate.category==="VFR"?theme.green:theme.amber;font.pixelSize:7*root.uiScale;font.bold:true}
            Text{x:9*root.uiScale;y:38*root.uiScale;text:root.secondaryAlternate ? root.secondaryAlternate.icao+" "+root.secondaryAlternate.distance+" "+root.secondaryAlternate.category : "";color:theme.green;font.pixelSize:7*root.uiScale;font.bold:true}
        }
        Rectangle { anchors { right:parent.right; top:parent.top; margins:14*root.uiScale } width:90*root.uiScale; height:36*root.uiScale; color:"#162633"; border.color:theme.borderBright; Text{anchors.centerIn:parent;text:"BACK";color:theme.primary;font.pixelSize:9*root.uiScale;font.bold:true} TapHandler{onTapped:root.weatherDetailVisible=false} }
        Rectangle { anchors { right:parent.right; top:parent.top; rightMargin:112*root.uiScale; topMargin:14*root.uiScale } width:92*root.uiScale; height:36*root.uiScale; color:"#162633"; border.color:weather.audioEnabled?theme.cyan:theme.amber; Text{anchors.centerIn:parent;text:weather.audioEnabled?"VOICE ON":"VOICE MUTE";color:weather.audioEnabled?theme.cyan:theme.amber;font.pixelSize:8*root.uiScale;font.bold:true} TapHandler{onTapped:weather.toggleAudio()} }
    }
}
