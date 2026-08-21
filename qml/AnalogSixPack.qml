import QtQuick
import EfisInstruments

// Disposizione classica a sei strumenti.
//
// La fila superiore porta anemometro, orizzonte e altimetro; quella inferiore
// virosbandometro, girodirezionale e variometro. Non e' una scelta estetica: e'
// la disposizione su cui ogni pilota ha imparato a spostare lo sguardo, e
// cambiarla renderebbe piu' lenta proprio la scansione che deve essere
// automatica.
Item {
    id: page

    required property var source

    // I sei strumenti sono quadrati e la pagina e' larga: la dimensione la
    // detta l'altezza disponibile, non la larghezza.
    readonly property real cell: Math.min(width / 3, height / 2)

    Grid {
        anchors.centerIn: parent
        columns: 3
        spacing: 0

        InstrumentItem {
            width: page.cell; height: page.cell
            kind: InstrumentItem.LinearDial
            title: "IAS"; unit: "KT"
            minimum: 40; maximum: 160; majorStep: 20; minorStep: 5
            // Le fasce vengono dalle velocita' del profilo aeromobile attivo.
            bands: page.source.airspeedBands
            value: page.source.airspeed
            validity: page.source.airspeedValidity
            simulated: page.source.simulated
        }

        InstrumentItem {
            width: page.cell; height: page.cell
            kind: InstrumentItem.Attitude
            title: "ATT"
            value: page.source.roll
            validity: page.source.rollValidity
            secondValue: page.source.pitch
            secondValidity: page.source.pitchValidity
            simulated: page.source.simulated
        }

        InstrumentItem {
            width: page.cell; height: page.cell
            kind: InstrumentItem.Altimeter
            title: "ALT"; unit: "FT"
            value: page.source.altitude
            validity: page.source.altitudeValidity
            windowText: page.source.baroSetting
            simulated: page.source.simulated
        }

        InstrumentItem {
            width: page.cell; height: page.cell
            kind: InstrumentItem.TurnAndSlip
            title: "T/C"
            value: page.source.turnRate
            validity: page.source.turnRateValidity
            secondValue: page.source.slip
            secondValidity: page.source.slipValidity
            simulated: page.source.simulated
        }

        InstrumentItem {
            width: page.cell; height: page.cell
            kind: InstrumentItem.Heading
            title: "HDG"; unit: "DEG"
            value: page.source.heading
            validity: page.source.headingValidity
            secondValue: page.source.selectedCourse
            secondValidity: page.source.selectedCourseValidity
            simulated: page.source.simulated
        }

        InstrumentItem {
            width: page.cell; height: page.cell
            kind: InstrumentItem.LinearDial
            title: "VSI"; unit: "FT/MIN"
            minimum: -2000; maximum: 2000; majorStep: 500; minorStep: 100
            value: page.source.verticalSpeed
            validity: page.source.verticalSpeedValidity
            simulated: page.source.simulated
        }
    }
}
