import QtQuick
import QtTest
import "../../qml" as Ui

TestCase {
    id: tests
    name: "EmsHeaderLayout"
    when: windowShown
    visible: true
    width: 1280
    height: 800
    Ui.HmiGoldenController { id: fixture }
    Ui.AdaptiveEmsView {
        id: view
        width: 800; height: 480
        controller: fixture
        goldenMode: true
        clockOverride: "12:45:30"
    }

    function init() {
        view.width = 800; view.height = 480
        view.setupVisible = false
        view.clockOverride = "12:45:30"
        fixture.engineSourceState = "LIVE"
        fixture.engineDataAgeMs = 0
        fixture.engineLimits = {status:"INDICATIVE_UNVERIFIED"}
        wait(0)
    }
    function test_responsive_header_data() {
        var rows = []
        var sizes = [[800,480],[1024,600],[1280,800]]
        var states = ["LIVE","SIM","STALE","DEGRADED","LOST","FAIL"]
        for (var s=0;s<sizes.length;s++)
            for (var i=0;i<states.length;i++)
                rows.push({tag:sizes[s][0]+"-"+states[i], w:sizes[s][0], h:sizes[s][1], state:states[i]})
        return rows
    }
    function header() { return findChild(view,"emsStatusHeader") }
    function test_responsive_header(row) {
        view.width=row.w; view.height=row.h
        fixture.engineSourceState=row.state
        fixture.engineDataAgeMs=600
        wait(0)
        var h=header()
        verify(h.height>=44)
        var names=["emsHeaderIdentity","emsHeaderTitle","emsHeaderState","emsHeaderClock"]
        var previousRight=0
        for(var i=0;i<names.length;i++) {
            var item=findChild(h,names[i])
            verify(item!==null,names[i])
            var p=item.mapToItem(h,0,0)
            verify(item.visible,names[i]+" visible")
            verify(item.width>0,names[i]+" width")
            verify(p.x>=previousRight,names[i]+" horizontal overlap")
            verify(p.y>=0 && p.y+item.height<=h.height+0.1,names[i]+" vertical overflow")
            verify(p.x+item.width<=h.width,names[i]+" right overflow")
            previousRight=p.x+item.width
        }
        var brand=findChild(h,"emsHeaderBrand")
        verify(brand.children[0].x+brand.children[0].width<=brand.children[1].x,
               "EFIS and NEXT must use measured text widths")
        compare(findChild(h,"emsHeaderTitle").truncated,false)
        compare(findChild(h,"emsHeaderState").dataState,row.state)
        compare(findChild(h,"emsHeaderClock").text,"12:45:30  LOCAL")
    }
    function test_maximum_age_and_forced_sim_do_not_overlap() {
        fixture.engineSourceState="STALE"
        fixture.engineDataAgeMs=2147483647
        wait(0)
        verify(findChild(header(),"emsHeaderTitle").width>100)
        compare(findChild(header(),"emsHeaderTitle").truncated,false)
        fixture.engineSourceState="SIM FORCED"
        wait(0)
        compare(findChild(header(),"emsHeaderState").dataState,"SIM FORCED")
        compare(findChild(header(),"emsHeaderTitle").truncated,false)
    }
    function test_no_fabricated_flight_values_in_header() {
        var texts=[]
        function collect(item) {
            if(item.text!==undefined)texts.push(String(item.text))
            for(var i=0;i<item.children.length;i++)collect(item.children[i])
        }
        collect(header())
        verify(!/24\s*°|1013|OAT|QNH/.test(texts.join(" ")))
    }
    function test_clock_override_remains_a_binding() {
        var clock=findChild(header(),"emsHeaderClock")
        view.clockOverride="01:02:03"
        compare(clock.text,"01:02:03  LOCAL")
        view.clockOverride=""
        wait(1100)
        verify(/^\d\d:\d\d:\d\d  LOCAL$/.test(clock.text))
        view.clockOverride="23:59:58"
        compare(clock.text,"23:59:58  LOCAL")
    }
    function test_short_tap_does_not_open_setup() {
        var target=findChild(header(),"emsHeaderSetupTarget")
        verify(target.width>=44 && target.height>=44)
        mouseClick(target,target.width/2,target.height/2)
        compare(view.setupVisible,false)
    }
    function test_long_press_opens_actual_setup() {
        var target=findChild(header(),"emsHeaderSetupTarget")
        mousePress(target,target.width/2,target.height/2)
        wait(1350)
        mouseRelease(target,target.width/2,target.height/2)
        tryCompare(view,"setupVisible",true)
        compare(view.setupSection,0)
    }
    function test_cas_labels_do_not_collide_data() {
        return [{tag:"800-indicative",w:800,h:480,verified:false},
                {tag:"1024-indicative",w:1024,h:600,verified:false},
                {tag:"1280-indicative",w:1280,h:800,verified:false},
                {tag:"800-verified-fixture",w:800,h:480,verified:true},
                {tag:"1024-verified-fixture",w:1024,h:600,verified:true},
                {tag:"1280-verified-fixture",w:1280,h:800,verified:true}]
    }
    function test_cas_labels_do_not_collide(row) {
        view.width=row.w;view.height=row.h
        fixture.engineLimits={status:row.verified?"VERIFIED":"INDICATIVE_UNVERIFIED"}
        wait(0)
        var title=findChild(view,"emsCasTitle"), notice=findChild(view,"emsLimitsNotice")
        compare(notice.visible,!row.verified)
        compare(title.truncated,false)
        if(notice.visible)verify(title.x+title.width<notice.x)
        var banner=findChild(view,"emsCasBanner"), airframe=findChild(view,"emsAirframePanel")
        verify(banner.y>=findChild(view,"emsCasHeader").height)
        verify(airframe.y>=banner.y+banner.height)
        verify(airframe.height>0)
    }
}
