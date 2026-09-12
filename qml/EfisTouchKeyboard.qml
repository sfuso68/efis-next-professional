import QtQuick

Item {
    id: keyboard
    property var target: null
    property real uiScale: 1
    property string fieldLabel: "TEXT ENTRY"
    property bool upperCase: true
    property bool symbols: false
    visible: false
    z: 500

    readonly property color cyan: "#28D9F4"
    readonly property color line: "#40535E"
    readonly property var alphaRows: [
        ["1","2","3","4","5","6","7","8","9","0"],
        ["q","w","e","r","t","y","u","i","o","p"],
        ["a","s","d","f","g","h","j","k","l","-"],
        ["z","x","c","v","b","n","m",".","/","_"]
    ]
    readonly property var symbolRows: [
        ["1","2","3","4","5","6","7","8","9","0"],
        ["[","]","{","}","(",")","<",">","=","+"],
        ["\"",":",",",";","-","_",".","/","\\","@"],
        ["#","%","*","?","!","&","|","'","^","~"]
    ]

    function openFor(item, label) {
        target = item
        fieldLabel = label || "TEXT ENTRY"
        visible = true
        if (target) target.forceActiveFocus()
    }
    function closeKeyboard() {
        visible = false
        if (target) target.focus = false
    }
    function insertCharacter(character) {
        if (!target) return
        var value = String(character)
        if (!symbols && upperCase && /^[a-z]$/.test(value)) value = value.toUpperCase()
        var start = target.selectionStart
        var end = target.selectionEnd
        if (start !== end) target.remove(start, end)
        var position = target.cursorPosition
        target.insert(position, value)
        target.cursorPosition = position + value.length
    }
    function backspace() {
        if (!target) return
        var start = target.selectionStart
        var end = target.selectionEnd
        if (start !== end) {
            target.remove(start, end)
            return
        }
        var position = target.cursorPosition
        if (position > 0) target.remove(position - 1, position)
    }
    function clearText() {
        if (target) target.text = ""
    }

    Rectangle {
        anchors.fill: parent
        color: "#D9061018"
        TapHandler { }
    }
    Rectangle {
        id: panel
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Math.min(parent.height, 258 * keyboard.uiScale)
        color: "#FA07141C"
        border.color: keyboard.cyan
        border.width: 1

        Text {
            x: 10 * keyboard.uiScale; y: 6 * keyboard.uiScale
            text: keyboard.fieldLabel
            color: keyboard.cyan
            font.pixelSize: 7 * keyboard.uiScale
            font.bold: true
        }
        Text {
            x: 10 * keyboard.uiScale; y: 22 * keyboard.uiScale
            width: parent.width - 20 * keyboard.uiScale
            elide: Text.ElideLeft
            text: keyboard.target ? keyboard.target.text : ""
            color: "white"
            font.pixelSize: 10 * keyboard.uiScale
            font.bold: true
        }

        Column {
            id: keyRows
            anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 43 * keyboard.uiScale }
            spacing: 3 * keyboard.uiScale
            Repeater {
                model: keyboard.symbols ? keyboard.symbolRows : keyboard.alphaRows
                delegate: Row {
                    required property var modelData
                    width: keyRows.width
                    height: 32 * keyboard.uiScale
                    spacing: 3 * keyboard.uiScale
                    Repeater {
                        model: parent.modelData
                        delegate: Rectangle {
                            required property string modelData
                            width: (keyRows.width - 27 * keyboard.uiScale) / 10
                            height: 32 * keyboard.uiScale
                            radius: 2 * keyboard.uiScale
                            color: "#16303C"
                            border.color: keyboard.line
                            Text { anchors.centerIn: parent; text: (!keyboard.symbols && keyboard.upperCase && /^[a-z]$/.test(modelData)) ? modelData.toUpperCase() : modelData; color: "white"; font.pixelSize: 9 * keyboard.uiScale; font.bold: true }
                            TapHandler { onTapped: keyboard.insertCharacter(modelData) }
                        }
                    }
                }
            }
        }

        Row {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 7 * keyboard.uiScale }
            height: 35 * keyboard.uiScale
            spacing: 4 * keyboard.uiScale
            Repeater {
                model: [
                    {label:keyboard.symbols ? "ABC" : "SYM", action:"symbols", weight:1},
                    {label:keyboard.upperCase ? "LOWER" : "UPPER", action:"case", weight:1},
                    {label:"CLEAR", action:"clear", weight:1},
                    {label:"SPACE", action:"space", weight:2},
                    {label:"⌫", action:"back", weight:1},
                    {label:"DONE", action:"done", weight:1}
                ]
                delegate: Rectangle {
                    required property var modelData
                    width: ((parent.width - 24 * keyboard.uiScale) / 7) * Number(modelData.weight)
                    height: parent.height
                    radius: 3 * keyboard.uiScale
                    color: modelData.action === "done" ? "#15503B" : "#17313D"
                    border.color: modelData.action === "done" ? "#52DE36" : keyboard.cyan
                    Text { anchors.centerIn: parent; text: modelData.label; color: "white"; font.pixelSize: 7 * keyboard.uiScale; font.bold: true }
                    TapHandler {
                        onTapped: {
                            if (modelData.action === "symbols") keyboard.symbols = !keyboard.symbols
                            else if (modelData.action === "case") keyboard.upperCase = !keyboard.upperCase
                            else if (modelData.action === "clear") keyboard.clearText()
                            else if (modelData.action === "space") keyboard.insertCharacter(" ")
                            else if (modelData.action === "back") keyboard.backspace()
                            else if (modelData.action === "done") keyboard.closeKeyboard()
                        }
                    }
                }
            }
        }
    }
}
