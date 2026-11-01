import QtQuick

Rectangle {
    id: root
    width: 1024; height: 600
    color: tokens.background
    HmiTokens { id: tokens }
    Text { x: 24; y: 18; text: "AVIONIX NEXT  •  HMI CONTRACT V1"; color: tokens.foreground; font.pixelSize: 22; font.bold: true }
    HmiArcGaugeV4 { x:20; y:56; width:270; height:270; label:"RPM"; value:5280; unit:"RPM"; minimum:0; maximum:6000; greenStart:1400; greenEnd:5500; majorStep:1000; minorStep:200; dominant:true; dataState:"SIM" }
    HmiCylinderMatrixV2 { x:300; y:60; width:440; height:200; chtValues:[104,108,111,106]; egtValues:[750,765,742,758]; dataState:"SIM"; limitsVerified:false }
    HmiAirframePanel { x:750; y:60; width:250; height:200; gearInstalled:true; noseLocked:true; leftLocked:true; rightLocked:true; gearState:"DOWN & LOCKED"; flapsInstalled:true; flapAngle:15; flapCommandedAngle:15; dataState:"SIM"; dataAgeMs:0; gearValid:true; flapAngleValid:true; flapCommandedValid:true }
    HmiFuelStatus { x:300; y:275; width:300; height:70; leftQuantity:11; rightQuantity:10.7; leftValid:true; rightValid:true; reserve:5; dataState:"SIM" }
    HmiElectricalSynoptic { x:610; y:275; width:390; height:70; busVoltage:14.2; alternatorCurrent:12; busVoltageValid:true; alternatorCurrentValid:true; dataState:"SIM" }
    HmiDataStateBadge { x:22; y:300; dataState:"SIM" }
    SkyViewCompassArc {
        x: 70; y: 350; width: 860; height: 230
        heading: 358; selectedCourse: 3; cdiDeviation: -0.25
        waypoint: "LIDB"; distanceNm: 12.4; ete: "06:18"; dataState: "SIM"
    }
}
