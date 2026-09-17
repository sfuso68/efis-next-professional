import QtQuick

Window {
    id:root
    width:1024; height:600; visible:true; color:"#03111C"
    title:"AVIONIX SKYVIEW GOLDEN • NOT FOR FLIGHT"
    HmiGoldenController { id:fixture }
    AdaptivePfd {
        anchors.fill:parent
        controller:fixture
        goldenMode:true
    }
}
