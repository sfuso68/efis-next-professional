import QtQuick
import QtQuick.Controls

Rectangle {
    id: overlay
    required property var controller
    property real uiScale: 1
    property int sectionIndex: 0
    property bool catalogVisible: false
    property bool reviewVisible: false
    property bool verificationVisible: false
    property bool engineReviewVisible: false
    property bool sensorMappingVisible: false
    property bool channelTestVisible: false
    property bool airportSelectorVisible: false
    property bool airportNearestOnly: false
    property bool connectivityVisible: false
    property bool benchVisible: false
    property string airportQuery: ""
    function visibleAirports() {
        const source=airportNearestOnly ? controller.nearestAirports : controller.airportCatalog
        const query=airportQuery.trim().toLowerCase()
        if (!query.length) return source
        return source.filter(function(a) { return String(a.icao+" "+a.name+" "+a.city).toLowerCase().indexOf(query)>=0 })
    }
    property string catalogMode: "aircraft"
    property string stagedSelection: ""
    signal closeRequested()
    Theme { id: theme }

    readonly property var sections: ["PLATFORM","DISPLAY","AIRCRAFT","SENSORS","NETWORK / CAN","NAV / RADIO","ENGINE","FPV / VIDEO","ALERTS / AUDIO","BLACK BOX","DIAGNOSTICS"]

    function items(index) {
        switch(index) {
        case 0: return [
            {t:"COMPUTE NODE",v:"RPi5 • PFD-01",s:"ACTIVE",c:theme.green}, {t:"SYSTEM PROFILE",v:"PROFESSIONAL",s:"STANDARD READY",c:theme.cyan},
            {t:"CORE",v:"EFIS CORE V2",s:"SIMULATION",c:theme.amber}, {t:"ROLE",v:"PRIMARY DISPLAY",s:"REDUNDANCY READY",c:theme.cyan},
            {t:"TIME SOURCE",v:"SYSTEM / GPS",s:"UTC",c:theme.primary}, {t:"SOFTWARE",v:"EFIS NEXT",s:"LOCAL BUILD",c:theme.primary}]
        case 1: return [
            {t:"DISPLAY",v:"MPI7002",s:"1024 × 600 @ 60",c:theme.green}, {t:"TOUCH",v:"QDTECH MPI7003",s:"CONNECTED",c:theme.green},
            {t:"BRIGHTNESS",v:"80 %",s:"AUTO READY",c:theme.cyan}, {t:"UI SCALE",v:"1.00",s:"ADAPTIVE",c:theme.cyan},
            {t:"THEME",v:"PRO NIGHT",s:"DAY / NIGHT",c:theme.primary}, {t:"ORIENTATION",v:"LANDSCAPE",s:"NORMAL",c:theme.primary}]
        case 2: return [
            {t:"AIRCRAFT PROFILE",v:controller.aircraftProfileName,s:"TAP TO BROWSE • "+controller.aircraftProfileState,c:controller.aircraftProfileState === "APPROVED" ? theme.green : theme.amber,a:"aircraft"}, {t:"V-SPEEDS",v:"POH / AFM",s:"REVIEW REQUIRED",c:theme.amber},
            {t:"UNITS",v:"KT • FT • INHG",s:"NORMALIZED",c:theme.green}, {t:"WEIGHT & BALANCE",v:"NOT CONFIGURED",s:"REQUIRED",c:theme.amber},
            {t:"CATALOG",v:controller.catalogVersion,s:"ACTIVE SNAPSHOT",c:theme.cyan}, {t:"CALLSIGN",v:"EFIS TEST",s:"BENCH",c:theme.primary}]
        case 3: return [
            {t:"FLIGHT COMPUTER",v:"PIXHAWK 6X",s:"PLANNED",c:theme.amber}, {t:"GNSS",v:"M10",s:"PLANNED",c:theme.amber},
            {t:"AIRSPEED",v:"MATEK ASPD-DLVR",s:"PLANNED",c:theme.amber}, {t:"IMU PRIMARY",v:"PIXHAWK",s:"NOT CONNECTED",c:theme.secondary},
            {t:"AIR DATA BACKUP",v:"ESP32-S3",s:"INTERFACE READY",c:theme.cyan},
            {t:"BANCO PROVA SENSORI",v:"MAVLINK MONITOR-ONLY",s:"OSSERVAZIONE",c:theme.green,a:"bench"}, {t:"CALIBRATION",v:"GUIDED WIZARD",s:"GROUND ONLY",c:theme.primary}]
        case 4: return [
            {t:"CONNECTIVITY",v:connectivity.state,s:"TAP • "+connectivity.activeSsid,c:connectivity.state === "ONLINE" ? theme.green : theme.amber,a:"connectivity"}, {t:"HOTSPOT / WI-FI",v:connectivity.activeSsid,s:connectivity.message,c:theme.cyan,a:"connectivity"},
            {t:"CAN A",v:"500 KBIT/S",s:"PIXHAWK / PANEL",c:theme.cyan}, {t:"CAN MENU TREE",v:"V2",s:"RPi4 COMPATIBLE",c:theme.green},
            {t:"ESP32-S3",v:"NODE DISCOVERY",s:"PENDING",c:theme.amber}, {t:"MAVLINK",v:"SERIAL / UDP",s:"DISABLED",c:theme.secondary}]
        case 5: return [
            {t:"NAV SOURCE",v:"GPS1",s:"SIMULATION",c:theme.amber}, {t:"QNH",v:"1013 HPA",s:"EDITABLE VALUE",c:theme.cyan},
            {t:"XPDR",v:"1200",s:"STANDBY MODEL",c:theme.cyan}, {t:"NAV DATABASE",v:"VENETO / LIPH",s:"TEST DATA",c:theme.amber},
            {t:"RADIO NAV",v:"EVIS NAV",s:"HARDWARE MAPPING",c:theme.amber}, {t:"AIRPORT DATABASE",v:controller.selectedAirport.icao || "NONE",s:controller.navigationDatabaseVersion+" • TAP TO SELECT",c:theme.cyan,a:"airport"}]
        case 6: return [
            {t:"EMS SOURCE",v:"CORE V2 SIM",s:"ACTIVE",c:theme.green}, {t:"CYLINDERS",v:"4 × CHT / EGT",s:"LIVE",c:theme.green},
            {t:"ENGINE PROFILE",v:"LSA GENERIC",s:"TAP TO BROWSE • REPLACE WITH POH",c:theme.amber,a:"engine"}, {t:"LIMITS",v:"CAUTION / WARNING",s:"SIMULATION",c:theme.amber},
            {t:"FUEL SENSORS",v:"LEFT / RIGHT",s:"SIMULATION",c:theme.cyan}, {t:"FAULT TEST",v:controller.engineFaultMode,s:"BENCH ONLY",c:theme.magenta}]
        case 7: return [
            {t:"VIDEO DECODER",v:"RPi4 H.264",s:"TARGET NODE",c:theme.cyan}, {t:"CAMERA 1",v:"FORWARD",s:"NOT CONNECTED",c:theme.secondary},
            {t:"CAMERA 2",v:"LEFT",s:"NOT CONNECTED",c:theme.secondary}, {t:"CAMERA 3",v:"RIGHT",s:"NOT CONNECTED",c:theme.secondary},
            {t:"CAMERA 4",v:"TAIL / GIMBAL",s:"NOT CONNECTED",c:theme.secondary}, {t:"FAIL-SAFE",v:"AUTO REVERT PFD",s:"ARMED",c:theme.green}]
        case 8: return [
            {t:"MASTER ALERTS",v:"ENABLED",s:"23 LEGACY RULES",c:theme.green}, {t:"VOICE",v:"ITALIAN / ENGLISH",s:"NORMALIZED",c:theme.cyan},
            {t:"BUZZER",v:"GPIO / AUDIO",s:"HARDWARE TEST",c:theme.amber}, {t:"VOLUME",v:"75 %",s:"GROUND SETUP",c:theme.primary},
            {t:"TRAFFIC",v:"VOICE + VISUAL",s:"PLANNED",c:theme.amber}, {t:"MUTE POLICY",v:"CRITICAL LOCKED",s:"SAFETY",c:theme.warning}]
        case 9: return [
            {t:"RECORDER",v:controller.blackBoxState,s:"CORE V2",c:controller.blackBoxState === "RECORDING" ? theme.green : theme.amber}, {t:"RECORDS",v:String(controller.blackBoxRecords),s:"CURRENT SESSION",c:theme.cyan},
            {t:"STORAGE",v:"SEGMENTED",s:"AUTO ROTATION",c:theme.green}, {t:"FLIGHT LOG",v:"EVENT + TELEMETRY",s:"NORMALIZED",c:theme.cyan},
            {t:"EXPORT",v:"USB / NETWORK",s:"PLANNED",c:theme.amber}, {t:"RETENTION",v:"CONFIGURABLE",s:"PENDING",c:theme.secondary}]
        default: return [
            {t:"CORE SEQUENCE",v:String(controller.sequence),s:"LIVE",c:theme.green}, {t:"SYSTEM HEALTH",v:String(controller.healthLevel),s:"MONITORED",c:controller.healthLevel >= 2 ? theme.warning : theme.green},
            {t:"FLIGHT FAULT",v:controller.faultMode,s:"INJECTION",c:controller.faultMode === "NONE" ? theme.green : theme.amber}, {t:"ENGINE FAULT",v:controller.engineFaultMode,s:"INJECTION",c:controller.engineFaultMode === "NONE" ? theme.green : theme.amber},
            {t:"TOUCH TEST",v:"AVAILABLE",s:"GROUND ONLY",c:theme.cyan}, {t:"CONFIG AUDIT",v:"NO PERSISTENCE YET",s:"PHASE 1",c:theme.amber}]
        }
    }

    color: "#FA050C12"
    Rectangle { id: header; anchors { left: parent.left; right: parent.right; top: parent.top } height: 62*overlay.uiScale; color: "#07131D"; border.color: theme.borderBright
        Text { x: 18*overlay.uiScale; y: 8*overlay.uiScale; text: "EFIS NEXT"; color: theme.primary; font.pixelSize: 20*overlay.uiScale; font.bold: true }
        Text { x: 18*overlay.uiScale; y: 36*overlay.uiScale; text: "UNIFIED SETUP CENTER • RPi4 / RPi5 / ESP32-S3"; color: theme.cyan; font.pixelSize: 9*overlay.uiScale; font.bold: true }
        Text { anchors { right: closeButton.left; rightMargin: 18*overlay.uiScale; verticalCenter: parent.verticalCenter } text: "GROUND CONFIGURATION"; color: theme.amber; font.pixelSize: 10*overlay.uiScale; font.bold: true }
        Rectangle { id: closeButton; anchors { right: parent.right; rightMargin: 12*overlay.uiScale; verticalCenter: parent.verticalCenter } width: 92*overlay.uiScale; height: 38*overlay.uiScale; radius: 3*overlay.uiScale; color: "#162633"; border.color: theme.borderBright
            Text { anchors.centerIn: parent; text: "CLOSE"; color: theme.primary; font.pixelSize: 11*overlay.uiScale; font.bold: true } TapHandler { onTapped: overlay.closeRequested() } }
    }
    Rectangle { id: sidebar; anchors { left: parent.left; top: header.bottom; bottom: footer.top } width: 190*overlay.uiScale; color: "#071019"; border.color: theme.border
        Column { anchors { fill: parent; margins: 7*overlay.uiScale } spacing: 3*overlay.uiScale
            Repeater { model: overlay.sections; delegate: Rectangle { required property string modelData; required property int index; width: parent.width; height: 39*overlay.uiScale; radius: 2*overlay.uiScale; color: overlay.sectionIndex === index ? "#143444" : "transparent"; border.color: overlay.sectionIndex === index ? theme.cyan : "transparent"
                Rectangle { width: 3*overlay.uiScale; height: parent.height-10*overlay.uiScale; anchors { left: parent.left; leftMargin: 4*overlay.uiScale; verticalCenter: parent.verticalCenter } color: overlay.sectionIndex === index ? theme.cyan : theme.border }
                Text { x: 14*overlay.uiScale; anchors.verticalCenter: parent.verticalCenter; text: modelData; color: overlay.sectionIndex === index ? theme.primary : theme.secondary; font.pixelSize: 9*overlay.uiScale; font.bold: true }
                TapHandler { onTapped: overlay.sectionIndex = index }
            } }
        }
    }
    Item { anchors { left: sidebar.right; right: parent.right; top: header.bottom; bottom: footer.top; margins: 12*overlay.uiScale }
        Text { id: pageTitle; text: overlay.sections[overlay.sectionIndex]; color: theme.primary; font.pixelSize: 18*overlay.uiScale; font.bold: true }
        Text { anchors { right: parent.right; baseline: pageTitle.baseline } text: "PROFILE: PROFESSIONAL • NODE: PFD-01"; color: theme.secondary; font.pixelSize: 8*overlay.uiScale; font.bold: true }
        Grid { anchors { left: parent.left; right: parent.right; top: pageTitle.bottom; topMargin: 12*overlay.uiScale } columns: 3; columnSpacing: 8*overlay.uiScale; rowSpacing: 8*overlay.uiScale
            Repeater { model: overlay.items(overlay.sectionIndex); delegate: Rectangle { required property var modelData; width: 258*overlay.uiScale; height: 112*overlay.uiScale; radius: 4*overlay.uiScale; color: "#C90A1721"; border.color: theme.border
                Rectangle { anchors { left: parent.left; top: parent.top; bottom: parent.bottom } width: 4*overlay.uiScale; color: modelData.c }
                Text { x: 14*overlay.uiScale; y: 12*overlay.uiScale; text: modelData.t; color: theme.secondary; font.pixelSize: 8*overlay.uiScale; font.bold: true }
                Text { x: 14*overlay.uiScale; y: 39*overlay.uiScale; width: parent.width-25*overlay.uiScale; text: modelData.v; color: modelData.c; font.pixelSize: 17*overlay.uiScale; font.bold: true; elide: Text.ElideRight }
                Text { x: 14*overlay.uiScale; anchors { bottom: parent.bottom; bottomMargin: 12*overlay.uiScale } text: modelData.s; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
                MouseArea { anchors.fill: parent; enabled: modelData.a !== undefined; onClicked: { if (modelData.a === "airport") overlay.airportSelectorVisible=true; else if (modelData.a === "bench") overlay.benchVisible=true; else if (modelData.a === "connectivity") overlay.connectivityVisible=true; else { overlay.catalogMode=modelData.a; overlay.catalogVisible=true } } }
            } }
        }
    }
    Rectangle {
        visible: overlay.connectivityVisible; z: 40
        anchors { left: sidebar.right; right: parent.right; top: header.bottom; bottom: footer.top }
        color: "#FD061018"; border.color: theme.cyan
        Text { x:18*overlay.uiScale; y:14*overlay.uiScale; text:"CONNECTIVITY • SAVED HOTSPOTS"; color:theme.primary; font.pixelSize:17*overlay.uiScale; font.bold:true }
        Text { x:18*overlay.uiScale; y:44*overlay.uiScale; text:connectivity.state+" • "+connectivity.activeSsid+" • "+connectivity.message; color:connectivity.state === "ONLINE" ? theme.green : theme.amber; font.pixelSize:9*overlay.uiScale; font.bold:true }
        Rectangle { x:620*overlay.uiScale; y:12*overlay.uiScale; width:82*overlay.uiScale; height:36*overlay.uiScale; color:"#14313D"; border.color:theme.cyan
            Text { anchors.centerIn:parent; text:"SCAN"; color:theme.cyan; font.pixelSize:9*overlay.uiScale; font.bold:true } TapHandler { onTapped:connectivity.refresh() } }
        Rectangle { x:710*overlay.uiScale; y:12*overlay.uiScale; width:82*overlay.uiScale; height:36*overlay.uiScale; color:"#18232B"; border.color:theme.borderBright
            Text { anchors.centerIn:parent; text:"BACK"; color:theme.primary; font.pixelSize:9*overlay.uiScale; font.bold:true } TapHandler { onTapped:overlay.connectivityVisible=false } }
        ListView { x:18*overlay.uiScale; y:78*overlay.uiScale; width:774*overlay.uiScale; height:330*overlay.uiScale; model:connectivity.networks; clip:true; spacing:5*overlay.uiScale
            delegate: Rectangle { required property var modelData; width:ListView.view.width; height:58*overlay.uiScale; color:modelData.active?"#12313D":"#0B1821"; border.color:modelData.active?theme.green:theme.border
                Text { x:12*overlay.uiScale; y:9*overlay.uiScale; text:modelData.ssid; color:theme.primary; font.pixelSize:11*overlay.uiScale; font.bold:true }
                Text { x:12*overlay.uiScale; y:34*overlay.uiScale; text:"SIGNAL "+modelData.signal+"% • "+modelData.security; color:theme.secondary; font.pixelSize:7*overlay.uiScale; font.bold:true }
                Text { anchors { right:parent.right; rightMargin:14*overlay.uiScale; verticalCenter:parent.verticalCenter } text:modelData.active?"ACTIVE":"USE SAVED PROFILE"; color:modelData.active?theme.green:theme.cyan; font.pixelSize:8*overlay.uiScale; font.bold:true }
                TapHandler { enabled:!modelData.active; onTapped:connectivity.activateSavedProfile(modelData.ssid, overlay.controller.airspeed < 5) }
            }
        }
        Text { x:18*overlay.uiScale; y:424*overlay.uiScale; width:774*overlay.uiScale; text:"Passwords are managed only by NetworkManager and never enter EFIS, logs or telemetry. New hotspot credentials must be saved through the operating-system setup while on ground."; color:theme.amber; font.pixelSize:8*overlay.uiScale; wrapMode:Text.WordWrap }
    }
    Rectangle {
        visible: overlay.airportSelectorVisible; z: 38
        anchors { left: sidebar.right; right: parent.right; top: header.bottom; bottom: footer.top }
        color: "#FD061018"; border.color: theme.cyan
        Text { x: 18*overlay.uiScale; y: 12*overlay.uiScale; text: "AIRPORT SELECTOR • "+overlay.controller.airportCatalog.length+" LOADED"; color: theme.primary; font.pixelSize: 17*overlay.uiScale; font.bold: true }
        Text { x: 18*overlay.uiScale; y: 40*overlay.uiScale; text: "NAV DB "+overlay.controller.navigationDatabaseVersion+" • source status is always shown"; color: theme.amber; font.pixelSize: 8*overlay.uiScale; font.bold: true }
        Rectangle { x: 650*overlay.uiScale; y: 10*overlay.uiScale; width: 150*overlay.uiScale; height: 36*overlay.uiScale; color: "#14232D"; border.color: theme.borderBright
            Text { anchors.centerIn: parent; text: "BACK"; color: theme.primary; font.pixelSize: 8*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: overlay.airportSelectorVisible=false }
        }
        Rectangle { x: 18*overlay.uiScale; y: 68*overlay.uiScale; width: 430*overlay.uiScale; height: 38*overlay.uiScale; color: "#101E28"; border.color: airportSearch.activeFocus ? theme.cyan : theme.borderBright
            TextField { id: airportSearch; anchors.fill: parent; placeholderText: "Search ICAO, airport or city"; text: overlay.airportQuery; onTextChanged: overlay.airportQuery=text; font.pixelSize: 9*overlay.uiScale }
        }
        Rectangle { x: 458*overlay.uiScale; y: 68*overlay.uiScale; width: 160*overlay.uiScale; height: 38*overlay.uiScale; color: !overlay.airportNearestOnly ? "#12313D" : "#14232D"; border.color: !overlay.airportNearestOnly ? theme.cyan : theme.border
            Text { anchors.centerIn: parent; text: "ALL"; color: theme.primary; font.pixelSize: 8*overlay.uiScale; font.bold: true } TapHandler { onTapped: overlay.airportNearestOnly=false }
        }
        Rectangle { x: 628*overlay.uiScale; y: 68*overlay.uiScale; width: 160*overlay.uiScale; height: 38*overlay.uiScale; color: overlay.airportNearestOnly ? "#12313D" : "#14232D"; border.color: overlay.airportNearestOnly ? theme.cyan : theme.border
            Text { anchors.centerIn: parent; text: "NEAREST"; color: theme.primary; font.pixelSize: 8*overlay.uiScale; font.bold: true } TapHandler { onTapped: overlay.airportNearestOnly=true }
        }
        ListView { x: 18*overlay.uiScale; y: 118*overlay.uiScale; width: 782*overlay.uiScale; height: 270*overlay.uiScale; model: overlay.visibleAirports(); clip: true; spacing: 5*overlay.uiScale
            delegate: Rectangle { required property var modelData; required property int index; width: ListView.view.width; height: 64*overlay.uiScale; color: modelData.icao === overlay.controller.selectedAirport.icao ? "#12313D" : (index%2 ? "#0B1821" : "#0E1D27"); border.color: modelData.icao === overlay.controller.selectedAirport.icao ? theme.cyan : theme.border
                Text { x: 12*overlay.uiScale; y: 10*overlay.uiScale; text: modelData.icao+" • "+modelData.name; color: theme.primary; font.pixelSize: 11*overlay.uiScale; font.bold: true }
                Text { x: 12*overlay.uiScale; y: 36*overlay.uiScale; text: modelData.city+" • "+modelData.elevation_ft+" FT • "+modelData.runways.length+" RWY"+(modelData.distance_nm === undefined ? "" : " • "+Number(modelData.distance_nm).toFixed(1)+" NM"); color: theme.secondary; font.pixelSize: 8*overlay.uiScale }
                Text { anchors { right: parent.right; rightMargin: 14*overlay.uiScale; verticalCenter: parent.verticalCenter } text: modelData.status; color: modelData.status === "ACTIVE" ? theme.green : theme.amber; font.pixelSize: 8*overlay.uiScale; font.bold: true }
                TapHandler { onTapped: overlay.controller.selectAirport(modelData.icao) }
            }
        }
        Rectangle { x: 18*overlay.uiScale; y: 404*overlay.uiScale; width: 782*overlay.uiScale; height: 52*overlay.uiScale; color: "#17120B"; border.color: theme.amber
            Text { x: 12*overlay.uiScale; y: 7*overlay.uiScale; text: "SELECTED • "+(overlay.controller.selectedAirport.icao || "NONE")+" • "+(overlay.controller.selectedAirport.status || "NO DATA"); color: theme.amber; font.pixelSize: 9*overlay.uiScale; font.bold: true }
            Text { x: 12*overlay.uiScale; y: 29*overlay.uiScale; text: overlay.controller.setupMessage; color: theme.primary; font.pixelSize: 7*overlay.uiScale }
        }
    }
    Rectangle {
        visible: overlay.catalogVisible; z: 30
        anchors { left: sidebar.right; right: parent.right; top: header.bottom; bottom: footer.top }
        color: "#FA061018"; border.color: theme.cyan
        readonly property var catalogModel: overlay.catalogMode === "aircraft" ? overlay.controller.aircraftCatalog : overlay.controller.engineCatalog
        Text { id: catalogTitle; x: 16*overlay.uiScale; y: 12*overlay.uiScale
            text: overlay.catalogMode === "aircraft" ? "AIRCRAFT CATALOG • "+parent.catalogModel.length+" MODELS" : "ENGINE CATALOG • "+parent.catalogModel.length+" MODELS"
            color: theme.primary; font.pixelSize: 16*overlay.uiScale; font.bold: true }
        Text { x: 16*overlay.uiScale; y: 39*overlay.uiScale; text: "Selection is staged only • POH/AFM verification required before activation"; color: theme.amber; font.pixelSize: 8*overlay.uiScale; font.bold: true }
        Rectangle { anchors { right: parent.right; rightMargin: 12*overlay.uiScale; top: parent.top; topMargin: 10*overlay.uiScale } width: 86*overlay.uiScale; height: 34*overlay.uiScale; color: "#142632"; border.color: theme.borderBright
            Text { anchors.centerIn: parent; text: "BACK"; color: theme.primary; font.pixelSize: 9*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: overlay.catalogVisible=false }
        }
        ListView { anchors { left: parent.left; right: parent.right; top: catalogTitle.bottom; bottom: stagedBar.top; margins: 16*overlay.uiScale; topMargin: 28*overlay.uiScale }
            clip: true; spacing: 4*overlay.uiScale; model: parent.catalogModel
            delegate: Rectangle { required property var modelData; required property int index; width: ListView.view.width; height: 48*overlay.uiScale; color: index%2 ? "#0C1822" : "#101F2A"; border.color: theme.border
                Text { x: 12*overlay.uiScale; anchors.verticalCenter: parent.verticalCenter; width: parent.width*.48; text: modelData.manufacturer+" • "+modelData.model+(modelData.variant ? " • "+modelData.variant : ""); color: theme.primary; font.pixelSize: 10*overlay.uiScale; font.bold: true; elide: Text.ElideRight }
                Text { anchors { right: statusText.left; rightMargin: 18*overlay.uiScale; verticalCenter: parent.verticalCenter } text: overlay.catalogMode === "aircraft" ? modelData.category : ((modelData.cylinders || "—")+" CYL • "+(modelData.power_hp || "—")+" HP"); color: theme.cyan; font.pixelSize: 8*overlay.uiScale; font.bold: true }
                Text { id: statusText; anchors { right: parent.right; rightMargin: 12*overlay.uiScale; verticalCenter: parent.verticalCenter } text: modelData.status; color: modelData.status === "APPROVED" ? theme.green : theme.amber; font.pixelSize: 7*overlay.uiScale; font.bold: true }
                TapHandler { onTapped: { overlay.stagedSelection=modelData.manufacturer+" • "+modelData.model; overlay.catalogVisible=false; if (overlay.catalogMode === "aircraft") { overlay.controller.stageAircraft(modelData.id); overlay.reviewVisible=true } else { overlay.controller.stageEngine(modelData.id); overlay.engineReviewVisible=true } } }
            }
        }
        Rectangle { id: stagedBar; anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 48*overlay.uiScale; color: "#0A1720"; border.color: theme.borderBright
            Text { x: 14*overlay.uiScale; anchors.verticalCenter: parent.verticalCenter; text: overlay.stagedSelection === "" ? "NO STAGED SELECTION" : "STAGED • "+overlay.stagedSelection; color: overlay.stagedSelection === "" ? theme.secondary : theme.cyan; font.pixelSize: 9*overlay.uiScale; font.bold: true }
        }
    }
    Rectangle {
        id: aircraftReview
        visible: overlay.reviewVisible; z: 35
        anchors { left: sidebar.right; right: parent.right; top: header.bottom; bottom: footer.top }
        color: "#FC061018"; border.color: theme.cyan
        readonly property var staged: overlay.controller.stagedAircraftProfile || ({})
        readonly property var speeds: staged.speeds_kt || ({})
        Text { x: 18*overlay.uiScale; y: 14*overlay.uiScale; text: "STAGED AIRCRAFT PROFILE REVIEW"; color: theme.primary; font.pixelSize: 17*overlay.uiScale; font.bold: true }
        Text { x: 18*overlay.uiScale; y: 43*overlay.uiScale; text: (parent.staged.manufacturer || "—")+" • "+(parent.staged.model || "—"); color: theme.cyan; font.pixelSize: 14*overlay.uiScale; font.bold: true }
        Rectangle { anchors { right: parent.right; rightMargin: 14*overlay.uiScale; top: parent.top; topMargin: 12*overlay.uiScale } width: 170*overlay.uiScale; height: 40*overlay.uiScale; color: "#18232B"; border.color: theme.borderBright
            Text { anchors.centerIn: parent; text: "BACK TO CATALOG"; color: theme.primary; font.pixelSize: 9*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: { overlay.reviewVisible=false; overlay.catalogVisible=true } }
        }
        Grid { x: 18*overlay.uiScale; y: 82*overlay.uiScale; columns: 4; columnSpacing: 7*overlay.uiScale; rowSpacing: 7*overlay.uiScale
            Repeater { model: [
                    {k:"CATEGORY",v:aircraftReview.staged.category || "—"}, {k:"ICAO",v:aircraftReview.staged.icao_type || "ZZZZ"},
                    {k:"MTOW",v:(aircraftReview.staged.mtow_kg || "—")+" KG"}, {k:"EMPTY",v:(aircraftReview.staged.empty_weight_kg || "—")+" KG"},
                    {k:"ENGINE",v:(aircraftReview.staged.engine_manufacturer || "—")+" "+(aircraftReview.staged.engine_model || "")}, {k:"FUEL",v:(aircraftReview.staged.fuel_capacity_l || "—")+" L"},
                    {k:"SOURCE",v:aircraftReview.staged.status || "—"}, {k:"CATALOG",v:aircraftReview.staged.catalog_version || "—"}
                ]; delegate: Rectangle { required property var modelData; width: 190*overlay.uiScale; height: 68*overlay.uiScale; color: "#0D1C26"; border.color: theme.border
                    Text { x: 10*overlay.uiScale; y: 8*overlay.uiScale; text: modelData.k; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
                    Text { x: 10*overlay.uiScale; y: 29*overlay.uiScale; width: parent.width-20*overlay.uiScale; text: modelData.v; color: theme.primary; font.pixelSize: 11*overlay.uiScale; font.bold: true; elide: Text.ElideRight }
                }
            }
        }
        Rectangle { x: 18*overlay.uiScale; y: 232*overlay.uiScale; width: 782*overlay.uiScale; height: 84*overlay.uiScale; color: "#0B1720"; border.color: theme.border
            Text { x: 12*overlay.uiScale; y: 8*overlay.uiScale; text: "V-SPEEDS • KT"; color: theme.secondary; font.pixelSize: 8*overlay.uiScale; font.bold: true }
            Row { x: 12*overlay.uiScale; y: 31*overlay.uiScale; spacing: 13*overlay.uiScale
                Repeater { model: ["VSO","VS1","VFE","VX","VY","VA","VNO","VNE","BEST_GLIDE","VREF"]; delegate: Column { required property string modelData; spacing: 2*overlay.uiScale
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData; color: theme.secondary; font.pixelSize: 6*overlay.uiScale; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: aircraftReview.speeds[modelData] || "—"; color: aircraftReview.speeds[modelData] ? theme.cyan : theme.amber; font.pixelSize: 11*overlay.uiScale; font.bold: true }
                } }
            }
        }
        Rectangle { x: 18*overlay.uiScale; y: 329*overlay.uiScale; width: 782*overlay.uiScale; height: 68*overlay.uiScale; color: "#17120B"; border.color: theme.amber
            Text { x: 14*overlay.uiScale; y: 9*overlay.uiScale; text: "VALIDATION • "+overlay.controller.stagingStatus; color: theme.amber; font.pixelSize: 11*overlay.uiScale; font.bold: true }
            Text { x: 14*overlay.uiScale; y: 34*overlay.uiScale; width: parent.width-28*overlay.uiScale; text: overlay.controller.setupMessage; color: theme.primary; font.pixelSize: 8*overlay.uiScale; wrapMode: Text.WordWrap }
        }
    }
    Rectangle {
        visible: overlay.verificationVisible; z: 45
        anchors { left: sidebar.right; right: parent.right; top: header.bottom; bottom: footer.top }
        color: "#FD061018"; border.color: theme.cyan
        property bool speedsOk: false
        property bool weightsOk: false
        property bool engineOk: false
        Text { x: 24*overlay.uiScale; y: 18*overlay.uiScale; text: "POH / AFM VERIFICATION"; color: theme.primary; font.pixelSize: 18*overlay.uiScale; font.bold: true }
        Text { x: 24*overlay.uiScale; y: 52*overlay.uiScale; width: 740*overlay.uiScale; text: "Ground-only safety gate • compare the installed aircraft, engine and operating limitations with the current approved document."; color: theme.amber; font.pixelSize: 9*overlay.uiScale; wrapMode: Text.WordWrap }
        Text { x: 24*overlay.uiScale; y: 94*overlay.uiScale; text: "DOCUMENT REFERENCE / REVISION"; color: theme.secondary; font.pixelSize: 8*overlay.uiScale; font.bold: true }
        Rectangle { x: 24*overlay.uiScale; y: 116*overlay.uiScale; width: 620*overlay.uiScale; height: 44*overlay.uiScale; color: "#101E28"; border.color: pohInput.activeFocus ? theme.cyan : theme.borderBright
            TextInput { id: pohInput; anchors { fill: parent; margins: 10*overlay.uiScale } color: theme.primary; font.pixelSize: 12*overlay.uiScale; clip: true; selectByMouse: true }
            Text { anchors { left: parent.left; leftMargin: 10*overlay.uiScale; verticalCenter: parent.verticalCenter } visible: pohInput.text.length === 0; text: "Example: POH Doc. 1234 • Rev. 5 • 2026-04-12"; color: theme.secondary; font.pixelSize: 9*overlay.uiScale }
        }
        Column { x: 24*overlay.uiScale; y: 184*overlay.uiScale; spacing: 12*overlay.uiScale
            Repeater { model: [
                {label:"V-speeds and limitations match the installed aircraft", key:0},
                {label:"MTOW, empty weight and fuel capacity have been checked", key:1},
                {label:"Engine model is compatible with the actual installation", key:2}
            ]; delegate: Rectangle { required property var modelData; width: 700*overlay.uiScale; height: 48*overlay.uiScale; color: "#0C1922"; border.color: theme.border
                    readonly property bool checked: modelData.key === 0 ? parent.parent.speedsOk : modelData.key === 1 ? parent.parent.weightsOk : parent.parent.engineOk
                    Rectangle { x: 12*overlay.uiScale; anchors.verticalCenter: parent.verticalCenter; width: 24*overlay.uiScale; height: 24*overlay.uiScale; color: parent.checked ? theme.green : "transparent"; border.color: parent.checked ? theme.green : theme.secondary
                        Text { anchors.centerIn: parent; visible: parent.parent.checked; text: "✓"; color: "#061018"; font.pixelSize: 15*overlay.uiScale; font.bold: true }
                    }
                    Text { x: 50*overlay.uiScale; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: theme.primary; font.pixelSize: 10*overlay.uiScale; font.bold: true }
                    TapHandler { onTapped: { if (modelData.key === 0) parent.parent.parent.speedsOk=!parent.parent.parent.speedsOk; else if (modelData.key === 1) parent.parent.parent.weightsOk=!parent.parent.parent.weightsOk; else parent.parent.parent.engineOk=!parent.parent.parent.engineOk } }
                }
            }
        }
        Rectangle { x: 24*overlay.uiScale; y: 360*overlay.uiScale; width: 180*overlay.uiScale; height: 44*overlay.uiScale; color: "#14232D"; border.color: theme.borderBright
            Text { anchors.centerIn: parent; text: "CANCEL"; color: theme.primary; font.pixelSize: 10*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: overlay.verificationVisible=false }
        }
        Rectangle { x: 220*overlay.uiScale; y: 360*overlay.uiScale; width: 424*overlay.uiScale; height: 44*overlay.uiScale; color: "#123025"; border.color: theme.green
            Text { anchors.centerIn: parent; text: "CONFIRM VERIFICATION"; color: theme.green; font.pixelSize: 10*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: { if (overlay.controller.verifyStagedAircraft(pohInput.text, parent.parent.speedsOk, parent.parent.weightsOk, parent.parent.engineOk)) overlay.verificationVisible=false } }
        }
        Text { x: 24*overlay.uiScale; y: 420*overlay.uiScale; width: 740*overlay.uiScale; text: overlay.controller.setupMessage; color: overlay.controller.stagingStatus === "VERIFICATION_REQUIRED" ? theme.warning : theme.secondary; font.pixelSize: 8*overlay.uiScale; wrapMode: Text.WordWrap }
    }
    Rectangle {
        visible: overlay.engineReviewVisible; z: 42
        anchors { left: sidebar.right; right: parent.right; top: header.bottom; bottom: footer.top }
        color: "#FD061018"; border.color: theme.cyan
        readonly property var engine: overlay.controller.stagedEngineProfile
        Text { x: 20*overlay.uiScale; y: 12*overlay.uiScale; text: "ENGINE PROFILE • LIMITS"; color: theme.primary; font.pixelSize: 17*overlay.uiScale; font.bold: true }
        Text { x: 20*overlay.uiScale; y: 40*overlay.uiScale; text: (parent.engine.manufacturer || "—")+" • "+(parent.engine.model || "—")+" • "+(parent.engine.power_hp || "—")+" HP"; color: theme.cyan; font.pixelSize: 12*overlay.uiScale; font.bold: true }
        Rectangle { x: 650*overlay.uiScale; y: 12*overlay.uiScale; width: 150*overlay.uiScale; height: 36*overlay.uiScale; color: "#14232D"; border.color: theme.borderBright
            Text { anchors.centerIn: parent; text: "BACK TO CATALOG"; color: theme.primary; font.pixelSize: 8*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: { overlay.engineReviewVisible=false; overlay.catalogVisible=true } }
        }
        Text { x: 20*overlay.uiScale; y: 72*overlay.uiScale; text: "ENGINE MANUAL / REVISION"; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
        Rectangle { x: 20*overlay.uiScale; y: 90*overlay.uiScale; width: 480*overlay.uiScale; height: 36*overlay.uiScale; color: "#101E28"; border.color: theme.borderBright
            TextInput { id: engineManual; anchors { fill: parent; margins: 8*overlay.uiScale } color: theme.primary; font.pixelSize: 10*overlay.uiScale; clip: true }
        }
        Text { x: 520*overlay.uiScale; y: 72*overlay.uiScale; text: "CYLINDERS"; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
        Rectangle { x: 520*overlay.uiScale; y: 90*overlay.uiScale; width: 100*overlay.uiScale; height: 36*overlay.uiScale; color: "#101E28"; border.color: theme.borderBright
            TextInput { id: engineCylinders; anchors { fill: parent; margins: 8*overlay.uiScale } text: String(parent.parent.engine.cylinders || 4); color: theme.primary; font.pixelSize: 10*overlay.uiScale; inputMethodHints: Qt.ImhDigitsOnly }
        }
        Grid { x: 20*overlay.uiScale; y: 145*overlay.uiScale; columns: 4; columnSpacing: 8*overlay.uiScale; rowSpacing: 8*overlay.uiScale
            Rectangle { width: 190*overlay.uiScale; height: 72*overlay.uiScale; color: "#0D1C26"; border.color: theme.border
                Text { x: 9*overlay.uiScale; y: 7*overlay.uiScale; text: "RPM MAX"; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
                TextInput { id: rpmMaxInput; x: 9*overlay.uiScale; y: 28*overlay.uiScale; width: 170*overlay.uiScale; text: String(overlay.controller.stagedEngineProfile.rpm_max || ""); color: theme.cyan; font.pixelSize: 14*overlay.uiScale; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            }
            Rectangle { width: 190*overlay.uiScale; height: 72*overlay.uiScale; color: "#0D1C26"; border.color: theme.border
                Text { x: 9*overlay.uiScale; y: 7*overlay.uiScale; text: "OIL PRESS MIN / MAX • PSI"; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
                Row { x: 9*overlay.uiScale; y: 28*overlay.uiScale; spacing: 10*overlay.uiScale
                    TextInput { id: oilPMinInput; width: 70*overlay.uiScale; color: theme.cyan; font.pixelSize: 14*overlay.uiScale; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    TextInput { id: oilPMaxInput; width: 70*overlay.uiScale; color: theme.cyan; font.pixelSize: 14*overlay.uiScale; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                }
            }
            Rectangle { width: 190*overlay.uiScale; height: 72*overlay.uiScale; color: "#0D1C26"; border.color: theme.border
                Text { x: 9*overlay.uiScale; y: 7*overlay.uiScale; text: "OIL TEMP MAX • °C"; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
                TextInput { id: oilTMaxInput; x: 9*overlay.uiScale; y: 28*overlay.uiScale; width: 170*overlay.uiScale; color: theme.cyan; font.pixelSize: 14*overlay.uiScale; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            }
            Rectangle { width: 190*overlay.uiScale; height: 72*overlay.uiScale; color: "#0D1C26"; border.color: theme.border
                Text { x: 9*overlay.uiScale; y: 7*overlay.uiScale; text: "CHT MAX • °C"; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
                TextInput { id: chtMaxInput; x: 9*overlay.uiScale; y: 28*overlay.uiScale; width: 170*overlay.uiScale; color: theme.cyan; font.pixelSize: 14*overlay.uiScale; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            }
            Rectangle { width: 190*overlay.uiScale; height: 72*overlay.uiScale; color: "#0D1C26"; border.color: theme.border
                Text { x: 9*overlay.uiScale; y: 7*overlay.uiScale; text: "EGT MAX • °C"; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
                TextInput { id: egtMaxInput; x: 9*overlay.uiScale; y: 28*overlay.uiScale; width: 170*overlay.uiScale; color: theme.cyan; font.pixelSize: 14*overlay.uiScale; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            }
            Rectangle { width: 190*overlay.uiScale; height: 72*overlay.uiScale; color: "#0D1C26"; border.color: theme.border
                Text { x: 9*overlay.uiScale; y: 7*overlay.uiScale; text: "FUEL PRESS MIN / MAX • PSI"; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
                Row { x: 9*overlay.uiScale; y: 28*overlay.uiScale; spacing: 10*overlay.uiScale
                    TextInput { id: fuelPMinInput; width: 70*overlay.uiScale; color: theme.cyan; font.pixelSize: 14*overlay.uiScale; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                    TextInput { id: fuelPMaxInput; width: 70*overlay.uiScale; color: theme.cyan; font.pixelSize: 14*overlay.uiScale; inputMethodHints: Qt.ImhFormattedNumbersOnly }
                }
            }
        }
        Rectangle { x: 20*overlay.uiScale; y: 322*overlay.uiScale; width: 580*overlay.uiScale; height: 52*overlay.uiScale; color: "#17120B"; border.color: theme.amber
            Text { x: 12*overlay.uiScale; y: 7*overlay.uiScale; text: "VALIDATION • "+overlay.controller.engineStagingStatus; color: theme.amber; font.pixelSize: 9*overlay.uiScale; font.bold: true }
            Text { x: 12*overlay.uiScale; y: 28*overlay.uiScale; text: overlay.controller.setupMessage; color: theme.primary; font.pixelSize: 7*overlay.uiScale }
        }
        Rectangle { x: 616*overlay.uiScale; y: 322*overlay.uiScale; width: 184*overlay.uiScale; height: 52*overlay.uiScale; color: "#123025"; border.color: theme.green
            Text { anchors.centerIn: parent; text: "VERIFY LIMITS"; color: theme.green; font.pixelSize: 9*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: overlay.controller.verifyStagedEngine(engineManual.text, Number(engineCylinders.text), Number(rpmMaxInput.text), Number(oilPMinInput.text), Number(oilPMaxInput.text), Number(oilTMaxInput.text), Number(chtMaxInput.text), Number(egtMaxInput.text), Number(fuelPMinInput.text), Number(fuelPMaxInput.text)) }
        }
        Text { x: 20*overlay.uiScale; y: 392*overlay.uiScale; text: "Next: map each CHT/EGT, oil, fuel-flow and RPM channel to CAN / ESP32-S3 inputs."; color: theme.secondary; font.pixelSize: 8*overlay.uiScale }
        Rectangle { x: 616*overlay.uiScale; y: 384*overlay.uiScale; width: 184*overlay.uiScale; height: 38*overlay.uiScale; color: "#102A35"; border.color: theme.cyan
            Text { anchors.centerIn: parent; text: "SENSOR MAPPING"; color: theme.cyan; font.pixelSize: 9*overlay.uiScale; font.bold: true }
            TapHandler { enabled: overlay.controller.engineStagingStatus === "READY_FOR_SENSOR_MAPPING"; onTapped: { overlay.engineReviewVisible=false; overlay.sensorMappingVisible=true } }
        }
    }
    Rectangle {
        visible: overlay.sensorMappingVisible; z: 44
        anchors { left: sidebar.right; right: parent.right; top: header.bottom; bottom: footer.top }
        color: "#FD061018"; border.color: theme.cyan
        Text { x: 18*overlay.uiScale; y: 10*overlay.uiScale; text: "EMS SENSOR MAPPING"; color: theme.primary; font.pixelSize: 17*overlay.uiScale; font.bold: true }
        Text { x: 18*overlay.uiScale; y: 38*overlay.uiScale; text: "Assign every signal to a unique physical endpoint • no automatic hardware assumptions"; color: theme.amber; font.pixelSize: 8*overlay.uiScale; font.bold: true }
        Rectangle { x: 650*overlay.uiScale; y: 10*overlay.uiScale; width: 150*overlay.uiScale; height: 34*overlay.uiScale; color: "#14232D"; border.color: theme.borderBright
            Text { anchors.centerIn: parent; text: "BACK TO ENGINE"; color: theme.primary; font.pixelSize: 8*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: { overlay.sensorMappingVisible=false; overlay.engineReviewVisible=true } }
        }
        Row { x: 18*overlay.uiScale; y: 68*overlay.uiScale; spacing: 2*overlay.uiScale
            Repeater { model: [{t:"SIGNAL",w:130},{t:"SOURCE",w:170},{t:"NODE",w:220},{t:"CHANNEL",w:100},{t:"STATE",w:130}]; delegate: Rectangle { required property var modelData; width: modelData.w*overlay.uiScale; height: 28*overlay.uiScale; color: "#132532"; border.color: theme.border
                    Text { anchors.centerIn: parent; text: modelData.t; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
            } }
        }
        ListView { id: mappingList; x: 18*overlay.uiScale; y: 98*overlay.uiScale; width: 758*overlay.uiScale; height: 294*overlay.uiScale; clip: true; spacing: 2*overlay.uiScale; model: overlay.controller.engineSensorMappings
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOn }
            delegate: Rectangle { required property var modelData; required property int index; width: mappingList.width-14*overlay.uiScale; height: 40*overlay.uiScale; color: index%2 ? "#0B1821" : "#0E1D27"; border.color: theme.border
                Text { x: 10*overlay.uiScale; anchors.verticalCenter: parent.verticalCenter; width: 120*overlay.uiScale; text: modelData.signal; color: theme.primary; font.pixelSize: 9*overlay.uiScale; font.bold: true }
                ComboBox { id: sourceBox; x: 132*overlay.uiScale; anchors.verticalCenter: parent.verticalCenter; width: 166*overlay.uiScale; height: 32*overlay.uiScale; model: ["UNASSIGNED","ESP32-S3","CAN-A","CAN-B","SERIAL","GPIO"]
                    Component.onCompleted: currentIndex=Math.max(0,model.indexOf(modelData.source)) }
                TextField { id: nodeField; x: 304*overlay.uiScale; anchors.verticalCenter: parent.verticalCenter; width: 210*overlay.uiScale; height: 32*overlay.uiScale; text: modelData.node; placeholderText: "node id / device"; font.pixelSize: 8*overlay.uiScale }
                SpinBox { id: channelBox; x: 520*overlay.uiScale; anchors.verticalCenter: parent.verticalCenter; width: 96*overlay.uiScale; height: 32*overlay.uiScale; from: -1; to: 255; value: modelData.channel }
                Rectangle { x: 622*overlay.uiScale; anchors.verticalCenter: parent.verticalCenter; width: 108*overlay.uiScale; height: 30*overlay.uiScale; color: modelData.state === "ASSIGNED" ? "#123025" : "#2B2110"; border.color: modelData.state === "ASSIGNED" ? theme.green : theme.amber
                    Text { anchors.centerIn: parent; text: modelData.state === "ASSIGNED" ? "TEST" : "SET"; color: modelData.state === "ASSIGNED" ? theme.green : theme.amber; font.pixelSize: 7*overlay.uiScale; font.bold: true }
                    TapHandler { onTapped: { if (modelData.state === "ASSIGNED") { overlay.controller.startEngineSensorTest(modelData.signal,false); overlay.channelTestVisible=true } else overlay.controller.setEngineSensorMapping(modelData.signal, sourceBox.currentText, nodeField.text, channelBox.value) } }
                }
            }
        }
        Rectangle { x: 18*overlay.uiScale; y: 405*overlay.uiScale; width: 560*overlay.uiScale; height: 48*overlay.uiScale; color: "#17120B"; border.color: theme.amber
            Text { x: 10*overlay.uiScale; y: 6*overlay.uiScale; text: "VALIDATION • "+overlay.controller.sensorMappingStatus; color: theme.amber; font.pixelSize: 8*overlay.uiScale; font.bold: true }
            Text { x: 10*overlay.uiScale; y: 26*overlay.uiScale; width: 540*overlay.uiScale; text: overlay.controller.setupMessage; color: theme.primary; font.pixelSize: 7*overlay.uiScale; elide: Text.ElideRight }
        }
        Rectangle { x: 592*overlay.uiScale; y: 405*overlay.uiScale; width: 184*overlay.uiScale; height: 48*overlay.uiScale; color: "#123025"; border.color: theme.green
            Text { anchors.centerIn: parent; text: "VALIDATE MAP"; color: theme.green; font.pixelSize: 9*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: overlay.controller.validateEngineSensorMappings() }
        }
    }
    Rectangle {
        visible: overlay.channelTestVisible; z: 48
        anchors { left: sidebar.right; right: parent.right; top: header.bottom; bottom: footer.top }
        color: "#FE061018"; border.color: theme.cyan
        Text { x: 22*overlay.uiScale; y: 14*overlay.uiScale; text: "LIVE CHANNEL TEST • "+overlay.controller.sensorTestSignal; color: theme.primary; font.pixelSize: 17*overlay.uiScale; font.bold: true }
        Text { x: 22*overlay.uiScale; y: 44*overlay.uiScale; text: "STATUS • "+overlay.controller.sensorTestStatus; color: overlay.controller.sensorTestFault ? theme.warning : theme.cyan; font.pixelSize: 10*overlay.uiScale; font.bold: true }
        Rectangle { x: 650*overlay.uiScale; y: 12*overlay.uiScale; width: 150*overlay.uiScale; height: 36*overlay.uiScale; color: "#14232D"; border.color: theme.borderBright
            Text { anchors.centerIn: parent; text: "BACK TO MAP"; color: theme.primary; font.pixelSize: 8*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: { overlay.controller.stopEngineSensorTest(); overlay.channelTestVisible=false } }
        }
        Grid { x: 22*overlay.uiScale; y: 88*overlay.uiScale; columns: 4; columnSpacing: 8*overlay.uiScale
            Repeater { model: [
                {k:"RAW INPUT",v:overlay.controller.sensorTestRaw.toFixed(2),u:"ADC / BUS"},
                {k:"CONVERTED",v:overlay.controller.sensorTestValue.toFixed(2),u:"ENGINE UNIT"},
                {k:"UPDATE RATE",v:overlay.controller.sensorTestRateHz.toFixed(1),u:"HZ"},
                {k:"SAMPLE AGE",v:String(overlay.controller.sensorTestAgeMs),u:"MS"}
            ]; delegate: Rectangle { required property var modelData; width: 190*overlay.uiScale; height: 96*overlay.uiScale; color: "#0D1C26"; border.color: theme.border
                    Text { x: 10*overlay.uiScale; y: 9*overlay.uiScale; text: modelData.k; color: theme.secondary; font.pixelSize: 7*overlay.uiScale; font.bold: true }
                    Text { x: 10*overlay.uiScale; y: 34*overlay.uiScale; text: modelData.v; color: overlay.controller.sensorTestFault ? theme.warning : theme.cyan; font.pixelSize: 20*overlay.uiScale; font.bold: true }
                    Text { x: 10*overlay.uiScale; y: 72*overlay.uiScale; text: modelData.u; color: theme.secondary; font.pixelSize: 7*overlay.uiScale }
            } }
        }
        Rectangle { x: 22*overlay.uiScale; y: 205*overlay.uiScale; width: 778*overlay.uiScale; height: 82*overlay.uiScale; color: "#0B1720"; border.color: theme.border
            Text { x: 12*overlay.uiScale; y: 9*overlay.uiScale; text: "LIVE TREND / QUALITY"; color: theme.secondary; font.pixelSize: 8*overlay.uiScale; font.bold: true }
            Rectangle { x: 12*overlay.uiScale; y: 38*overlay.uiScale; width: 750*overlay.uiScale; height: 20*overlay.uiScale; color: "#081119"; border.color: theme.border
                Rectangle { x: 2*overlay.uiScale; y: 2*overlay.uiScale; height: parent.height-4*overlay.uiScale; width: parent.width*(overlay.controller.sensorTestFault ? 0.08 : 0.82); color: overlay.controller.sensorTestFault ? theme.warning : theme.green }
            }
        }
        Text { x: 22*overlay.uiScale; y: 310*overlay.uiScale; text: "CALIBRATION PREVIEW"; color: theme.secondary; font.pixelSize: 8*overlay.uiScale; font.bold: true }
        Rectangle { x: 22*overlay.uiScale; y: 332*overlay.uiScale; width: 150*overlay.uiScale; height: 40*overlay.uiScale; color: "#101E28"; border.color: theme.borderBright
            TextInput { id: testGain; anchors { fill: parent; margins: 8*overlay.uiScale } text: "1.0"; color: theme.primary; font.pixelSize: 11*overlay.uiScale; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            Text { anchors { right: parent.right; rightMargin: 8*overlay.uiScale; verticalCenter: parent.verticalCenter } text: "GAIN"; color: theme.secondary; font.pixelSize: 7*overlay.uiScale }
        }
        Rectangle { x: 182*overlay.uiScale; y: 332*overlay.uiScale; width: 150*overlay.uiScale; height: 40*overlay.uiScale; color: "#101E28"; border.color: theme.borderBright
            TextInput { id: testOffset; anchors { fill: parent; margins: 8*overlay.uiScale } text: "0.0"; color: theme.primary; font.pixelSize: 11*overlay.uiScale; inputMethodHints: Qt.ImhFormattedNumbersOnly }
            Text { anchors { right: parent.right; rightMargin: 8*overlay.uiScale; verticalCenter: parent.verticalCenter } text: "OFFSET"; color: theme.secondary; font.pixelSize: 7*overlay.uiScale }
        }
        Rectangle { x: 342*overlay.uiScale; y: 332*overlay.uiScale; width: 150*overlay.uiScale; height: 40*overlay.uiScale; color: "#102A35"; border.color: theme.cyan
            Text { anchors.centerIn: parent; text: "APPLY PREVIEW"; color: theme.cyan; font.pixelSize: 8*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: overlay.controller.setEngineSensorCalibration(Number(testGain.text),Number(testOffset.text)) }
        }
        Rectangle { x: 22*overlay.uiScale; y: 396*overlay.uiScale; width: 240*overlay.uiScale; height: 46*overlay.uiScale; color: "#123025"; border.color: theme.green
            Text { anchors.centerIn: parent; text: "START BENCH SIM"; color: theme.green; font.pixelSize: 9*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: overlay.controller.startEngineSensorTest(overlay.controller.sensorTestSignal || "RPM",true) }
        }
        Rectangle { x: 274*overlay.uiScale; y: 396*overlay.uiScale; width: 240*overlay.uiScale; height: 46*overlay.uiScale; color: "#2A160F"; border.color: theme.warning
            Text { anchors.centerIn: parent; text: overlay.controller.sensorTestFault ? "CLEAR TIMEOUT" : "INJECT TIMEOUT"; color: theme.warning; font.pixelSize: 9*overlay.uiScale; font.bold: true }
            TapHandler { onTapped: overlay.controller.toggleEngineSensorTestFault() }
        }
        Text { x: 530*overlay.uiScale; y: 398*overlay.uiScale; width: 270*overlay.uiScale; text: "BENCH SIM is synthetic data only. Hardware tests remain WAITING_FOR_PROVIDER until a real adapter is active."; color: theme.amber; font.pixelSize: 7*overlay.uiScale; wrapMode: Text.WordWrap }
    }
    Rectangle { id: footer; anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 46*overlay.uiScale; color: "#07131D"; border.color: theme.borderBright
        Text { x: 14*overlay.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "Hold EFIS NEXT logo 1.2 s to open • validate configuration before flight"; color: theme.secondary; font.pixelSize: 8*overlay.uiScale }
        Rectangle { anchors { right: parent.right; rightMargin: 12*overlay.uiScale; verticalCenter: parent.verticalCenter } width: 190*overlay.uiScale; height: 30*overlay.uiScale; radius: 2*overlay.uiScale; color: overlay.controller.stagingStatus === "READY_TO_APPLY" ? "#123025" : "#10232E"; border.color: overlay.controller.stagingStatus === "READY_TO_APPLY" ? theme.green : theme.amber
            Text { anchors.centerIn: parent; text: overlay.controller.stagingStatus === "READY_TO_APPLY" ? "ACTIVATE PROFILE" : "VERIFY STAGED PROFILE"; color: overlay.controller.stagingStatus === "READY_TO_APPLY" ? theme.green : theme.amber; font.pixelSize: 9*overlay.uiScale; font.bold: true }
            TapHandler { enabled: overlay.stagedSelection !== ""; onTapped: { if (overlay.controller.stagingStatus === "READY_TO_APPLY") overlay.controller.applyStagedAircraft(); else overlay.verificationVisible=true } }
        }
    }

    // Banco prova sensori. Il pannello vive in un file proprio: e' l'unica
    // pagina che mostra dati provenienti dall'hardware invece che dal
    // simulatore, e tenerla separata rende evidente dove passa quel confine.
    SensorBenchPanel {
        visible: overlay.benchVisible
        z: 41
        anchors { left: sidebar.right; right: parent.right; top: header.bottom; bottom: footer.top }
        uiScale: overlay.uiScale
        theme: theme
        onClosed: overlay.benchVisible = false
    }

}
