import QtQuick
pragma ComponentBehavior: Bound

Item {
    id:root
    property real uiScale:1
    property bool compact:false
    function textSize(base, minimum) { return Math.max(compact ? minimum : 0, base*uiScale) }
    property real leftQuantity:0
    property real rightQuantity:0
    property real tankCapacity:20
    property real reserve:0
    property string unit:"GAL"
    property string dataState:"LOST"
    readonly property real total:Math.max(0,leftQuantity)+Math.max(0,rightQuantity)
    readonly property color cyan:"#28D7E8"
    readonly property color normal:"#39D353"
    readonly property color caution:"#FFC928"
    readonly property color warning:"#FF3B30"
    readonly property bool dataValid:dataState!=="LOST"&&dataState!=="FAIL"
    Text { x:0; y:0; text:"FUEL"; color:root.cyan; font.pixelSize:root.textSize(8,8); font.bold:true }
    Text { anchors { right:parent.right; top:parent.top } text:root.dataValid?root.total.toFixed(1)+" "+root.unit:"—"; color:root.dataValid?(root.total<=root.reserve?root.warning:"#F4F8FC"):"#8A96A3"; font.pixelSize:root.textSize(12,11); font.bold:true }
    Row { id:tankRow; anchors { left:parent.left; right:parent.right; bottom:parent.bottom } height:parent.height-23*root.uiScale; spacing:8*root.uiScale
        Repeater { model:[{n:"LEFT",v:root.leftQuantity},{n:"RIGHT",v:root.rightQuantity}]
            delegate:Item { id:tank; required property var modelData; width:(tankRow.width-tankRow.spacing)/2; height:tankRow.height
                Text { text:tank.modelData.n+"  "+(root.dataValid?Number(tank.modelData.v).toFixed(1):"—"); color:"#DDE7EA"; font.pixelSize:root.textSize(6,8); font.bold:true }
                Rectangle { anchors { left:parent.left; right:parent.right; bottom:parent.bottom } height:10*root.uiScale; color:"#142630"; border.color:"#60727B"
                    Rectangle { anchors { left:parent.left; top:parent.top; bottom:parent.bottom; margins:1 } width:(parent.width-2)*Math.max(0,Math.min(1,Number(tank.modelData.v)/root.tankCapacity)); color:!root.dataValid?"#8A96A3":tank.modelData.v<4?root.warning:tank.modelData.v<7?root.caution:root.normal }
                }
            }
        }
    }
}
