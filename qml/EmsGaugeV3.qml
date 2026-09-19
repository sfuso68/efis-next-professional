import QtQuick

Item {
    id: gauge
    required property string label
    required property real value
    required property string unit
    required property real minimum
    required property real maximum
    required property real greenStart
    required property real greenEnd
    property real majorStep: (maximum-minimum)/6
    property real minorStep: majorStep/5
    property int decimals: 0
    property real uiScale: 1
    property bool dominant: false
    property bool compact: false
    property bool normalRangesVisible: true
    readonly property real span: Math.max(0.001, maximum-minimum)
    readonly property real fraction: Math.max(0, Math.min(1, (value-minimum)/span))
    readonly property real cx: width/2
    readonly property real cy: height*0.51
    readonly property real radius: Math.min(width*0.47, height*0.45)
    onCompactChanged: face.requestPaint()
    onNormalRangesVisibleChanged: face.requestPaint()
    Timer { interval:120; running:true; repeat:false; onTriggered:face.requestPaint() }

    Canvas {
        id: face
        anchors.fill: parent
        renderTarget: Canvas.Image
        renderStrategy: Canvas.Cooperative
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const cx = gauge.cx, cy = gauge.cy, r = gauge.radius
            if (r <= 4) return

            let halo = ctx.createRadialGradient(cx-r*.22, cy-r*.28, r*.05, cx, cy, r*1.08)
            halo.addColorStop(0, "#173143")
            halo.addColorStop(.46, "#071722")
            halo.addColorStop(1, "#01070B")
            ctx.fillStyle = halo
            ctx.beginPath(); ctx.arc(cx,cy,r*.99,0,Math.PI*2); ctx.fill()

            const rings = [
                [1.00, "#AAB7BD", 1.9], [.977, "#34444C", 3.2],
                [.947, "#93A0A6", 1.3], [.922, "#111D23", 2.0],
                [.885, "#5D7079", 1.1]
            ]
            for (let i=0;i<rings.length;i++) {
                ctx.strokeStyle=rings[i][1]; ctx.lineWidth=rings[i][2]*gauge.uiScale
                ctx.beginPath(); ctx.arc(cx,cy,r*rings[i][0],0,Math.PI*2); ctx.stroke()
            }

            const start = 140*Math.PI/180, sweep = 260*Math.PI/180
            function angleFor(v) { return start + Math.max(0,Math.min(1,(v-gauge.minimum)/gauge.span))*sweep }
            function arc(from,to,color,width) {
                ctx.strokeStyle=color; ctx.lineWidth=width; ctx.lineCap="butt"
                ctx.beginPath(); ctx.arc(cx,cy,r*.86,angleFor(from),angleFor(to)); ctx.stroke()
            }
            if (gauge.normalRangesVisible) {
                arc(gauge.minimum,gauge.greenStart,"#D93A2E",r*.037)
                arc(gauge.greenStart,gauge.greenEnd,"#4BCD37",r*.052)
                arc(gauge.greenEnd,Math.min(gauge.maximum,gauge.greenEnd+gauge.span*.10),"#FFD31A",r*.052)
                arc(Math.min(gauge.maximum,gauge.greenEnd+gauge.span*.10),gauge.maximum,"#EE3224",r*.052)
            } else {
                arc(gauge.minimum,gauge.maximum,"#8A96A3",r*.052)
            }

            ctx.textAlign="center"; ctx.textBaseline="middle"
            const count = Math.min(150,Math.floor(gauge.span/gauge.minorStep+.5))
            for (let i=0;i<=count;i++) {
                const v=gauge.minimum+i*gauge.minorStep
                const ratio=gauge.majorStep/gauge.minorStep
                const isMajor=Math.abs(i/ratio-Math.round(i/ratio))<.07
                const isMid=!isMajor && i%5===0
                const a=angleFor(v), outer=r*.82
                const len=isMajor?r*.12:isMid?r*.085:r*.052
                ctx.strokeStyle=isMajor?"#FFFFFF":isMid?"#E1EBEE":"#AFC0C7"
                ctx.lineWidth=(isMajor?2.2:isMid?1.4:.75)*gauge.uiScale
                ctx.beginPath(); ctx.moveTo(cx+Math.cos(a)*(outer-len),cy+Math.sin(a)*(outer-len)); ctx.lineTo(cx+Math.cos(a)*outer,cy+Math.sin(a)*outer); ctx.stroke()
                if (isMajor) {
                    ctx.fillStyle="#EEF4F6"
                    ctx.font="600 "+Math.max(8,Math.round(r*.105))+"px sans-serif"
                    const textRadius=r*.625
                    ctx.fillText(Number(v).toFixed(0),cx+Math.cos(a)*textRadius,cy+Math.sin(a)*textRadius)
                }
            }

            if (!gauge.compact) {
                ctx.fillStyle="#DCE6EA"; ctx.font="600 "+Math.max(10,Math.round(r*.12))+"px sans-serif"
                ctx.fillText(gauge.label,cx,cy-r*.16)
            }

            let glass=ctx.createLinearGradient(cx,cy-r*.83,cx,cy+r*.12)
            glass.addColorStop(0,"rgba(205,235,248,0.16)")
            glass.addColorStop(.36,"rgba(86,143,169,0.045)")
            glass.addColorStop(1,"rgba(0,0,0,0)")
            ctx.fillStyle=glass; ctx.beginPath(); ctx.arc(cx,cy,r*.80,Math.PI*1.12,Math.PI*1.88); ctx.quadraticCurveTo(cx,cy+r*.10,cx-r*.75,cy-r*.20); ctx.fill()
            ctx.strokeStyle="rgba(225,242,249,0.36)"; ctx.lineWidth=Math.max(1,r*.008)
            ctx.beginPath(); ctx.arc(cx,cy,r*.94,Math.PI*1.16,Math.PI*1.84); ctx.stroke()
        }
    }

    Repeater {
        visible:gauge.compact; model:31
        delegate:Item {
            required property int index
            x:gauge.cx; y:gauge.cy; width:0; height:0
            rotation:-130+index*(260/30)
            readonly property real sampleValue:gauge.minimum+(gauge.span*index/30)
            readonly property color bandColor:!gauge.normalRangesVisible?"#8A96A3":sampleValue<gauge.greenStart?"#D93A2E":sampleValue<=gauge.greenEnd?"#4BCD37":sampleValue<=gauge.greenEnd+gauge.span*.10?"#FFD31A":"#EE3224"
            Rectangle { x:-Math.max(1.8,gauge.radius*.04); y:-gauge.radius*.88; width:Math.max(3.6,gauge.radius*.08); height:gauge.radius*.075; color:parent.bandColor }
        }
    }
    Repeater {
        visible:gauge.compact; model:11
        delegate:Item {
            required property int index
            x:gauge.cx; y:gauge.cy; width:0; height:0
            rotation:-130+index*(260/10)
            Rectangle { x:-Math.max(.7,gauge.radius*.012); y:-gauge.radius*.80; width:Math.max(1.4,gauge.radius*.024); height:gauge.radius*(index%2===0?.12:.07); color:index%2===0?"#FFFFFF":"#C9D6DB" }
        }
    }

    // Compact PFD deck fallback: these vector overlays remain crisp even when
    // the Canvas is downscaled aggressively by the 1024x600 flight layout.
    Rectangle { visible:gauge.compact; x:gauge.cx-gauge.radius; y:gauge.cy-gauge.radius; width:gauge.radius*2; height:width; radius:width/2; color:"transparent"; border.width:Math.max(1,1.4*gauge.uiScale); border.color:"#AAB7BD" }
    Rectangle { visible:gauge.compact; x:gauge.cx-gauge.radius*.92; y:gauge.cy-gauge.radius*.92; width:gauge.radius*1.84; height:width; radius:width/2; color:"transparent"; border.width:Math.max(1,gauge.uiScale); border.color:"#4E626C" }
    Text { visible:gauge.compact; anchors { horizontalCenter:parent.horizontalCenter; top:parent.top; topMargin:4*gauge.uiScale }
        width:parent.width*.76; horizontalAlignment:Text.AlignHCenter; elide:Text.ElideRight
        text:gauge.label; color:"#DCE6EA"; font.pixelSize:Math.max(8,9*gauge.uiScale); font.bold:true }

    Item {
        id: needle
        x: gauge.cx; y: gauge.cy
        rotation: -130 + gauge.fraction*260
        Behavior on rotation { NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        Rectangle { x: 1*gauge.uiScale; y: -gauge.radius*.67; width: 4*gauge.uiScale; height: gauge.radius*.72; radius: 2*gauge.uiScale; color: "#80000000" }
        Rectangle { x: -2*gauge.uiScale; y: -gauge.radius*.69; width: 4*gauge.uiScale; height: gauge.radius*.72; radius: 2*gauge.uiScale
            gradient: Gradient { orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#8B969B" }
                GradientStop { position: .48; color: "#FFFFFF" }
                GradientStop { position: 1; color: "#79868C" } }
            layer.enabled: true
        }
        Rectangle { x: -1*gauge.uiScale; y: 0; width: 2*gauge.uiScale; height: gauge.radius*.18; color: "#66747B" }
    }
    Rectangle { x: gauge.cx-gauge.radius*.105; y: gauge.cy-gauge.radius*.105; width: gauge.radius*.21; height: width; radius: width/2
        color: "#11191D"; border.width: Math.max(1,2*gauge.uiScale); border.color: "#68747A"
        Rectangle { anchors.centerIn: parent; width: parent.width*.48; height: width; radius: width/2; color: "#11171A"; border.width: 1; border.color: "#B3BDC1" }
        Rectangle { anchors.centerIn: parent; width: parent.width*.20; height: width; radius: width/2; color: "#79868B" }
    }
    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: dominant ? 20*uiScale : 14*uiScale }
        text: Number(gauge.value).toFixed(gauge.decimals); color: "#F5F8F9"; font.pixelSize: Math.max(dominant?38:26,(dominant?45:31)*gauge.uiScale); font.bold: true }
    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 4*uiScale }
        text: gauge.unit; color: "#D2DCE0"; font.pixelSize: Math.max(9,(dominant?12:10)*gauge.uiScale); font.bold: true }
}
