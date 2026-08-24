import QtQuick
import QtQuick.Controls

// Finestra del nodo EMS. Tiene la stessa geometria di riferimento 1024x600 del
// PFD perche' la pagina motore e' disegnata su quel rapporto: cambiarla qui
// significherebbe verificare su questo nodo un layout diverso da quello che poi
// finisce nel display completo.
ApplicationWindow {
    id: root
    visible: true
    width: 1024; height: 600
    minimumWidth: 800; minimumHeight: 480
    title: "EFIS NEXT Engine Monitoring"
    color: "#061018"

    AdaptiveEmsView {
        anchors.fill: parent
        controller: ems
        Component.onCompleted: {
            var blackboxPreview = Qt.application.arguments.indexOf("--setup-blackbox-preview") >= 0
            setupVisible = Qt.application.arguments.indexOf("--setup-preview") >= 0 || blackboxPreview
            if (blackboxPreview) setupSection = 4
        }
    }

    Shortcut { sequence: "Ctrl+F"; onActivated: ems.cycleEngineFault() }
    Shortcut { sequence: "Esc"; onActivated: root.showNormal() }
}
