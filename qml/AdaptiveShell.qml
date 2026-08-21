import QtQuick

Item {
    id: shell
    required property var controller
    property string initialPage: "PFD"
    property string currentPage: initialPage
    property real uiScale: Math.min(width/1024, height/600)

    AdaptivePfd { anchors.fill: parent; controller: shell.controller; visible: shell.currentPage === "PFD"; onNavigationRequested: shell.currentPage = "NAV" }
    AdaptiveCanvasView { anchors.fill: parent; controller: shell.controller; visible: shell.currentPage === "CANVAS" }
    AdaptiveMissionView { anchors.fill: parent; controller: shell.controller; visible: shell.currentPage === "MISSION" }
    AdaptiveNavigationView { anchors.fill: parent; controller: shell.controller; visible: shell.currentPage === "NAV" }
    AdaptiveEmsView { anchors.fill: parent; controller: shell.controller; visible: shell.currentPage === "EMS" }
    AdaptiveFpvView { anchors.fill: parent; controller: shell.controller; visible: shell.currentPage === "FPV"; onAutoRevertRequested: shell.currentPage = "PFD" }
    PageSelector {
        anchors { right: parent.right; top: parent.top;
            rightMargin: ((shell.currentPage === "PFD" || shell.currentPage === "CANVAS") ? 198 : 8)*shell.uiScale;
            topMargin: ((shell.currentPage === "PFD" || shell.currentPage === "CANVAS") ? 12 : 62)*shell.uiScale }
        z: 80; uiScale: shell.uiScale; currentPage: shell.currentPage
        onPageRequested: page => shell.currentPage = page
    }
}
