import QtQuick

QtObject {
    property string dataSourceState:typeof hmiGoldenDataState!=="undefined"?hmiGoldenDataState:"SIM"
    property int dataAgeMs:typeof hmiGoldenDataAgeMs!=="undefined"?hmiGoldenDataAgeMs:0
    property int engineHealth:0
    property real engineRpm:5060
    property real manifoldPressure:24.3
    property real oilPressure:58
    property real oilTemperature:96
    property real fuelFlow:4.8
    property real fuelLeft:10.6
    property real fuelRight:10.6
    property real busVoltage:14.0
    property real alternatorCurrent:17
    property var chtValues:[107,107,107,107]
    property var egtValues:[777,781,789,800]
    property var engineLimits:({rpm_max:5800,oil_pressure_min_psi:29,oil_pressure_max_psi:102,oil_temperature_max_c:130,cht_max_c:135,egt_max_c:900})
    property var engineCapabilities:({mixture_control:false})
    property string engineProfileName:"ROTAX 912 UL • GOLDEN FIXTURE"
    property bool landingGearInstalled:true
    property bool gearNoseLocked:true
    property bool gearLeftLocked:true
    property bool gearRightLocked:true
    property string gearState:"DOWN & LOCKED"
    property bool flapsInstalled:true
    property var flapDetents:[0,10,15,20,30]
    property real flapAngle:15
    property real flapCommandedAngle:15
    property string unitSystem:"AVIATION"
    property real fuelUsedGallons:3.8
    property real fuelReserveGallons:5.0
    property real fuelOnBoardGallons:25.0
    property real fuelRemainingGallons:21.2
    property var fuelCalibrationLeft:[0,1,2,3,4]
    property var fuelCalibrationRight:[0,1,2,3,4]
    property string fuelLeakState:"NORMAL"
    property real fuelLeakExcessGph:0
    property string centralBlackBoxState:"ONLINE"
    property string blackBoxState:"ONLINE"
    property real airspeed:96
    property real selectedAirspeed:100
    property bool selectedAirspeedValid:true
    property real airspeedTrend:3
    property bool airspeedValid:true
    property real altitude:4200
    property real selectedAltitude:4500
    property bool selectedAltitudeValid:true
    property real altitudeTrend:180
    property bool altitudeValid:true
    property real verticalSpeed:350
    property bool verticalSpeedValid:true
    property real roll:-4
    property real pitch:2
    property bool attitudeValid:true
    property real heading:327
    property bool headingValid:true
    property real latitude:46.1648
    property real longitude:12.2487
    property bool positionValid:true
    property real groundSpeed:104
    property bool groundSpeedValid:true
    property real selectedCourse:332
    property bool selectedCourseValid:true
    property real commandedRoll:0
    property real commandedPitch:4
    property bool flightDirectorActive:true
    property bool flightDirectorGuidanceValid:true
    property bool navigationValid:true
    property var selectedAirport:({icao:"LIDB",latitude_deg:46.16655,longitude_deg:12.25039})
    property var aircraftSpeedProfile:({vso:39,vs1:45,vx:61,vy:68,vfe:81,vno:118,vne:145,va:97,bestGlide:65,vref:55})
    property bool aircraftSpeedProfileAvailable: true
    property string aircraftSpeedProfileMessage: "V-SPEEDS VERIFIED PROFILE"
    property string imageryState:"GOLDEN FIXTURE"
    property bool imageryOfflineReady:true
    property int imageryTileCount:24
    property int imageryCoverageGaps:0
    property string engineDataState:dataSourceState
    property string annunciation:"SYSTEM NORMAL"
    property int annunciationSeverity:0
    property string radarSource:""
    property string radarState:"SIMULATED"
    property string radarProduct:"REFLECTIVITY"
    property string radarArea:"VENETO"
    property int radarAgeMinutes:0
    property bool simulation:true
    property int simulationPhase:0
    function cycleProduct() {}
    function setUnitSystem(value) { unitSystem=value }
    function setFuelOnBoardGallons(value) { fuelOnBoardGallons=value; return true }
    function setFuelReserveGallons(value) { fuelReserveGallons=value; return true }
    function captureFuelCalibrationPoint(tank) { return tank.length>0 }
    function clearFuelCalibration(tank) { return tank.length>0 }
    function resetFuelUsed() { fuelUsedGallons=0 }
}
