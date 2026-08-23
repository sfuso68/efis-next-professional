import QtQuick

Rectangle {
    id: root
    required property var chtValues
    required property var egtValues
    property real uiScale: 1
    property bool compact: false
    readonly property color cyan: "#28D9F4"
    readonly property color green: "#52DE36"
    readonly property color amber: "#FFC72B"
    readonly property color warning: "#FF3933"
    color: "#B008131C"; border.color: "#40535E"; radius: 4*uiScale

    Text { x:8*root.uiScale; y:4*root.uiScale; text:"CHT / EGT"; color:root.cyan; font.pixelSize:(root.compact?6:9)*root.uiScale; font.bold:true }
    Row {
        anchors { left:parent.left; right:parent.right; top:parent.top; bottom:parent.bottom; leftMargin:(root.compact?22:34)*root.uiScale; rightMargin:5*root.uiScale; topMargin:(root.compact?16:23)*root.uiScale; bottomMargin:4*root.uiScale }
        spacing:(root.compact?2:4)*root.uiScale
        Repeater {
            model:4
            delegate:Item {
                required property int index
                width:(parent.width-3*parent.spacing)/4; height:parent.height
                Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top } text:"CYL "+(index+1); color:"#EDF3F5"; font.pixelSize:(root.compact?5:7)*root.uiScale; font.bold:true }
                Row { anchors { left:parent.left; right:parent.right; top:parent.top; bottom:parent.bottom; topMargin:(root.compact?12:18)*root.uiScale } spacing:2*root.uiScale
                    Repeater {
                        model:[
                            {n:"CHT",v:Number(root.chtValues[index]),lo:100,hi:260,caution:220,warning:240},
                            {n:"EGT",v:Number(root.egtValues[index]),lo:400,hi:950,caution:850,warning:920}
                        ]
                        delegate:Item {
                            id:bar; required property var modelData
                            width:(parent.width-parent.spacing)/2; height:parent.height
                            readonly property real fraction:Math.max(0,Math.min(1,(modelData.v-modelData.lo)/(modelData.hi-modelData.lo)))
                            readonly property color valueColor:modelData.v>=modelData.warning?root.warning:modelData.v>=modelData.caution?root.amber:root.green
                            Rectangle { id:track; anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:2*root.uiScale } width:(root.compact?8:12)*root.uiScale; height:parent.height-(root.compact?16:22)*root.uiScale; color:"#14242D"; border.color:"#718089"
                                Rectangle { anchors { left:parent.left; right:parent.right; bottom:parent.bottom; margins:1 } height:Math.max(2,(parent.height-2)*bar.fraction); color:bar.valueColor
                                    Behavior on height { NumberAnimation { duration:140; easing.type:Easing.OutCubic } }
                                }
                                Repeater { model:5; delegate:Rectangle { required property int index; x:-3*root.uiScale; y:index*(track.height-1)/4; width:index%2===0?5*root.uiScale:3*root.uiScale; height:1; color:"#DCE5E8" } }
                                Rectangle { x:parent.width+2*root.uiScale; y:parent.height*(1-bar.fraction)-1; width:5*root.uiScale; height:2*root.uiScale; color:"white" }
                            }
                            Text { anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom } text:Math.round(modelData.v); color:bar.valueColor; font.pixelSize:(root.compact?6:8)*root.uiScale; font.bold:true }
                        }
                    }
                }
            }
        }
    }
    Text { x:3*root.uiScale; y:root.height*.30; text:"CHT\n°C"; color:root.cyan; font.pixelSize:(root.compact?5:7)*root.uiScale; horizontalAlignment:Text.AlignHCenter; font.bold:true }
    Text { x:3*root.uiScale; y:root.height*.68; text:"EGT\n°C"; color:root.cyan; font.pixelSize:(root.compact?5:7)*root.uiScale; horizontalAlignment:Text.AlignHCenter; font.bold:true }
}
