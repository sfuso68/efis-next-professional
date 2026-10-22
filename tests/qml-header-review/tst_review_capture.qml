import QtQuick
import QtTest
import "../../qml" as Ui
TestCase {
    name: "R108ReviewCapture"
    when: windowShown
    visible: true
    width:1280; height:800
    Ui.HmiGoldenController { id:fixture }
    Ui.AdaptiveEmsView { id:view; width:800; height:480; controller:fixture
        clockOverride:"12:45:30"; goldenMode:true }
    function test_capture_data() {
        var rows=[];var sizes=[[800,480],[1024,600],[1280,800]]
        var states=["LIVE","SIM","STALE","DEGRADED","LOST","FAIL"]
        var ages=[0,0,600,600,1500,0]
        for(var s=0;s<sizes.length;s++)for(var i=0;i<states.length;i++)
            rows.push({tag:sizes[s][0]+"-"+states[i],w:sizes[s][0],h:sizes[s][1],state:states[i],age:ages[i]})
        return rows
    }
    function test_capture(row) {
        view.width=row.w;view.height=row.h
        fixture.engineSourceState=row.state;fixture.engineDataAgeMs=row.age
        wait(150)
        var image=grabImage(view)
        compare(image.width,row.w);compare(image.height,row.h)
        image.save("output/ems-header-review/ems-"+row.w+"x"+row.h+"-"+row.state.toLowerCase()+".png")
    }
}
