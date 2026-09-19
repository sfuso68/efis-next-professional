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
    property int dataAgeMs: -1
    readonly property color cyan:"#28D7E8"
    readonly property color normal:"#39D353"
    readonly property color caution:"#FFC928"
    readonly property color warning:"#FF3B30"
    readonly property color invalid:"#8A96A3"
    readonly property bool dataValid:dataState!=="LOST"&&dataState!=="FAIL"
    readonly property bool dataDegraded:dataState==="STALE"||dataState==="DEGRADED"
    Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top } text:"AIRFRAME"; color:root.cyan; font.pixelSize:root.textSize(8,8); font.bold:true }
    Text { anchors { right:parent.right; top:parent.top } text:root.dataValid?"":root.dataState+((root.dataState==="LOST"&&root.dataAgeMs>=0)?" "+(root.dataAgeMs/1000).toFixed(1)+"s":""); color:root.warning; font.pixelSize:root.textSize(6,8); font.bold:true }
    Item { anchors { left:parent.left; top:parent.top; bottom:parent.bottom; topMargin:17*root.uiScale } width:parent.width*.55
        Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top } text:"GEAR"; color:root.cyan; font.pixelSize:root.textSize(7,8); font.bold:true }
        EmsVectorIcon { visible:root.gearInstalled; anchors.centerIn:parent; anchors.verticalCenterOffset:-3*root.uiScale; width:55*root.uiScale; height:43*root.uiScale; kind:"airplane"; strokeColor:root.dataValid?"#EDF3F5":root.invalid }
        Row { visible:root.gearInstalled; anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:17*root.uiScale } spacing:8*root.uiScale
            Repeater { model:[{n:"N",ok:root.noseLocked},{n:"L",ok:root.leftLocked},{n:"R",ok:root.rightLocked}]
                delegate:Column { id:gearLeg; required property var modelData; spacing:1*root.uiScale
                    Rectangle { width:13*root.uiScale; height:width; radius:width/2; color:gearLeg.modelData.ok&&root.dataValid&&!root.dataDegraded?"#0A2A14":"#351317"; border.width:2; border.color:!root.dataValid?root.invalid:root.dataDegraded?root.caution:gearLeg.modelData.ok?root.normal:root.warning
                        Rectangle { anchors.centerIn:parent; width:4*root.uiScale; height:width; radius:width/2; color:parent.border.color } }
                    Text { anchors.horizontalCenter:parent.horizontalCenter; text:gearLeg.modelData.n; color:"#DDE7EA"; font.pixelSize:root.textSize(5,8); font.bold:true }
                }
            }
        }
        Text { anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:2*root.uiScale } text:root.gearInstalled?root.gearState:"FIXED / N.A."; color:!root.dataValid?root.invalid:root.dataDegraded?root.caution:root.gearState==="DOWN & LOCKED"?root.normal:root.warning; font.pixelSize:root.textSize(6,8); font.bold:true }
    }
    Rectangle { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; bottom:parent.bottom; topMargin:19*root.uiScale } width:1; color:"#314650" }
    Item { anchors { right:parent.right; top:parent.top; bottom:parent.bottom; topMargin:17*root.uiScale } width:parent.width*.43
        Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top } text:"FLAPS"; color:root.cyan; font.pixelSize:root.textSize(7,8); font.bold:true }
        Text { anchors { left:parent.left; verticalCenter:parent.verticalCenter; leftMargin:2*root.uiScale } text:root.flapsInstalled?Math.round(root.flapAngle)+"°":"N/A"; color:root.dataValid?"#F4F8FC":root.invalid; font.pixelSize:root.textSize(16,14); font.bold:true }
        Rectangle { id:rail; visible:root.flapsInstalled; x:parent.width*.58; y:18*root.uiScale; width:6*root.uiScale; height:parent.height-24*root.uiScale; radius:3*root.uiScale; color:"#233943"
            Rectangle { anchors { left:parent.left; right:parent.right; top:parent.top } height:parent.height*Math.max(0,Math.min(1,root.flapAngle/30)); radius:3*root.uiScale; color:!root.dataValid?root.invalid:root.dataDegraded?root.caution:root.cyan }
            Rectangle { x:-5*root.uiScale; y:parent.height*Math.max(0,Math.min(1,root.flapAngle/30))-4*root.uiScale; width:16*root.uiScale; height:8*root.uiScale; rotation:45; color:!root.dataValid?root.invalid:root.dataDegraded?root.caution:root.cyan; border.color:"white" }
            Rectangle { x:-8*root.uiScale; y:parent.height*Math.max(0,Math.min(1,root.flapCommandedAngle/30))-1; width:22*root.uiScale; height:2; color:"white" }
        }
        Repeater { model:root.flapDetents; delegate:Text { required property real modelData; x:rail.x+13*root.uiScale; y:rail.y+rail.height*modelData/30-4*root.uiScale; text:modelData+"°"; color:Math.abs(modelData-root.flapAngle)<1?(root.dataDegraded?root.caution:root.cyan):"#B7C5CB"; font.pixelSize:root.textSize(5,8); font.bold:Math.abs(modelData-root.flapAngle)<1 } }
    }
}
