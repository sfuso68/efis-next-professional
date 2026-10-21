import QtQuick
import QtTest
import "../../qml" as Ui

TestCase {
    id: testCase
    name: "WeatherRadarPresentation"
    when: windowShown
    visible: true
    width: 420
    height: 300

    // A generated opaque 16x16 red PNG, local and deterministic: no HTTP,
    // provider availability, installed SVG plugin or external clock required.
    readonly property string readyPng: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAAGUlEQVR4nGP4z8DwnxLMMGrAqAGjBgwXAwAwxP4QHCfkAAAAAABJRU5ErkJggg=="
    QtObject {
        id: provider
        property string radarState: "CURRENT"
        property string radarSource: testCase.readyPng
        property var radarAgeMinutes: 0
        property string radarProduct: "VMI"
        property string radarArea: "TEST FIXTURE"
        property bool simulation: false
        property int simulationPhase: 1
        function cycleProduct() {}
    }
    Rectangle {
        id: stage
        width: 400
        height: 260
        color: "#112233"
        Ui.WeatherRadarOverlay {
            id: overlay
            anchors.fill: parent
            weatherProvider: provider
            interactive: false
        }
    }
    // Discover the real image and badge by their public Qt properties so that
    // these regressions also run against the unmodified faulty component.
    function imageItem() {
        for (var i=0; i<overlay.children.length; ++i) {
            var child=overlay.children[i]
            if (child.source !== undefined && child.fillMode !== undefined)
                return child
        }
        fail("Radar image not instantiated")
    }
    function badgeItem() {
        for (var i=0; i<overlay.children.length; ++i) {
            var child=overlay.children[i]
            if (child.border !== undefined && child.children.length > 0) {
                var column=child.children[0]
                if (column.children.length > 0 && String(column.children[0].text).indexOf("WX ") === 0)
                    return child
            }
        }
        fail("Weather badge not instantiated")
    }
    function init() {
        overlay.weatherProvider=provider
        overlay.radarEnabled=true
        overlay.compactLayout=false
        provider.simulation=false
        provider.radarState="CURRENT"
        provider.radarAgeMinutes=0
        provider.radarSource=readyPng
        tryCompare(imageItem(), "status", Image.Ready)
        tryCompare(overlay, "available", true)
        compare(stage.visible,true,"Test surface must actually be shown")
        compare(imageItem().visible,true,"Fresh fixture must actually be drawn before testing its removal")
    }
    function test_expired_cache_is_hidden() {
        provider.radarState="EXPIRED"
        provider.radarAgeMinutes=21
        compare(overlay.available,false)
        compare(imageItem().visible,false,"Expired decoded cached image must not remain drawn")
    }
    function test_stale_primary_colour_is_not_current() {
        var freshColour=String(badgeItem().border.color)
        provider.radarState="STALE"
        provider.radarAgeMinutes=12
        compare(overlay.available,true)
        verify(String(badgeItem().border.color)!==freshColour,
               "STALE needs a primary caution cue, not the CURRENT frame colour")
    }
    function visibleTextCount(item, text) {
        var count=item.text === text && item.visible ? 1 : 0
        for (var i=0;i<item.children.length;++i)
            count+=visibleTextCount(item.children[i],text)
        return count
    }
    function test_cache_age_transitions_data() {
        return [{tag:"regular",compact:false},{tag:"compact",compact:true}]
    }
    function test_cache_age_transitions(row) {
        overlay.compactLayout=row.compact
        // Controlled provider inputs. The production C++ classifier's exact
        // second boundaries are tested separately in weather_service_tests.
        var timeline=[
            {seconds:0,state:"CURRENT",show:true},
            {seconds:600,state:"CURRENT",show:true},
            {seconds:601,state:"STALE",show:true},
            {seconds:1200,state:"STALE",show:true},
            {seconds:1201,state:"EXPIRED",show:false},
            {seconds:-6,state:"CLOCK SKEW",show:false},
            {seconds:0,state:"CURRENT",show:true}
        ]
        for(var i=0;i<timeline.length;++i) {
            var tick=timeline[i]
            provider.radarAgeMinutes=tick.seconds < 0 ? -1 : Math.floor(tick.seconds/60)
            provider.radarState=tick.state
            if(tick.show)tryCompare(imageItem(),"status",Image.Ready)
            compare(overlay.available,tick.show)
            compare(imageItem().visible,tick.show)
            var badgeText=String(badgeItem().children[0].children[1].text)
            verify(badgeText.indexOf(tick.state)>=0,"Cause must remain visible even in compact mode: "+badgeText)
            if(!tick.show)verify(badgeText.indexOf("NO DATA")>=0)
        }
    }
    function test_disallowed_states_data() {
        return ["EXPIRED","CLOCK SKEW","CLOCK_SKEW","NO DATA","ERROR","INVALID","","UNKNOWN","current","CACHED"]
            .map(function(state){return {tag:state || "empty",state:state}})
    }
    function test_disallowed_states(row) {
        provider.radarState=row.state
        compare(overlay.available,false)
        compare(imageItem().visible,false)
        compare(String(imageItem().source),"")
    }
    function test_invalid_age_data() {
        return [{tag:"negative",age:-1},{tag:"nan",age:NaN},{tag:"infinite",age:Infinity},
                {tag:"string",age:"12"},{tag:"null",age:null},{tag:"missing",age:undefined}]
    }
    function test_invalid_age(row) {
        provider.radarAgeMinutes=row.age
        compare(overlay.available,false)
        compare(imageItem().visible,false)
        verify(String(badgeItem().children[0].children[1].text).indexOf("AGE UNKNOWN")>=0)
    }
    function test_off_and_recovery() {
        overlay.radarEnabled=false
        compare(overlay.available,false)
        compare(imageItem().visible,false)
        compare(String(imageItem().source),"")
        compare(String(badgeItem().children[0].children[1].text),"OFF")
        overlay.radarEnabled=true
        tryCompare(imageItem(),"status",Image.Ready)
        compare(overlay.available,true)
        compare(imageItem().visible,true)
    }
    function test_simulation_and_decorations() {
        var currentColour=String(badgeItem().border.color)
        provider.simulation=true
        compare(overlay.available,false,"Real CURRENT frame cannot be relabelled SIM")
        provider.radarState="SIMULATED"
        tryCompare(imageItem(),"status",Image.Ready)
        compare(overlay.available,true)
        verify(String(badgeItem().border.color)!==currentColour)
        verify(String(badgeItem().children[0].children[1].text).indexOf("TRAINING")>=0)
        compare(visibleTextCount(overlay,"ϟ"),3)
        compare(visibleTextCount(overlay,"TRAINING"),1)
        overlay.radarEnabled=false
        compare(visibleTextCount(overlay,"ϟ"),0)
        compare(visibleTextCount(overlay,"TRAINING"),0)
        overlay.radarEnabled=true
        provider.radarState="EXPIRED"
        compare(overlay.available,false)
        compare(visibleTextCount(overlay,"ϟ"),0)
        compare(visibleTextCount(overlay,"TRAINING"),0)
        provider.simulation=false
        provider.radarState="SIMULATED"
        compare(overlay.available,false,"SIMULATED without simulation provenance must fail closed")
    }
    function test_missing_provider() {
        overlay.weatherProvider=null
        compare(overlay.available,false)
        compare(imageItem().visible,false)
        verify(String(badgeItem().children[0].children[1].text).indexOf("NO DATA")>=0)
    }
    function test_image_error_and_recovery() {
        provider.radarSource="data:image/png;base64,AAAA"
        tryCompare(imageItem(),"status",Image.Error)
        compare(overlay.available,false)
        compare(imageItem().visible,false)
        verify(String(badgeItem().children[0].children[1].text).indexOf("IMAGE ERROR")>=0)
        provider.radarSource=readyPng
        tryCompare(imageItem(),"status",Image.Ready)
        compare(overlay.available,true)
    }
    function test_rendered_pixels_removed_on_expiry() {
        wait(30)
        var fresh=grabImage(stage)
        verify(!Qt.colorEqual(fresh.pixel(340,120),stage.color),"Ready fixture must produce radar pixels")
        provider.radarState="EXPIRED"
        wait(30)
        var expired=grabImage(stage)
        verify(Qt.colorEqual(expired.pixel(340,120),stage.color),"Expired radar must leave the underlying map/background visible")
    }
}
