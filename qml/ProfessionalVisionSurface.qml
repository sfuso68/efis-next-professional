import QtQuick
import EfisNext.Native

Rectangle {
    id: vision
    required property real roll
    required property real pitch
    required property real heading
    required property real latitude
    required property real longitude
    required property real altitude
    required property real groundSpeed
    required property real selectedCourse
    required property real commandedRoll
    required property real commandedPitch
    required property bool flightDirectorActive
    required property bool flightDirectorGuidanceValid
    required property bool attitudeValid
    required property bool headingValid
    required property bool altitudeValid
    required property bool positionValid
    required property bool groundSpeedValid
    required property bool selectedCourseValid
    required property bool navigationValid
    // Larghezza degli strumenti che coprono i bordi della superficie. La
    // superficie si estende sotto i nastri, quindi senza questa misura le sue
    // targhette finiscono sopra di essi: e' cosi' che la targhetta imagery
    // copriva l'etichetta IAS e la sua unita'.
    property real leftInstrumentInset: 0
    property real rightInstrumentInset: 0

    property string imageryState: "NO PACKAGE"
    property bool imageryOfflineReady: false
    property int imageryTileCount: 0
    property int imageryCoverageGaps: 0
    property var selectedAirport: ({})
    property real uiScale: 1
    signal navigationRequested()
    function distanceNm() {
        if (!positionValid || selectedAirport.latitude_deg === undefined || selectedAirport.longitude_deg === undefined)
            return NaN
        const targetLat=selectedAirport.latitude_deg === undefined ? 45.6484 : selectedAirport.latitude_deg
        const targetLon=selectedAirport.longitude_deg === undefined ? 12.1944 : selectedAirport.longitude_deg
        const lat1=latitude*Math.PI/180, lat2=targetLat*Math.PI/180
        const dLat=lat2-lat1, dLon=(targetLon-longitude)*Math.PI/180
        const a=Math.sin(dLat/2)*Math.sin(dLat/2)+Math.cos(lat1)*Math.cos(lat2)*Math.sin(dLon/2)*Math.sin(dLon/2)
        return 3440.065*2*Math.atan2(Math.sqrt(a),Math.sqrt(1-a))
    }
    function eteText() {
        if (!positionValid || !groundSpeedValid || groundSpeed <= 1 || !isFinite(distanceNm()))
            return "--:--"
        const minutes=Math.round(distanceNm()/groundSpeed*60)
        return ("0"+Math.floor(minutes/60)).slice(-2)+":"+("0"+(minutes%60)).slice(-2)
    }
    Theme { id: theme }
    color: "#155D91"; clip: true; border.color: theme.border

    Terrain3DSurface {
        anchors.fill: parent
        visible: vision.attitudeValid && vision.headingValid && vision.positionValid && vision.altitudeValid
        roll: vision.attitudeValid ? vision.roll : 0
        pitch: vision.attitudeValid ? vision.pitch : 0
        heading: vision.headingValid ? vision.heading : 0
        latitude: vision.positionValid ? vision.latitude : 0
        longitude: vision.positionValid ? vision.longitude : 0
        altitude: vision.altitude
    }

    Rectangle {
        z: 35
        visible: !vision.attitudeValid
        anchors.centerIn: parent
        width: 244*vision.uiScale; height: 60*vision.uiScale; radius: 5*vision.uiScale
        color: "#E02A1014"; border.color: theme.warning; border.width: 3*vision.uiScale
        Text {
            anchors.centerIn: parent; text: "ATTITUDE INVALID"
            color: theme.warning; font.pixelSize: 17*vision.uiScale; font.bold: true
        }
    }

    Text {
        z: 34
        visible: vision.attitudeValid && !(vision.headingValid && vision.positionValid && vision.altitudeValid)
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 72*vision.uiScale }
        text: "SVS DATA INVALID"; color: theme.amber
        font.pixelSize: 10*vision.uiScale; font.bold: true
    }

    // Targhette di stato della superficie, raccolte in una colonna sola.
    //
    // Stanno a destra dei nastri e sotto l'arco di prua: sono le uniche due
    // fasce dove non coprono uno strumento. Prima erano collocate a coordinate
    // fisse dal bordo, che e' il bordo della superficie e non quello dell'area
    // libera, e finivano sopra il nastro velocita'.
    Column {
        z: 30
        spacing: 6*vision.uiScale
        anchors {
            left: parent.left
            top: parent.top
            leftMargin: vision.leftInstrumentInset + 8*vision.uiScale
            topMargin: headingArc.height + 6*vision.uiScale
        }

        Rectangle {
            width: 142*vision.uiScale; height: 28*vision.uiScale; radius: 3*vision.uiScale
            color: "#A8050D14"; border.color: vision.imageryOfflineReady ? theme.green : theme.amber
            Column { anchors.centerIn: parent; spacing: 0
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: vision.imageryOfflineReady ? "IMAGERY • OFFLINE READY" : "IMAGERY • "+vision.imageryState; color: vision.imageryOfflineReady ? theme.green : theme.amber; font.pixelSize: 7*vision.uiScale; font.bold: true }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: vision.imageryTileCount+" TILES"+(vision.imageryCoverageGaps>0 ? " • "+vision.imageryCoverageGaps+" GAPS" : ""); color: theme.secondary; font.pixelSize: 6*vision.uiScale }
            }
        }

        Rectangle {
            width: 151*vision.uiScale; height: 35*vision.uiScale; radius: 3*vision.uiScale
            color: "#9A050D14"; border.color: theme.borderBright
            Text { x: 7*vision.uiScale; y: 5*vision.uiScale; text: "QUICK 3D • REAL DEM • TAWS"; color: theme.cyan; font.pixelSize: 7*vision.uiScale; font.bold: true }
            Text { x: 7*vision.uiScale; y: 19*vision.uiScale; text: "COP-DEM GLO-30 • N46E012 • GPU • NON-CERT"; color: theme.amber; font.pixelSize: 6*vision.uiScale; font.bold: true }
        }
    }

    // Transparent, finely graduated upper heading arc.
    Item { id: headingArc; visible: vision.headingValid; anchors { horizontalCenter: parent.horizontalCenter; top: parent.top } width: parent.width*.68; height: 66*vision.uiScale
        Repeater { model: 48; delegate: Item { required property int index; anchors.fill: parent
                property real d1: (index-24)*2.5; property real d2: d1+2.5
                property real x1: parent.width/2+d1*parent.width/130; property real x2: parent.width/2+d2*parent.width/130
                property real y1: 30*vision.uiScale+Math.pow(d1/60,2)*10*vision.uiScale
                property real y2: 30*vision.uiScale+Math.pow(d2/60,2)*10*vision.uiScale
                Rectangle { x: parent.x1; y: parent.y1; width: Math.sqrt(Math.pow(parent.x2-parent.x1,2)+Math.pow(parent.y2-parent.y1,2)); height: 1.2*vision.uiScale
                    rotation: Math.atan2(parent.y2-parent.y1,parent.x2-parent.x1)*180/Math.PI; transformOrigin: Item.Left; color: "#D6E5E9"; opacity: .88 }
        } }
        Repeater { model: 49; delegate: Item { required property int index; property real delta: (index-24)*2.5
                x: parent.width/2+delta*parent.width/130; y: 30*vision.uiScale+Math.pow(delta/60,2)*10*vision.uiScale; width: 1; height: 24*vision.uiScale
                Rectangle { anchors.horizontalCenter: parent.horizontalCenter; width: 1.2*vision.uiScale; height: (index%2===0?10:5)*vision.uiScale; color: theme.primary; rotation: delta*.10 }
                Text { visible: index%4===0; anchors.horizontalCenter: parent.horizontalCenter; y: 11*vision.uiScale
                    text: ("000"+Math.round((vision.heading+parent.delta+360)%360)).slice(-3); color: theme.primary; font.pixelSize: 8*vision.uiScale; font.bold: true }
        } }
        Rectangle { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 1*vision.uiScale } width: 58*vision.uiScale; height: 23*vision.uiScale; radius: 3*vision.uiScale
            color: "#B003080D"; border.color: theme.primary
            Text { anchors.centerIn: parent; text: ("000"+Math.round(vision.heading)).slice(-3)+"°"; color: theme.primary; font.pixelSize: 13*vision.uiScale; font.bold: true } }
        Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 22*vision.uiScale } text: "▼"; color: theme.cyan; font.pixelSize: 12*vision.uiScale; font.bold: true }
        Item { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 49*vision.uiScale } width: 162*vision.uiScale; height: 17*vision.uiScale
            Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 3*vision.uiScale; color: theme.cyan }
            Rectangle { anchors { left: parent.left; top: parent.top } width: 3*vision.uiScale; height: 9*vision.uiScale; color: theme.cyan }
            Rectangle { anchors { right: parent.right; top: parent.top } width: 3*vision.uiScale; height: 9*vision.uiScale; color: theme.cyan }
            Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: -3*vision.uiScale } text: "▲"; color: theme.cyan; font.pixelSize: 17*vision.uiScale; font.bold: true }
        }
    }

    // Pitch ladder remains screen-readable while following attitude.
    Item { visible: vision.attitudeValid; anchors.fill: parent; rotation: vision.attitudeValid ? -vision.roll : 0; transform: Translate { y: vision.attitudeValid ? vision.pitch*vision.height/75 : 0 }
        Repeater { model: [-20,-15,-10,-5,5,10,15,20]; delegate: Item { required property int modelData;
                x: parent.width/2-width/2; y: parent.height*0.46-modelData*vision.height/75;
                width: (Math.abs(modelData)%10===0 ? 96 : 56)*vision.uiScale; height: 2
                Rectangle { anchors.fill: parent; color: "#EAF1F2" }
                Text { anchors { right: parent.left; rightMargin: 6*vision.uiScale; verticalCenter: parent.verticalCenter } text: Math.abs(modelData); color: theme.primary; font.pixelSize: 9*vision.uiScale; font.bold: true }
                Text { anchors { left: parent.right; leftMargin: 6*vision.uiScale; verticalCenter: parent.verticalCenter } text: Math.abs(modelData); color: theme.primary; font.pixelSize: 9*vision.uiScale; font.bold: true }
        } }
    }

    Row { visible: vision.attitudeValid; anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 54*vision.uiScale } spacing: 16*vision.uiScale
        Repeater { model: ["30","20","10","▲","10","20","30"]; delegate: Text { required property string modelData;
                text: modelData; color: modelData==="▲" ? theme.cyan : theme.primary; font.pixelSize: (modelData==="▲" ? 12 : 7)*vision.uiScale; font.bold: true }
        } }

    // Aircraft reference and flight director.
    Item { anchors.centerIn: parent; y: -12*vision.uiScale; width: 214*vision.uiScale; height: 56*vision.uiScale
        Rectangle { x: 0; y: 27*vision.uiScale; width: 78*vision.uiScale; height: 4*vision.uiScale; color: "#F2D13D" }
        Rectangle { x: 136*vision.uiScale; y: 27*vision.uiScale; width: 78*vision.uiScale; height: 4*vision.uiScale; color: "#F2D13D" }
        Rectangle { anchors.centerIn: parent; width: 12*vision.uiScale; height: 12*vision.uiScale; radius: width/2; color: "transparent"; border.color: "#F2D13D"; border.width: 3 }
        Row { visible: vision.flightDirectorActive && vision.flightDirectorGuidanceValid && vision.attitudeValid; anchors.centerIn: parent; spacing: 41*vision.uiScale; y: visible ? (vision.commandedPitch-vision.pitch)*-3*vision.uiScale : 0
            Rectangle { width: 58*vision.uiScale; height: 5*vision.uiScale; color: theme.magenta }
            Rectangle { width: 58*vision.uiScale; height: 5*vision.uiScale; color: theme.magenta }
        }
    }

    // Flight-path vector
    Item { visible: vision.attitudeValid && vision.positionValid; x: parent.width/2-14*vision.uiScale; y: parent.height*0.42; width: 28*vision.uiScale; height: 28*vision.uiScale
        Rectangle { anchors.centerIn: parent; width: 13*vision.uiScale; height: 13*vision.uiScale; radius: width/2; color: "transparent"; border.color: theme.green; border.width: 2 }
        Rectangle { anchors { left: parent.left; verticalCenter: parent.verticalCenter } width: 8*vision.uiScale; height: 2*vision.uiScale; color: theme.green }
        Rectangle { anchors { right: parent.right; verticalCenter: parent.verticalCenter } width: 8*vision.uiScale; height: 2*vision.uiScale; color: theme.green }
        Rectangle { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top } width: 2*vision.uiScale; height: 8*vision.uiScale; color: theme.green }
    }

    // Compact lower HSI: one hierarchy for heading, course and aircraft symbol.
    Item { anchors.fill: parent; clip: true
        Item { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 125*vision.uiScale; z: 20
            TapHandler { acceptedButtons: Qt.LeftButton; onTapped: vision.navigationRequested() }
        }
        Repeater { model: 8; delegate: Text { required property int index
                visible: vision.headingValid
                property real compassAngle: (235+index*10)*Math.PI/180
                property real compassRadius: vision.width*.402
                x: vision.width*.5 + Math.cos(compassAngle)*compassRadius - width/2
                y: vision.height*.96+vision.width*.4257*.81915204 + Math.sin(compassAngle)*compassRadius - height/2
                text: vision.headingValid ? ("000"+Math.round((vision.heading-35+index*10+360)%360)).slice(-3) : "---"
                color: (index===3||index===4) ? theme.primary : "#C9D6DA"; font.pixelSize: 8*vision.uiScale; font.bold: true
        } }
        Rectangle { visible: vision.selectedCourseValid; anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 17*vision.uiScale }
            width: 2*vision.uiScale; height: 52*vision.uiScale; color: theme.magenta }
        Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 4*vision.uiScale }
            text: "▲"; color: theme.primary; font.pixelSize: 15*vision.uiScale; font.bold: true }
        Item { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 7*vision.uiScale }
            width: 292*vision.uiScale; height: 36*vision.uiScale
            Column { anchors.left: parent.left; spacing: 1*vision.uiScale
                Text { text: "GPS1 • SIM"; color: theme.magenta; font.pixelSize: 8*vision.uiScale; font.bold: true }
                Text { text: "DTK  "+(vision.selectedCourseValid ? Math.round(vision.selectedCourse)+"°" : "---"); color: vision.selectedCourseValid ? theme.primary : theme.amber; font.pixelSize: 7*vision.uiScale; font.bold: true }
                Text { text: "DIS  "+(vision.positionValid && isFinite(vision.distanceNm()) ? vision.distanceNm().toFixed(1)+" NM" : "---"); color: vision.positionValid ? theme.cyan : theme.amber; font.pixelSize: 7*vision.uiScale; font.bold: true }
            }
            Column { anchors.right: parent.right; spacing: 1*vision.uiScale
                Text { anchors.right: parent.right; text: vision.selectedAirport.icao || "----"; color: theme.magenta; font.pixelSize: 8*vision.uiScale; font.bold: true }
                Text { anchors.right: parent.right; text: "ETE  "+vision.eteText(); color: theme.primary; font.pixelSize: 7*vision.uiScale; font.bold: true }
                Text { anchors.right: parent.right; text: vision.navigationValid?"CDI VALID":"NAV INVALID"; color: vision.navigationValid?theme.green:theme.amber; font.pixelSize: 7*vision.uiScale; font.bold: true }
            }
        }
        Item { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 6*vision.uiScale } width: 76*vision.uiScale; height: 12*vision.uiScale
            Rectangle { anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter } height: 1*vision.uiScale; color: theme.cyan }
            Rectangle { anchors.left: parent.left; width: 1*vision.uiScale; height: 7*vision.uiScale; color: theme.cyan }
            Rectangle { anchors.right: parent.right; width: 1*vision.uiScale; height: 7*vision.uiScale; color: theme.cyan }
            Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.top } text: "2.5 NM"; color: theme.cyan; font.pixelSize: 6*vision.uiScale; font.bold: true }
        }
    }

}
