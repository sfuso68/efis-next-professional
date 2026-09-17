import QtQuick
pragma ComponentBehavior: Bound

Item {
    id:root
    property real uiScale:1
    property bool compact:false
    function textSize(base, minimum) { return Math.max(compact ? minimum : 0, base*uiScale) }
    property real busVoltage:0
    property real alternatorCurrent:0
    property string dataState:"LOST"
    property int dataAgeMs:-1
    readonly property bool dataValid:dataState!=="LOST"&&dataState!=="FAIL"
    readonly property bool busNormal:busVoltage>=13.2&&busVoltage<=14.8
    readonly property color cyan:"#28D7E8"
    readonly property color normal:"#39D353"
    readonly property color caution:"#FFC928"
    readonly property color invalid:"#8A96A3"
    Text { x:0; y:0; text:"ELECTRICAL"; color:root.cyan; font.pixelSize:root.textSize(8,8); font.bold:true }
    Text { anchors { right:parent.right; top:parent.top } text:root.dataValid?"":root.dataState+((root.dataState==="LOST"&&root.dataAgeMs>=0)?" "+(root.dataAgeMs/1000).toFixed(1)+"s":""); color:root.caution; font.pixelSize:root.textSize(6,8); font.bold:true }
    Row { id:electricalRow; anchors { left:parent.left; right:parent.right; top:parent.top; bottom:parent.bottom; topMargin:18*root.uiScale } spacing:3*root.uiScale
        Repeater { model:[{n:"ALT",v:root.alternatorCurrent.toFixed(0)+" A",ok:root.alternatorCurrent>=0},{n:"MAIN BUS",v:root.busVoltage.toFixed(1)+" V",ok:root.busNormal},{n:"ESS BUS",v:root.busVoltage.toFixed(1)+" V",ok:root.busNormal}]
            delegate:Rectangle { id:busBlock; required property var modelData; width:(electricalRow.width-2*electricalRow.spacing)/3; height:electricalRow.height; radius:3*root.uiScale; color:"#081820"; border.color:!root.dataValid?root.invalid:busBlock.modelData.ok?root.normal:root.caution
                Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:3*root.uiScale } text:busBlock.modelData.n; color:root.cyan; font.pixelSize:root.textSize(5,8); font.bold:true }
                Text { anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom; bottomMargin:3*root.uiScale } text:root.dataValid?busBlock.modelData.v:"—"; color:!root.dataValid?root.invalid:busBlock.modelData.ok?"#F4F8FC":root.caution; font.pixelSize:root.textSize(7,9); font.bold:true }
            }
        }
    }
}
