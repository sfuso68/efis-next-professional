import QtQuick
import QtTest
import "../../qml" as Ui

TestCase {
    id: tests
    name:"WeatherReportFreshness"
    when:windowShown
    visible:true
    width:400; height:180
    Ui.WeatherReportCard { id:card; width:400; height:180 }
    function test_states_data() {
        return [{tag:"current",state:"CURRENT",sim:false,show:true},
                {tag:"stale",state:"STALE",sim:false,show:true},
                {tag:"expired",state:"EXPIRED",sim:false,show:false},
                {tag:"unknown-time",state:"UNKNOWN TIME",sim:false,show:false},
                {tag:"future",state:"CLOCK SKEW",sim:false,show:false},
                {tag:"not-started",state:"NOT YET VALID",sim:false,show:false},
                {tag:"unknown-state",state:"WAITING",sim:false,show:false},
                {tag:"training",state:"SIMULATED",sim:true,show:true},
                {tag:"sim-mismatch",state:"CURRENT",sim:true,show:false}]
    }
    function test_states(row) {
        card.taf=false
        card.report={icao:"LIPH",metar:"VFR RESIDUAL",metarState:row.state,metarAgeMinutes:90,simulated:row.sim}
        compare(card.usable,row.show)
        compare(findChild(card,"weatherReportReading").text,row.show?"VFR RESIDUAL":"NO USABLE REPORT")
        compare(card.current,row.state==="CURRENT"&&!row.sim)
        verify(findChild(card,"weatherReportStatus").text.indexOf(row.state)>=0)
    }
    function test_independent_taf_and_metar() {
        card.report={icao:"LIPH",metar:"OLD METAR",metarState:"EXPIRED",taf:"CURRENT TAF",tafState:"CURRENT",simulated:false}
        card.taf=false; compare(card.usable,false)
        card.taf=true; compare(card.reading,"CURRENT TAF")
    }
    function test_missing_contract_is_unavailable() {
        card.report={icao:"LIPH",metar:"OLD VFR"};card.taf=false
        compare(card.usable,false)
        compare(card.reading,"NO USABLE REPORT")
    }
    function test_expired_record_keeps_cause_not_residual_text() {
        card.taf=false
        card.report={icao:"LIPH",metar:"",rawMetar:"OLD VFR",metarState:"EXPIRED",simulated:false}
        compare(card.state,"EXPIRED")
        compare(card.usable,false)
        compare(card.reading,"NO USABLE REPORT")
        card.report={icao:"LIPH",metarState:"CURRENT",simulated:false}
        compare(card.current,false)
        compare(card.state,"NO DATA")
    }
}
