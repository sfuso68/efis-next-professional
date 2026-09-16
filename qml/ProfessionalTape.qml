import QtQuick

Rectangle {
    id: tape
    required property string role
    required property real value
    required property real selectedValue
    required property real trend
    required property bool valid
    property real verticalSpeed: 0
    property real step: role === "IAS" ? 10 : 100
    property real uiScale: 1
    property bool rightSide: role === "ALT"
    property var speedProfile: ({})
    Theme { id: theme }

    color: "transparent"; border.color: "transparent"; border.width: 0
    clip: false

    function speedY(speed) {
        return tape.height/2-(speed-tape.value)/(tape.step/2)*25*tape.uiScale
    }

    function indicatedValueColor() {
        if (!tape.valid)
            return theme.warning
        if (tape.role !== "IAS" || tape.speedProfile.vne === undefined)
            return theme.primary
        if (tape.value < tape.speedProfile.vso)
            return theme.warning
        if (tape.value < tape.speedProfile.vs1)
            return "#F4F7FA"
        if (tape.value <= tape.speedProfile.vno)
            return theme.green
        if (tape.value < tape.speedProfile.vne)
            return theme.amber
        return theme.warning
    }

    Item { anchors.fill: parent; visible: tape.role === "IAS" && tape.speedProfile.vne !== undefined; clip: true
        Rectangle { x: tape.width-7*tape.uiScale; width: 3*tape.uiScale; color: "white"
            y: Math.min(tape.speedY(tape.speedProfile.vso),tape.speedY(tape.speedProfile.vfe));
            height: Math.abs(tape.speedY(tape.speedProfile.vso)-tape.speedY(tape.speedProfile.vfe)) }
        Rectangle { x: tape.width-13*tape.uiScale; width: 5*tape.uiScale; color: theme.green
            y: Math.min(tape.speedY(tape.speedProfile.vs1),tape.speedY(tape.speedProfile.vno));
            height: Math.abs(tape.speedY(tape.speedProfile.vs1)-tape.speedY(tape.speedProfile.vno)) }
        Rectangle { x: tape.width-13*tape.uiScale; width: 5*tape.uiScale; color: theme.amber
            y: Math.min(tape.speedY(tape.speedProfile.vno),tape.speedY(tape.speedProfile.vne));
            height: Math.abs(tape.speedY(tape.speedProfile.vno)-tape.speedY(tape.speedProfile.vne)) }
        Rectangle { x: tape.width-17*tape.uiScale; width: 14*tape.uiScale; height: 3*tape.uiScale; color: theme.warning; y: tape.speedY(tape.speedProfile.vne)-height/2 }
        Repeater { model: [
            {n:"VA",v:tape.speedProfile.va,c:theme.magenta}, {n:"BG",v:tape.speedProfile.bestGlide,c:theme.green},
            {n:"REF",v:tape.speedProfile.vref,c:"white"}
        ]; delegate: Item { required property var modelData; visible: modelData.v !== undefined
                x: tape.width-48*tape.uiScale; y: tape.speedY(modelData.v)-5*tape.uiScale
                width: 33*tape.uiScale; height: 10*tape.uiScale
                Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 8*tape.uiScale; height: 2*tape.uiScale; color: modelData.c }
                Text { x: 10*tape.uiScale; anchors.verticalCenter: parent.verticalCenter; text: modelData.n; color: modelData.c; font.pixelSize: 6*tape.uiScale; font.bold: true }
        } }
    }

    Repeater {
        model: tape.role === "IAS" ? [
            {n:"VX",v:tape.speedProfile.vx,c:"#00D8FF"},
            {n:"VY",v:tape.speedProfile.vy,c:"#8DFF4F"}
        ] : []
        delegate: Item {
            required property var modelData
            x: tape.width+2*tape.uiScale
            y: tape.speedY(modelData.v)-7*tape.uiScale
            width: 48*tape.uiScale; height: 14*tape.uiScale
            visible: modelData.v !== undefined && y >= 30*tape.uiScale && y+height <= tape.height-24*tape.uiScale
            Rectangle { anchors.verticalCenter: parent.verticalCenter; width: 19*tape.uiScale; height: 3*tape.uiScale; color: modelData.c }
            Text { x: 23*tape.uiScale; anchors.verticalCenter: parent.verticalCenter; text: modelData.n; color: modelData.c; font.pixelSize: 9*tape.uiScale; font.bold: true; style: Text.Outline; styleColor: "#C0000000" }
        }
    }

    Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 30*tape.uiScale; color: "transparent"; border.color: "transparent"; border.width: 0
        Text { x: 9*tape.uiScale; anchors.verticalCenter: parent.verticalCenter; text: tape.role;
            color: theme.primary; font.pixelSize: 15*tape.uiScale; font.bold: true }
        Text { anchors { right: parent.right; rightMargin: 9*tape.uiScale; verticalCenter: parent.verticalCenter }
            text: tape.role === "IAS" ? "KT" : "FT"; color: theme.secondary; font.pixelSize: 9*tape.uiScale; font.bold: true }
    }

    Repeater {
        model: 13
        delegate: Item {
            required property int index
            property real tickValue: Math.round(tape.value/(tape.step/2))*(tape.step/2) - (index-6)*(tape.step/2)
            y: tape.height/2 + (index-6)*25*tape.uiScale; width: tape.width; height: 1
            Rectangle { anchors.right: tape.rightSide ? undefined : parent.right; anchors.left: tape.rightSide ? parent.left : undefined;
                width: (index%2===0 ? 22 : 11)*tape.uiScale; height: (index%2===0 ? 2 : 1)*tape.uiScale;
                color: index%2===0 ? "#E6F0F5" : "#91A8B5" }
            // L'etichetta centrale della scala ripeterebbe il valore che il
            // riquadro mostra gia' in grande, e cadrebbe proprio dove passano il
            // riquadro stesso e la barra di tendenza: sull'ALT traspariva da
            // sotto il riquadro semitrasparente, sull'IAS finiva sotto la barra
            // magenta. Il numero al puntatore lo dice il riquadro; sulla scala
            // servono i riferimenti vicini, non la ripetizione di quello.
            Text { visible: index%2===0 &&
                       Math.abs(parent.y - tape.height/2) > 33*tape.uiScale;
                   anchors.verticalCenter: parent.verticalCenter;
                anchors.right: tape.rightSide ? undefined : parent.right; anchors.left: tape.rightSide ? parent.left : undefined;
                anchors.rightMargin: 24*tape.uiScale; anchors.leftMargin: 24*tape.uiScale;
                text: Math.max(0, Math.round(parent.tickValue)); color: "#F4F7FA"; font.pixelSize: 12*tape.uiScale; font.bold: true;
                style: Text.Outline; styleColor: "#B0000000" }
        }
    }

    Rectangle {
        y: Math.max(38*tape.uiScale, Math.min(tape.height-28*tape.uiScale,
           tape.height/2-(tape.selectedValue-tape.value)/(tape.step/2)*25*tape.uiScale))
        anchors.right: tape.rightSide ? undefined : parent.right
        anchors.left: tape.rightSide ? parent.left : undefined
        width: 18*tape.uiScale; height: 10*tape.uiScale; color: theme.cyan
        Text { anchors.centerIn: parent; text: "◆"; color: theme.background; font.pixelSize: 7*tape.uiScale }
    }

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: tape.rightSide ? parent.right : undefined
        anchors.left: tape.rightSide ? undefined : parent.left
        // ALT e VS devono restare due letture distinte anche sul 1024x600: una
        // fascia vuota separa i due riquadri invece di farli sembrare un unico
        // numero composto.
        anchors.rightMargin: tape.rightSide ? 45*tape.uiScale : 0
        anchors.leftMargin: tape.rightSide ? 0 : 5*tape.uiScale
        width: (tape.role === "IAS" ? 66 : 76)*tape.uiScale; height: 50*tape.uiScale
        color: "#A0061018"; border.color: "transparent"; border.width: 0; radius: 2*tape.uiScale
        Text { anchors.centerIn: parent; text: tape.valid ? Math.round(tape.value) : "---";
            color: tape.indicatedValueColor(); font.pixelSize: (tape.role === "IAS" ? 30 : 27)*tape.uiScale; font.bold: true;
            style: Text.Outline; styleColor: "#C0000000" }
    }

    Canvas {
        anchors.verticalCenter: parent.verticalCenter
        x: tape.rightSide ? tape.width-124*tape.uiScale : 71*tape.uiScale
        width: 9*tape.uiScale; height: 18*tape.uiScale
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0,0,width,height)
            ctx.fillStyle = "#A0061018"
            ctx.beginPath()
            if (tape.rightSide) {
                ctx.moveTo(width,0); ctx.lineTo(0,height/2); ctx.lineTo(width,height)
            } else {
                ctx.moveTo(0,0); ctx.lineTo(width,height/2); ctx.lineTo(0,height)
            }
            ctx.closePath(); ctx.fill()
        }
    }

    Rectangle {
        visible: tape.valid && Math.abs(tape.trend)>0.1; width: 3*tape.uiScale
        height: Math.min(80*tape.uiScale, Math.max(8*tape.uiScale, Math.abs(tape.trend/tape.step)*50*tape.uiScale))
        // Accanto alla scala, non sopra il riquadro del valore. Sul nastro
        // altitudine la barra cadeva dentro il riquadro e tagliava in due la
        // prima cifra della quota, che e' il numero che deve restare leggibile
        // per primo. Il vettore di tendenza appartiene alla scala: e' li' che
        // dice qualcosa, sovrapposto alle cifre toglie soltanto.
        x: tape.rightSide ? 7*tape.uiScale : tape.width-32*tape.uiScale
        y: tape.height/2 - (tape.trend>0 ? height : 0); color: theme.magenta
        Rectangle { width: 11*tape.uiScale; height: 3*tape.uiScale; anchors.horizontalCenter: parent.horizontalCenter;
            anchors.top: tape.trend>0 ? parent.top : undefined; anchors.bottom: tape.trend<=0 ? parent.bottom : undefined; color: theme.magenta }
    }

    Rectangle { visible: tape.rightSide; anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
        width: 31*tape.uiScale; color: "transparent"; border.color: "transparent"
        Rectangle { anchors { left: parent.left; top: parent.top; bottom: parent.bottom } width: 1*tape.uiScale; color: "#7A91A1" }
        Text { anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 8*tape.uiScale } text: "VS"; color: theme.secondary; font.pixelSize: 7*tape.uiScale; font.bold: true }
        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 36*tape.uiScale;
            color: "#F003080D"; border.color: theme.borderBright
            Text { anchors.centerIn: parent; text: Math.round(tape.verticalSpeed/100)*100; color: theme.primary; font.pixelSize: 10*tape.uiScale; font.bold: true }
        }
    }

    Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 24*tape.uiScale; color: "transparent"; border.color: "transparent"
        Text { anchors.centerIn: parent; text: tape.valid ? "SIM • TREND 6S" : "DATA INVALID";
            color: tape.valid ? theme.cyan : theme.warning; font.pixelSize: 7*tape.uiScale; font.bold: true }
    }
}
