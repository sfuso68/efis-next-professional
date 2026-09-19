import QtQuick

Window {
    id:root
    width:1024; height:600; visible:true; color:"#03111C"
    title:"AVIONIX HMI GOLDEN • NOT FOR FLIGHT"
    HmiGoldenController { id:fixture }
    AdaptiveEmsView {
        anchors.fill:parent
        controller:fixture
        clockOverride:"12:45:30"
    }
}
