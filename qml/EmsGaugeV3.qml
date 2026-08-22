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
    readonly property real span: Math.max(0.001, maximum-minimum)
    readonly property real fraction: Math.max(0, Math.min(1, (value-minimum)/span))
    readonly property real cx: width/2
    readonly property real cy: height*0.51
    readonly property real radius: Math.min(width*0.47, height*0.45)

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
                [1.00, "#89949A", 1.7], [.977, "#202B31", 3.2],
                [.947, "#6F787D", 1.2], [.922, "#0A1115", 2.0],
                [.885, "#3C4A51", 1.0]
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
            arc(gauge.minimum,gauge.greenStart,"#D93A2E",r*.037)
            arc(gauge.greenStart,gauge.greenEnd,"#4BCD37",r*.052)
            arc(gauge.greenEnd,Math.min(gauge.maximum,gauge.greenEnd+gauge.span*.10),"#FFD31A",r*.052)
            arc(Math.min(gauge.maximum,gauge.greenEnd+gauge.span*.10),gauge.maximum,"#EE3224",r*.052)

            ctx.textAlign="center"; ctx.textBaseline="middle"
            const count = Math.min(150,Math.floor(gauge.span/gauge.minorStep+.5))
            for (let i=0;i<=count;i++) {
                const v=gauge.minimum+i*gauge.minorStep
                const ratio=gauge.majorStep/gauge.minorStep
                const isMajor=Math.abs(i/ratio-Math.round(i/ratio))<.07
                const isMid=!isMajor && i%5===0
                const a=angleFor(v), outer=r*.82
                const len=isMajor?r*.12:isMid?r*.085:r*.052
                ctx.strokeStyle=isMajor?"#F4F8F9":isMid?"#CBD7DB":"#91A4AD"
                ctx.lineWidth=(isMajor?2.2:isMid?1.4:.75)*gauge.uiScale
                ctx.beginPath(); ctx.moveTo(cx+Math.cos(a)*(outer-len),cy+Math.sin(a)*(outer-len)); ctx.lineTo(cx+Math.cos(a)*outer,cy+Math.sin(a)*outer); ctx.stroke()
                if (isMajor) {
                    ctx.fillStyle="#EEF4F6"
                    ctx.font="600 "+Math.max(8,Math.round(r*.105))+"px 'Segoe UI'"
                    const textRadius=r*.625
                    ctx.fillText(Number(v).toFixed(0),cx+Math.cos(a)*textRadius,cy+Math.sin(a)*textRadius)
                }
            }

            ctx.fillStyle="#DCE6EA"; ctx.font="600 "+Math.max(10,Math.round(r*.12))+"px 'Segoe UI'"
            ctx.fillText(gauge.label,cx,cy-r*.16)

            let glass=ctx.createLinearGradient(cx,cy-r*.83,cx,cy+r*.12)
            glass.addColorStop(0,"rgba(205,235,248,0.16)")
            glass.addColorStop(.36,"rgba(86,143,169,0.045)")
            glass.addColorStop(1,"rgba(0,0,0,0)")
            ctx.fillStyle=glass; ctx.beginPath(); ctx.arc(cx,cy,r*.80,Math.PI*1.12,Math.PI*1.88); ctx.quadraticCurveTo(cx,cy+r*.10,cx-r*.75,cy-r*.20); ctx.fill()
            ctx.strokeStyle="rgba(225,242,249,0.36)"; ctx.lineWidth=Math.max(1,r*.008)
            ctx.beginPath(); ctx.arc(cx,cy,r*.94,Math.PI*1.16,Math.PI*1.84); ctx.stroke()
        }
    }

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
        text: Number(gauge.value).toFixed(gauge.decimals); color: "#F5F8F9"; font.family: "Segoe UI"; font.pixelSize: (dominant?45:31)*gauge.uiScale; font.bold: true }
    Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 4*uiScale }
        text: gauge.unit; color: "#D2DCE0"; font.family: "Segoe UI"; font.pixelSize: (dominant?12:10)*gauge.uiScale; font.bold: true }
}
