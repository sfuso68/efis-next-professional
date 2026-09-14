import QtQuick

Rectangle {
    id: selector
    property real uiScale: 1
    property string currentPage: "PFD"
    property bool expanded: false
    signal pageRequested(string page)
    width: (expanded ? 104 : 76) * uiScale; height: (expanded ? 294 : 34) * uiScale
    color: "#08121B"; opacity: 0.96; border.color: "#314654"; radius: 3
    Column {
        anchors.fill: parent
        Repeater {
            model: selector.expanded ? ["PFD", "NAV", "RADIO", "CANVAS", "MISSION", "EMS", "FPV"] : ["MENU"]
            delegate: Rectangle {
                required property string modelData
                width: selector.width; height: selector.expanded ? selector.height / 7 : selector.height
                color: selector.currentPage === modelData ? "#123444" : "transparent"
                border.color: selector.currentPage === modelData ? "#19C3E6" : "#314654"
                Text { anchors.centerIn: parent; text: modelData; color: selector.currentPage === modelData ? "#19C3E6" : "#F2F0E9"; font.pixelSize: 10 * selector.uiScale; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: {
                    if (modelData === "MENU") selector.expanded = true
                    else { selector.pageRequested(modelData); selector.expanded = false }
                } }
            }
        }
    }
}
