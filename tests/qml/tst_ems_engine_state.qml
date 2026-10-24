import QtQuick
import QtTest
import "../../qml" as Ui

TestCase {
    id: testCase
    name: "EmsEngineStateIsolation"
    when: windowShown
    width: 1024
    height: 600

    Ui.HmiGoldenController { id: fixture }
    Ui.AdaptiveEmsView {
        id: view
        width: 1024
        height: 600
        controller: fixture
        goldenMode: true
    }

    // Inspect actual instantiated controls, including their nested state badges.
    // A fixture/helper-only test would not detect a wrong binding in this view.
    function stateConsumers(item, result) {
        if (item.objectName === "emsAirframePanel") return result
        if (item.dataState !== undefined) result.push(item)
        for (var i = 0; i < item.children.length; ++i)
            stateConsumers(item.children[i], result)
        return result
    }

    function test_independent_domains_data() {
        return [
            {tag:"flight-live-engine-lost", flight:"LIVE", engine:"LOST", expected:"LOST", usable:false, degraded:false},
            {tag:"flight-live-engine-fail", flight:"LIVE", engine:"FAIL", expected:"FAIL", usable:false, degraded:false},
            {tag:"flight-fail-engine-live", flight:"FAIL", engine:"LIVE", expected:"LIVE", usable:true, degraded:false},
            {tag:"flight-lost-engine-sim", flight:"LOST", engine:"SIM", expected:"SIM", usable:true, degraded:false},
            {tag:"engine-stale", flight:"LIVE", engine:"STALE", expected:"STALE", usable:true, degraded:true},
            {tag:"engine-degraded", flight:"LIVE", engine:"DEGRADED", expected:"DEGRADED", usable:true, degraded:true},
            {tag:"engine-forced-sim", flight:"LIVE", engine:"SIM FORCED", expected:"SIM FORCED", usable:true, degraded:false},
            {tag:"unknown-contract", flight:"LIVE", engine:"WAITING", expected:"LOST", usable:false, degraded:false},
            {tag:"empty-contract", flight:"LIVE", engine:"", expected:"LOST", usable:false, degraded:false}
        ]
    }

    function test_independent_domains(row) {
        fixture.dataSourceState = row.flight
        fixture.dataAgeMs = 7
        fixture.engineSourceState = row.engine
        fixture.engineDataAgeMs = 2400
        compare(view.normalizedDataState, row.expected)
        compare(view.dataUsable, row.usable)
        compare(view.dataDegraded, row.degraded)
        compare(view.dataAgeMs, 2400)
        var consumers = stateConsumers(view, [])
        verify(consumers.length >= 10, "real gauges, bars, airframe, fuel, electrical and badges required")
        var gauges = 0
        for (var i = 0; i < consumers.length; ++i) {
            compare(consumers[i].dataState, row.expected)
            if (consumers[i].dataAgeMs !== undefined)
                compare(consumers[i].dataAgeMs, 2400)
            if (consumers[i].ageMs !== undefined)
                compare(consumers[i].ageMs, 2400)
            if (consumers[i].usable !== undefined && consumers[i].label !== undefined) {
                ++gauges
                compare(consumers[i].usable, row.usable && row.expected!=="STALE")
            }
        }
        compare(gauges, 5)
        if (!row.usable || row.degraded) {
            compare(view.casHeadline, "ENGINE DATA " + row.expected)
            compare(view.casLevel, row.usable ? "CAUTION" : "WARNING")
        }
        // Reversing the flight state cannot heal or invalidate the engine.
        fixture.dataSourceState = row.flight === "LIVE" ? "FAIL" : "LIVE"
        fixture.dataAgeMs = 99999
        compare(view.normalizedDataState, row.expected)
        compare(view.dataAgeMs, 2400)
        if (!row.usable) {
            view.startLean()
            compare(view.leanActive, false)
            compare(view.checklistComplete, false)
        }
    }

    function test_invalid_age_data() {
        return [{tag:"negative", age:-1}, {tag:"nan", age:NaN}, {tag:"infinite", age:Infinity}]
    }
    function test_invalid_age(row) {
        fixture.engineDataAgeMs = row.age
        fixture.dataAgeMs = 0
        compare(view.dataAgeMs, -1)
    }

    function test_missing_contract_fails_closed() {
        var legacy = {}
        for (var key in fixture) {
            if (key !== "engineSourceState" && key !== "engineDataAgeMs")
                legacy[key] = fixture[key]
        }
        legacy.dataSourceState = "LIVE"
        legacy.dataAgeMs = 0
        view.controller = legacy
        compare(view.normalizedDataState, "LOST")
        compare(view.dataAgeMs, -1)
        compare(view.dataUsable, false)
        var consumers = stateConsumers(view, [])
        for (var i = 0; i < consumers.length; ++i)
            compare(consumers[i].dataState, "LOST")
        view.controller = fixture
    }
}
