import QtQuick

Canvas {
    id: icon
    property string kind: "engine"
    property color strokeColor: "#28D9F4"
    property real strokeScale: 1
    antialiasing: true
    onKindChanged: requestPaint()
    onStrokeColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const c=getContext("2d"), w=width, h=height, s=Math.min(w,h)
        c.reset(); c.strokeStyle=strokeColor; c.fillStyle="transparent"
        c.lineWidth=Math.max(1.2,s*.065*strokeScale); c.lineCap="round"; c.lineJoin="round"
        function line(points) { c.beginPath(); c.moveTo(points[0]*w,points[1]*h); for(let i=2;i<points.length;i+=2)c.lineTo(points[i]*w,points[i+1]*h); c.stroke() }
        function rect(x,y,rw,rh) { c.strokeRect(x*w,y*h,rw*w,rh*h) }
        if(kind==="engine") {
            line([.16,.35,.26,.35,.31,.24,.65,.24,.72,.35,.84,.35,.84,.70,.74,.70,.68,.80,.31,.80,.25,.70,.16,.70,.16,.35])
            line([.37,.24,.37,.15,.59,.15,.59,.24]); line([.84,.46,.94,.46,.94,.62,.84,.62])
        } else if(kind==="indications") {
            rect(.16,.55,.15,.28); rect(.42,.36,.15,.47); rect(.68,.18,.15,.65)
        } else if(kind==="electrical") {
            line([.10,.59,.28,.59,.38,.29,.52,.76,.65,.42,.73,.59,.91,.59])
        } else if(kind==="fuel") {
            rect(.20,.15,.43,.68); line([.25,.29,.58,.29]); line([.63,.27,.76,.38,.76,.72,.88,.72,.88,.39,.82,.31])
        } else if(kind==="system") {
            c.beginPath(); c.arc(.5*w,.5*h,.26*s,0,Math.PI*2); c.stroke(); c.beginPath(); c.arc(.5*w,.5*h,.10*s,0,Math.PI*2); c.stroke()
            for(let i=0;i<8;i++){const a=i*Math.PI/4; line([.5+Math.cos(a)*.29,.5+Math.sin(a)*.29,.5+Math.cos(a)*.41,.5+Math.sin(a)*.41])}
        } else if(kind==="checklist") {
            rect(.23,.14,.55,.72); line([.33,.34,.39,.40,.51,.27]); line([.33,.54,.39,.60,.51,.47]); line([.56,.35,.70,.35]); line([.56,.55,.70,.55])
        } else if(kind==="lean") {
            c.beginPath(); c.moveTo(.18*w,.70*h); c.bezierCurveTo(.30*w,.18*h,.78*w,.18*h,.82*w,.22*h); c.bezierCurveTo(.77*w,.68*h,.43*w,.84*h,.18*w,.70*h); c.stroke(); line([.25,.75,.65,.35])
        } else if(kind==="trend") {
            line([.15,.80,.15,.18]); line([.15,.80,.88,.80]); line([.24,.66,.40,.49,.54,.60,.75,.30,.87,.40])
        } else if(kind==="airplane") {
            c.fillStyle=strokeColor; c.beginPath(); c.moveTo(.5*w,.08*h); c.lineTo(.57*w,.38*h); c.lineTo(.91*w,.50*h); c.lineTo(.91*w,.58*h); c.lineTo(.57*w,.54*h); c.lineTo(.55*w,.78*h); c.lineTo(.68*w,.87*h); c.lineTo(.68*w,.92*h); c.lineTo(.5*w,.88*h); c.lineTo(.32*w,.92*h); c.lineTo(.32*w,.87*h); c.lineTo(.45*w,.78*h); c.lineTo(.43*w,.54*h); c.lineTo(.09*w,.58*h); c.lineTo(.09*w,.50*h); c.lineTo(.43*w,.38*h); c.closePath(); c.fill()
        }
    }
}
