import QtQuick
pragma ComponentBehavior: Bound

Item {
    id: root
    property real uiScale: 1
    property bool compact: false
    function textSize(base, minimum) { return Math.max(compact ? minimum : 0, base*uiScale) }
    property bool gearInstalled: false
    property bool noseLocked: false
    property bool leftLocked: false
    property bool rightLocked: false
    property string gearState: "NOT INSTALLED"
    property bool flapsInstalled: false
    property var flapDetents: [0,10,15,20,30]
    property real flapAngle: 0
    property real flapCommandedAngle: 0
    property string dataState: "LOST"
    property real dataAgeMs: -1
    // Independent airframe source: engine EDP health cannot validate gear/flaps.
    property bool gearValid: false
    property bool flapAngleValid: false
    property bool flapCommandedValid: false
    readonly property color cyan:"#28D7E8"
    readonly property color normal:"#39D353"
    readonly property color caution:"#FFC928"
    readonly property color warning:"#FF3B30"
    readonly property color invalid:"#8A96A3"
    readonly property bool dataValid:["LIVE","SIM","SIM FORCED","DEGRADED"].indexOf(dataState)>=0 && isFinite(dataAgeMs) && dataAgeMs>=0
    readonly property bool gearUsable:gearInstalled && dataValid && gearValid
    readonly property bool scaleValid: {
        if (!flapDetents || typeof flapDetents.length!=="number" || flapDetents.length<2 || flapDetents.length>32) return false
        for (var i=0;i<flapDetents.length;++i)
            if (typeof flapDetents[i]!=="number" || !isFinite(flapDetents[i]) ||
                (i>0 && flapDetents[i]<=flapDetents[i-1])) return false
        return true
    }
    function inScale(value) { return scaleValid && isFinite(value) && value>=flapDetents[0] && value<=flapDetents[flapDetents.length-1] }
    function fraction(value) { return inScale(value) ? (value-flapDetents[0])/(flapDetents[flapDetents.length-1]-flapDetents[0]) : 0 }
    readonly property bool flapUsable:flapsInstalled && dataValid && flapAngleValid && inScale(flapAngle)
    readonly property bool commandUsable:flapsInstalled && dataValid && flapCommandedValid && inScale(flapCommandedAngle)
    readonly property bool dataDegraded:dataState==="STALE"||dataState==="DEGRADED"
    Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top } text:"AIRFRAME"; color:root.cyan; font.pixelSize:root.textSize(8,8); font.bold:true }
    Text { objectName:"airframeSourceLabel"; anchors { right:parent.right; top:parent.top } text:["LIVE","SIM","SIM FORCED","STALE","DEGRADED","LOST","FAIL"].indexOf(root.dataState)>=0?root.dataState:"NO DATA"; color:root.dataValid?root.caution:root.warning; font.pixelSize:root.textSize(6,8); font.bold:true }
    Item { anchors { left:parent.left; top:parent.top; bottom:parent.bottom; topMargin:17*root.uiScale } width:parent.width*.55
        Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top } text:"GEAR"; color:root.cyan; font.pixelSize:root.textSize(7,8); font.bold:true }
        EmsVectorIcon { visible:root.gearInstalled; anchors.centerIn:parent; anchors.verticalCenterOffset:-3*root.uiScale; width:55*root.uiScale; height:43*root.uiScale; kind:"airplane"; strokeColor:root.dataValid?"#EDF3F5":root.invalid }
        Row { visible:root.gearInstalled; anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:17*root.uiScale } spacing:8*root.uiScale
            Repeater { model:[{n:"N",ok:root.noseLocked},{n:"L",ok:root.leftLocked},{n:"R",ok:root.rightLocked}]
                delegate:Column { id:gearLeg; required property var modelData; spacing:1*root.uiScale
                    Rectangle { width:13*root.uiScale; height:width; radius:width/2; color:!root.gearUsable?"#233943":gearLeg.modelData.ok&&!root.dataDegraded?"#0A2A14":"#351317"; border.width:2; border.color:!root.gearUsable?root.invalid:root.dataDegraded?root.caution:gearLeg.modelData.ok?root.normal:root.warning
                        Rectangle { anchors.centerIn:parent; width:4*root.uiScale; height:width; radius:width/2; color:parent.border.color } }
                    Text { anchors.horizontalCenter:parent.horizontalCenter; text:gearLeg.modelData.n; color:"#DDE7EA"; font.pixelSize:root.textSize(5,8); font.bold:true }
                }
            }
        }
        Text { objectName:"airframeGearValue"; anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:2*root.uiScale } text:!root.gearInstalled?"N/A":!root.gearUsable?"NO DATA":root.gearState; color:!root.gearUsable?root.invalid:root.dataDegraded?root.caution:root.gearState==="DOWN & LOCKED"?root.normal:root.warning; font.pixelSize:root.textSize(6,8); font.bold:true }
    }
    Rectangle { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; bottom:parent.bottom; topMargin:19*root.uiScale } width:1; color:"#314650" }
    Item { anchors { right:parent.right; top:parent.top; bottom:parent.bottom; topMargin:17*root.uiScale } width:parent.width*.43
        Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top } text:"FLAPS"; color:root.cyan; font.pixelSize:root.textSize(7,8); font.bold:true }
        Text { objectName:"airframeFlapValue"; anchors { left:parent.left; verticalCenter:parent.verticalCenter; leftMargin:2*root.uiScale } text:!root.flapsInstalled?"N/A":root.flapUsable?Math.round(root.flapAngle)+"°":"—"; color:root.flapUsable?"#F4F8FC":root.invalid; font.pixelSize:root.textSize(16,14); font.bold:true }
        Rectangle { id:rail; visible:root.flapsInstalled; x:parent.width*.58; y:18*root.uiScale; width:6*root.uiScale; height:parent.height-24*root.uiScale; radius:3*root.uiScale; color:"#233943"
            Rectangle { visible:root.flapUsable; anchors { left:parent.left; right:parent.right; top:parent.top } height:parent.height*root.fraction(root.flapAngle); radius:3*root.uiScale; color:root.dataDegraded?root.caution:root.cyan }
            Rectangle { objectName:"airframeMeasuredMarker"; visible:root.flapUsable; x:-5*root.uiScale; y:parent.height*root.fraction(root.flapAngle)-4*root.uiScale; width:16*root.uiScale; height:8*root.uiScale; rotation:45; color:root.dataDegraded?root.caution:root.cyan; border.color:"white" }
            Rectangle { objectName:"airframeCommandMarker"; visible:root.commandUsable; x:-8*root.uiScale; y:parent.height*root.fraction(root.flapCommandedAngle)-1; width:22*root.uiScale; height:2; color:"white" }
        }
        Repeater { model:root.scaleValid?root.flapDetents:[]; delegate:Text { required property real modelData; x:rail.x+13*root.uiScale; y:rail.y+rail.height*root.fraction(modelData)-4*root.uiScale; text:modelData+"°"; color:root.flapUsable&&Math.abs(modelData-root.flapAngle)<1?(root.dataDegraded?root.caution:root.cyan):"#B7C5CB"; font.pixelSize:root.textSize(5,8); font.bold:root.flapUsable&&Math.abs(modelData-root.flapAngle)<1 } }
    }
}
