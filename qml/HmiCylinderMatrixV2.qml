import QtQuick
pragma ComponentBehavior: Bound

Rectangle {
    id: root
    required property var chtValues
    required property var egtValues
    property real uiScale: 1
    property string temperatureUnit: "°C"
    property real chtMaximumC: 230
    property real egtMaximumC: 920
    property string dataState: "LOST"
    property bool limitsVerified: false
    readonly property color cyan: "#28D7E8"
    readonly property color normal: "#39D353"
    readonly property color caution: "#FFC928"
    readonly property color warning: "#FF3B30"
    readonly property color invalid: "#8A96A3"
    function displayTemperature(celsius) { return temperatureUnit === "°F" ? celsius*9/5+32 : celsius }
    function valid(values, index) { return index < values.length && isFinite(Number(values[index])) }
    function hottest(values) {
        var result=-1; var hottestIndex=-1
        for (var i=0;i<values.length;i++) if (valid(values,i) && Number(values[i])>result) { result=Number(values[i]); hottestIndex=i }
        return hottestIndex
    }
    function spread(values) {
        var low=Number.POSITIVE_INFINITY; var high=Number.NEGATIVE_INFINITY
        for (var i=0;i<values.length;i++) if (valid(values,i)) { low=Math.min(low,Number(values[i])); high=Math.max(high,Number(values[i])) }
        return isFinite(low) && isFinite(high) ? high-low : 0
    }
    readonly property int hottestCht: hottest(chtValues)
    readonly property int hottestEgt: hottest(egtValues)
    color: "#B008131C"; border.color: "#40535E"; radius: 5*uiScale

    Text { x:9*root.uiScale; y:5*root.uiScale; text:"CHT / EGT  •  4 CYLINDER MATRIX"; color:root.cyan; font.pixelSize:8*root.uiScale; font.bold:true }
    Text { anchors { right:parent.right; top:parent.top; rightMargin:8*root.uiScale; topMargin:5*root.uiScale }
        text:root.limitsVerified ? root.dataState : "LIMITS UNVERIFIED  •  "+root.dataState
        color:root.limitsVerified ? root.cyan : root.caution; font.pixelSize:6*root.uiScale; font.bold:true }
    Row {
        id:cylinderRow
        anchors { left:parent.left; right:parent.right; top:parent.top; bottom:footer.top; leftMargin:8*root.uiScale; rightMargin:8*root.uiScale; topMargin:23*root.uiScale; bottomMargin:4*root.uiScale }
        spacing:5*root.uiScale
        Repeater {
            model:4
            delegate:Rectangle {
                id:cylinder; required property int index
                width:(cylinderRow.width-3*cylinderRow.spacing)/4; height:cylinderRow.height; radius:3*root.uiScale
                color:"#081820"; border.width:(root.hottestCht===index||root.hottestEgt===index)?2:1
                border.color:(root.hottestCht===index||root.hottestEgt===index)?root.caution:"#314650"
                Text { anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:3*root.uiScale }
                    text:"CYL "+(cylinder.index+1); color:"#F4F8FC"; font.pixelSize:7*root.uiScale; font.bold:true }
                Row {
                    id:channelRow
                    x:4*root.uiScale; y:16*root.uiScale; width:parent.width-8*root.uiScale; height:parent.height-19*root.uiScale; spacing:3*root.uiScale
                    Repeater {
                        model:[
                            {name:"CHT", value:root.valid(root.chtValues,cylinder.index)?root.displayTemperature(Number(root.chtValues[cylinder.index])):NaN,
                             high:root.displayTemperature(root.chtMaximumC*1.08), warning:root.displayTemperature(root.chtMaximumC), caution:root.displayTemperature(root.chtMaximumC*.92)},
                            {name:"EGT", value:root.valid(root.egtValues,cylinder.index)?root.displayTemperature(Number(root.egtValues[cylinder.index])):NaN,
                             high:root.displayTemperature(root.egtMaximumC*1.03), warning:root.displayTemperature(root.egtMaximumC), caution:root.displayTemperature(root.egtMaximumC*.92)}]
                        delegate:Item {
                            id:channel; required property var modelData
                            width:(channelRow.width-channelRow.spacing)/2; height:channelRow.height
                            readonly property bool channelValid:isFinite(modelData.value) && root.dataState!=="LOST" && root.dataState!=="FAIL"
                            readonly property real fraction:channelValid?Math.max(0,Math.min(1,modelData.value/modelData.high)):0
                            readonly property color channelColor:!channelValid?root.invalid:modelData.value>=modelData.warning?root.warning:modelData.value>=modelData.caution?root.caution:root.normal
                            Text { anchors.horizontalCenter:parent.horizontalCenter; text:channel.modelData.name; color:root.cyan; font.pixelSize:5.5*root.uiScale; font.bold:true }
                            Rectangle { id:track; anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:10*root.uiScale; bottom:reading.top; bottomMargin:2*root.uiScale }
                                width:10*root.uiScale; color:"#14242D"; border.color:"#64757E"
                                Rectangle { anchors { left:parent.left; right:parent.right; bottom:parent.bottom; margins:1 }
                                    height:Math.max(1,(parent.height-2)*channel.fraction); color:channel.channelColor }
                                Repeater { model:5; delegate:Rectangle { required property int index; x:-3*root.uiScale; y:index*(track.height-1)/4; width:16*root.uiScale; height:1; color:"#BDCAD0" } }
                            }
                            Text { id:reading; anchors { horizontalCenter:parent.horizontalCenter; bottom:parent.bottom }
                                text:channel.channelValid?Math.round(channel.modelData.value):"—"; color:channel.channelColor; font.pixelSize:7*root.uiScale; font.bold:true }
                        }
                    }
                }
            }
        }
    }
    Item { id:footer; anchors { left:parent.left; right:parent.right; bottom:parent.bottom } height:16*root.uiScale
        Text { x:8*root.uiScale; text:"HOT CHT  "+(root.hottestCht>=0?"CYL "+(root.hottestCht+1):"—")+"   Δ "+Math.round(root.displayTemperature(root.spread(root.chtValues)))+root.temperatureUnit; color:root.caution; font.pixelSize:6*root.uiScale; font.bold:true }
        Text { anchors.right:parent.right; anchors.rightMargin:8*root.uiScale; text:"HOT EGT  "+(root.hottestEgt>=0?"CYL "+(root.hottestEgt+1):"—")+"   Δ "+Math.round(root.displayTemperature(root.spread(root.egtValues)))+root.temperatureUnit; color:root.caution; font.pixelSize:6*root.uiScale; font.bold:true }
    }
}
