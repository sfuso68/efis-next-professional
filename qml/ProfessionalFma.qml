import QtQuick

Rectangle {
    id: fma
    required property var controller
    property real uiScale: 1
    Theme { id: theme }
    color: "#F0050D14"; border.color: theme.border

    Text { x: 14*fma.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "EFIS NEXT";
        color: theme.primary; font.pixelSize: 18*fma.uiScale; font.bold: true }
    Text { x: 111*fma.uiScale; anchors.verticalCenter: parent.verticalCenter; text: "PFD PROFESSIONAL";
        color: theme.cyan; font.pixelSize: 8*fma.uiScale; font.bold: true }

    Row { anchors.centerIn: parent; spacing: 5*fma.uiScale
        Repeater { model: [
            {k:"AP",v:controller.flightDirectorActive ? "ACTIVE" : "OFF",c:controller.flightDirectorActive ? theme.green : theme.secondary},
            {k:"LATERAL",v:"HDG",c:theme.green}, {k:"VERTICAL",v:"ALT",c:theme.green},
            {k:"ALT SEL",v:Math.round(controller.selectedAltitude)+" FT",c:theme.cyan}
        ]; delegate: Rectangle { required property var modelData; width: 88*fma.uiScale; height: 42*fma.uiScale;
                color: "#0A151F"; border.color: modelData.c; radius: 2*fma.uiScale
                Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 4*fma.uiScale } text: modelData.k; color: theme.secondary; font.pixelSize: 6*fma.uiScale; font.bold: true }
                Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 5*fma.uiScale } text: modelData.v; color: modelData.c; font.pixelSize: 11*fma.uiScale; font.bold: true }
        } }
    }

    Rectangle { anchors { right: parent.right; top: parent.top; bottom: parent.bottom } width: 190*fma.uiScale;
        color: controller.annunciationSeverity>0 ? "#301C1207" : "#08121A"; border.color: controller.annunciationSeverity>0 ? theme.amber : theme.border
        Text { anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 7*fma.uiScale } text: "SYSTEM STATUS"; color: theme.secondary; font.pixelSize: 7*fma.uiScale; font.bold: true }
        Text { anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 8*fma.uiScale } text: controller.annunciation;
            color: controller.annunciationSeverity>0 ? theme.amber : theme.green; font.pixelSize: 10*fma.uiScale; font.bold: true }
    }
}
