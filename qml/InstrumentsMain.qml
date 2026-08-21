import QtQuick
import QtQuick.Window
import EfisInstruments

Window {
    id: window
    // La risoluzione del display del PFD. Fissarla qui rende la cattura
    // confrontabile fra una prova e l'altra.
    width: 1024
    height: 600
    visible: true
    color: "#071019"
    title: "EFIS NEXT — strumenti analogici"
    visibility: efisKiosk ? Window.FullScreen : Window.Windowed

    InstrumentSource {
        id: flight
        Component.onCompleted: {
            if (efisInjectedFailure !== "") injectFailure(efisInjectedFailure)
        }
    }

    AnalogSixPack {
        anchors.fill: parent
        source: flight
    }
}
