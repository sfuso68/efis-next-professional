import QtQuick

Item {
    id: shell
    required property var controller
    property string initialPage: "PFD"
    property string currentPage: initialPage
    property real uiScale: Math.min(width/1024, height/600)

    // Una pagina nascosta non deve continuare a rivalutare centinaia di binding
    // avionici. Il Loader sincrono mantiene il cambio pagina immediato, ma
    // istanzia soltanto la vista realmente presentata.
    Loader {
        id: pageLoader
        anchors.fill: parent
        asynchronous: false
        sourceComponent: shell.currentPage === "PFD" ? pfdPage
                       : shell.currentPage === "CANVAS" ? canvasPage
                       : shell.currentPage === "MISSION" ? missionPage
                       : shell.currentPage === "NAV" ? navigationPage
                       : shell.currentPage === "EMS" ? emsPage
                       : shell.currentPage === "FPV" ? fpvPage
                       : shell.currentPage === "RADIO" ? radioPage
                       : shell.currentPage === "EMERGENCY" ? emergencyPage
                       : pfdPage
    }

    Component { id: pfdPage; AdaptivePfd { controller: shell.controller; onNavigationRequested: shell.currentPage = "NAV" } }
    Component { id: canvasPage; AdaptiveCanvasView { controller: shell.controller } }
    Component { id: missionPage; AdaptiveMissionView { controller: shell.controller } }
    Component { id: navigationPage; AdaptiveNavigationView { controller: shell.controller } }
    Component { id: emsPage; AdaptiveEmsView { controller: shell.controller } }
    Component { id: fpvPage; AdaptiveFpvView { controller: shell.controller; onAutoRevertRequested: shell.currentPage = "PFD" } }
    Component { id: radioPage; RadioAssistView { controller: shell.controller; onCloseRequested: shell.currentPage = "PFD" } }
    Component { id: emergencyPage; EmergencyAssistView { controller: shell.controller; onCloseRequested: shell.currentPage = "PFD" } }
    PageSelector {
        anchors { right: parent.right; top: parent.top;
            rightMargin: ((shell.currentPage === "PFD" || shell.currentPage === "CANVAS") ? 198 : 8)*shell.uiScale;
            topMargin: ((shell.currentPage === "PFD" || shell.currentPage === "CANVAS") ? 12 : 62)*shell.uiScale }
        z: 80; uiScale: shell.uiScale; currentPage: shell.currentPage
        onPageRequested: page => shell.currentPage = page
        visible: shell.currentPage !== "EMERGENCY" && shell.currentPage !== "RADIO"
    }
    Connections { target: shell.controller; function onEmergencyAssistChanged() { if (shell.controller.emergencyAssistActive) shell.currentPage = "EMERGENCY" } }
}
