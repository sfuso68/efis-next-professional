import QtQuick
import QtTest
import "../../qml" as Ui

TestCase {
    name:"MissionControls"
    when:windowShown
    visible:true
    width:1024; height:600
    Ui.HmiGoldenController {
        id:fixture
        property bool systemSimulationActive:false
        property bool systemSimulationRunning:false
        property string systemSimulationScenario:"mission-full-flight"
        property string systemSimulationPhase:"COLD_AND_DARK"
        property real systemSimulationProgress:0.5
        property real systemSimulationDurationSeconds:1820
        property real systemSimulationSpeed:1
        property string systemSimulationLastEvent:"TEST FIXTURE"
        property string engineFaultMode:"NONE"
        property string started:""
        function startSystemSimulation(name){started=name;systemSimulationScenario=name;systemSimulationActive=true}
        function toggleSystemSimulation(){systemSimulationRunning=!systemSimulationRunning}
        function stopSystemSimulation(){systemSimulationActive=false;systemSimulationRunning=false}
        function setSystemSimulationSpeed(speed){systemSimulationSpeed=speed}
        function seekSystemSimulation(fraction){systemSimulationProgress=fraction}
    }
    Ui.SystemSimulatorOverlay { id:overlay; width:1024; height:600; controller:fixture }
    function test_scenario_buttons_data(){return overlay.scenarios.map(function(s){return {tag:s.id,name:s.id}})}
    function test_scenario_buttons(row){
        var button=findChild(overlay,"scenario-"+row.name)
        verify(button!==null)
        mouseClick(button,button.width/2,button.height/2)
        compare(fixture.started,row.name)
    }
    function test_computed_duration_not_fixed_movie(){
        fixture.systemSimulationProgress=0.5
        fixture.systemSimulationDurationSeconds=1820
        compare(findChild(overlay,"missionTimelineText").text,"910 / 1820 s")
        fixture.systemSimulationDurationSeconds=2400
        compare(findChild(overlay,"missionTimelineText").text,"1200 / 2400 s")
    }
}
