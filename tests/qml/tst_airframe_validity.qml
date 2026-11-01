import QtQuick
import QtTest
import "../../qml" as Ui

TestCase {
    id: testCase
    name: "AirframeValidity"
    when: windowShown
    visible: true
    width: 1024
    height: 600
    Ui.HmiGoldenController { id: fixture }
    Ui.AdaptiveEmsView { id: view; width:1024; height:600; controller:fixture; goldenMode:true }
    Ui.ProfessionalSystemsDeck { id: deck; width:1024; height:120; visible:false; controller:fixture }

    function panel() { return findChild(view,"emsAirframePanel") }
    function init() {
        fixture.engineSourceState="LIVE"
        fixture.engineDataAgeMs=0
        fixture.airframeSourceState="SIM"
        fixture.airframeDataAgeMs=0
        fixture.gearDataValid=true
        fixture.flapAngleValid=true
        fixture.flapCommandedValid=true
        fixture.landingGearInstalled=true
        fixture.flapsInstalled=true
        fixture.gearState="DOWN & LOCKED"
        fixture.flapDetents=[0,10,15,20,30]
        fixture.flapAngle=15
        fixture.flapCommandedAngle=20
    }
    function test_unavailable_data() {
        return ["LOST","FAIL","STALE","WAITING",""].map(function(s) { return {tag:s||"empty",state:s} })
    }
    function test_unavailable(row) {
        fixture.airframeSourceState=row.state
        compare(panel().gearUsable,false)
        compare(findChild(view,"airframeGearValue").text,"NO DATA")
        compare(findChild(view,"airframeFlapValue").text,"—")
        compare(findChild(view,"airframeMeasuredMarker").visible,false)
        compare(findChild(view,"airframeCommandMarker").visible,false)
        compare(deck.flapReading,"—")
    }
    function test_independent_channels() {
        fixture.gearDataValid=false
        compare(panel().gearUsable,false)
        compare(panel().flapUsable,true)
        fixture.flapAngleValid=false
        compare(panel().flapUsable,false)
        compare(panel().commandUsable,true)
        fixture.flapCommandedValid=false
        compare(panel().commandUsable,false)
    }
    function test_engine_health_cannot_heal_airframe() {
        fixture.airframeSourceState="LOST"
        fixture.engineSourceState="LIVE"
        compare(panel().gearUsable,false)
        fixture.engineSourceState="SIM"
        compare(panel().flapUsable,false)
        compare(view.airframeUnavailable,true)
        compare(view.casHeadline,"AIRFRAME DATA UNAVAILABLE")
        compare(view.casLevel,"CAUTION")
        fixture.airframeSourceState="SIM"
        fixture.engineSourceState="FAIL"
        compare(panel().gearUsable,true)
        compare(deck.flapReading,"15° SIM")
        compare(findChild(view,"airframeSourceLabel").text,"SIM")
    }
    function test_bad_age_data() {
        return [{tag:"unknown",age:-1},{tag:"nan",age:NaN},{tag:"infinite",age:Infinity}]
    }
    function test_bad_age(row) {
        fixture.airframeDataAgeMs=row.age
        compare(panel().gearUsable,false)
        compare(panel().flapUsable,false)
    }
    function test_scale_data() {
        return [
            {tag:"empty",detents:[]}, {tag:"one",detents:[0]},
            {tag:"duplicate",detents:[0,15,15]}, {tag:"reversed",detents:[30,0]},
            {tag:"nan",detents:[0,NaN]}, {tag:"text",detents:[0,"30"]}
        ]
    }
    function test_scale(row) {
        fixture.flapDetents=row.detents
        compare(panel().scaleValid,false)
        compare(panel().flapUsable,false)
        compare(panel().commandUsable,false)
        compare(findChild(view,"airframeFlapValue").text,"—")
    }
    function test_configured_range() {
        fixture.flapDetents=[-10,0,20,40]
        fixture.flapAngle=15
        fixture.flapCommandedAngle=40
        compare(panel().fraction(15),0.5)
        compare(panel().fraction(40),1)
        compare(panel().flapUsable,true)
        fixture.flapAngle=41
        compare(panel().flapUsable,false)
        compare(panel().commandUsable,true)
        fixture.flapAngle=NaN
        fixture.flapCommandedAngle=Infinity
        compare(panel().flapUsable,false)
        compare(panel().commandUsable,false)
    }
    function test_no_equipment() {
        fixture.landingGearInstalled=false
        fixture.flapsInstalled=false
        compare(findChild(view,"airframeGearValue").text,"N/A")
        compare(findChild(view,"airframeFlapValue").text,"N/A")
        compare(findChild(view,"airframeMeasuredMarker").visible,false)
    }
    function test_missing_contract() {
        var legacy={}
        for (var key in fixture)
            if (["airframeSourceState","airframeDataAgeMs","gearDataValid","flapAngleValid","flapCommandedValid"].indexOf(key)<0)
                legacy[key]=fixture[key]
        view.controller=legacy
        compare(panel().gearUsable,false)
        compare(panel().flapUsable,false)
        view.controller=fixture
    }
}
