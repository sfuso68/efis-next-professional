import QtQuick
import QtTest
import "../../qml" as Ui

TestCase {
    id: tests
    name: "EngineChannelValidity"
    when: windowShown
    visible: true
    width: 1024
    height: 600
    Ui.HmiGoldenController { id: fixture }
    Ui.AdaptiveEmsView { id: view; anchors.fill: parent; controller: fixture; goldenMode: true }
    Ui.ProfessionalSystemsDeck { id: deck; width:1024; height:150; y:440; controller:fixture }

    function gauges(item, result) {
        if (item.readingValid !== undefined && item.label !== undefined) result.push(item)
        for (var i=0;i<item.children.length;i++) gauges(item.children[i], result)
        return result
    }
    function gauge(label) {
        var items=gauges(view,[])
        for(var i=0;i<items.length;i++) if(items[i].label===label)return items[i]
        fail("missing actual gauge "+label)
    }
    function init() {
        fixture.engineSourceState="LIVE"
        var fields=["engineRpm","manifoldPressure","oilPressure","oilTemperature","fuelFlow","fuelLeft","fuelRight","busVoltage","alternatorCurrent"]
        for(var i=0;i<fields.length;i++)fixture[fields[i]+"Valid"]=true
        fixture.fuelOnBoardValid=true
        fixture.engineRpm=5060
        fixture.chtValues=[107,108,109,110]
        fixture.egtValues=[777,781,789,800]
        wait(0)
    }
    function test_scalar_independence_data() {
        return [{tag:"rpm",name:"engineRpm",label:"RPM"},
                {tag:"map",name:"manifoldPressure",label:"MAP"},
                {tag:"oil-pressure",name:"oilPressure",label:"OIL PRESS"},
                {tag:"oil-temperature",name:"oilTemperature",label:"OIL TEMP"},
                {tag:"fuel-flow",name:"fuelFlow",label:"FUEL FLOW"}]
    }
    function test_scalar_independence(row) {
        var control=gauge(row.label)
        verify(control.readingValid)
        verify(findChild(control,"engineGaugeNeedle").visible)
        fixture[row.name+"Valid"]=false
        compare(control.readingValid,false)
        compare(findChild(control,"engineGaugeReading").text,"—")
        compare(findChild(control,"engineGaugeNeedle").visible,false)
        compare(gauge(row.label==="RPM"?"MAP":"RPM").readingValid,true)
        verify(view.casHeadline!=="SYSTEM NORMAL")
        fixture[row.name+"Valid"]=true
        compare(control.readingValid,true)
    }
    function test_usable_zero_is_not_missing() {
        fixture.engineRpm=0
        compare(findChild(gauge("RPM"),"engineGaugeReading").text,"0")
        compare(gauge("RPM").readingValid,true)
    }
    function test_partial_fuel() {
        fixture.fuelLeftValid=false
        compare(findChild(view,"fuelTankReadingLEFT").text,"LEFT  —")
        verify(findChild(view,"fuelTankReadingRIGHT").text.indexOf("—")<0)
        compare(findChild(view,"fuelTotalReading").text,"—")
        fixture.fuelFlowValid=false
        verify(!isFinite(view.fuelEnduranceHours))
        fixture.fuelOnBoardValid=false
        verify(!isFinite(view.fuelTotalGallons))
    }
    function test_partial_electrical() {
        fixture.busVoltageValid=false
        compare(findChild(view,"electricalReadingMAIN BUS").text,"—")
        verify(findChild(view,"electricalReadingALT").text.indexOf("—")<0)
    }
    function test_pfd_deck_uses_channel_flags() {
        fixture.engineRpmValid=false
        var items=gauges(deck,[])
        compare(items.length,5)
        for(var i=0;i<items.length;i++) {
            if(items[i].label==="RPM")compare(findChild(items[i],"engineGaugeReading").text,"—")
            else verify(items[i].readingValid)
        }
        fixture.engineSourceState="LOST"
        for(var j=0;j<items.length;j++)compare(findChild(items[j],"engineGaugeNeedle").visible,false)
    }
    function test_per_cylinder_masking() {
        fixture.chtValues=[NaN,108,109,110]
        var count=0, numeric=0
        function visit(item) {
            if(item.objectName==="cylinderTemperatureReading") {
                if(item.text==="—")count++;else numeric++
            }
            for(var i=0;i<item.children.length;i++)visit(item.children[i])
        }
        visit(view)
        compare(count,1)
        compare(numeric,7)
    }
    function test_lean_refuses_invalid_and_stale_channels() {
        view.leanActive=false
        fixture.egtValues=[NaN,781,789,800]
        view.startLean()
        compare(view.leanActive,false)
        fixture.egtValues=[777,781,789,800]
        view.startLean()
        compare(view.leanActive,true)
        fixture.egtValues=[777,NaN,789,800]
        view.updateLean()
        compare(view.leanActive,false)
        fixture.egtValues=[777,781,789,800]
        fixture.engineSourceState="STALE"
        view.startLean()
        compare(view.leanActive,false)
    }
    function test_stale_never_keeps_readable_residual() {
        fixture.engineSourceState="STALE"
        var items=gauges(view,[])
        compare(items.length,5)
        for(var i=0;i<items.length;i++) {
            compare(findChild(items[i],"engineGaugeReading").text,"—")
            compare(findChild(items[i],"engineGaugeNeedle").visible,false)
        }
    }
}
