import QtQuick

// Banco prova sensori del Setup Center.
//
// Mostra cio' che il controllore di volo sta dicendo davvero, e in quali
// condizioni. La regola che governa ogni scelta di questa pagina: un valore non
// utilizzabile non deve mai avere l'aspetto di un valore buono. Per questo il
// numero, quando non e' utilizzabile, non viene mostrato affatto e al suo posto
// compare il motivo. Un cruscotto scritto con leggerezza mostrerebbe qui
// numeri rassicuranti proprio nelle condizioni in cui il banco si trova: nessun
// fix satellitare e sensore differenziale dichiarato non sano.
Rectangle {
    id: panel

    property real uiScale: 1
    property var theme
    signal closed()

    color: "#FD061018"
    border.color: theme.cyan

    Text {
        id: title
        x: 18 * panel.uiScale; y: 14 * panel.uiScale
        text: "BANCO PROVA SENSORI • SOLA OSSERVAZIONE"
        color: theme.primary
        font.pixelSize: 17 * panel.uiScale
        font.bold: true
    }

    // Dichiarazione, non decorazione. Finche' ARSPD_USE resta a zero nulla di
    // quanto si vede qui entra nel controllo di volo, e va detto invece che
    // lasciato intendere.
    Rectangle {
        id: badge
        x: 18 * panel.uiScale
        y: 40 * panel.uiScale
        width: monitorText.width + 16 * panel.uiScale
        height: 22 * panel.uiScale
        radius: 2 * panel.uiScale
        color: bench.feedsFlightControl ? "#3A1512" : "#122A22"
        border.color: bench.feedsFlightControl ? theme.warning : theme.green
        Text {
            id: monitorText
            anchors.centerIn: parent
            text: bench.feedsFlightControl
                  ? "ATTENZIONE: ALIMENTA IL CONTROLLO DI VOLO"
                  : "MONITOR-ONLY • NON ALIMENTA IL CONTROLLO DI VOLO"
            color: bench.feedsFlightControl ? theme.warning : theme.green
            font.pixelSize: 8 * panel.uiScale
            font.bold: true
        }
    }

    Text {
        x: 18 * panel.uiScale
        y: 70 * panel.uiScale
        text: (bench.active ? (bench.sourceName + " • " + bench.message)
                            : "banco fermo") +
              "   GNSS: " + bench.gnssSummary +
              "   ARIA: " + bench.airDataSummary
        color: theme.secondary
        font.pixelSize: 9 * panel.uiScale
    }

    Text {
        x: 18 * panel.uiScale
        y: 88 * panel.uiScale
        visible: bench.statusText !== ""
        text: "AUTOPILOTA: " + bench.statusText
        color: theme.amber
        font.pixelSize: 9 * panel.uiScale
    }

    Row {
        id: controls
        anchors { right: parent.right; rightMargin: 14 * panel.uiScale }
        y: 12 * panel.uiScale
        spacing: 8 * panel.uiScale

        Rectangle {
            width: 108 * panel.uiScale; height: 36 * panel.uiScale
            color: "#14313D"; border.color: theme.cyan
            Text {
                anchors.centerIn: parent
                text: bench.active ? "ARRESTA" : "AVVIA REPLAY"
                color: theme.cyan
                font.pixelSize: 9 * panel.uiScale
                font.bold: true
            }
            TapHandler {
                onTapped: bench.active ? bench.stop()
                                       : bench.startReplay(bench.defaultSource)
            }
        }
        Rectangle {
            width: 82 * panel.uiScale; height: 36 * panel.uiScale
            color: "#18232B"; border.color: theme.border
            Text {
                anchors.centerIn: parent
                text: "INDIETRO"
                color: theme.primary
                font.pixelSize: 9 * panel.uiScale
                font.bold: true
            }
            TapHandler { onTapped: panel.closed() }
        }
    }

    // Intestazione della tabella. L'eta' e il valore grezzo hanno una colonna
    // propria: senza l'eta' un valore fermo e' indistinguibile da uno appena
    // arrivato, e senza il grezzo un sensore fermo e' indistinguibile da una
    // conversione sbagliata.
    Row {
        id: header
        x: 18 * panel.uiScale
        y: 112 * panel.uiScale
        spacing: 0
        Text { width: 190 * panel.uiScale; text: "GRANDEZZA"; color: theme.secondary; font.pixelSize: 8 * panel.uiScale; font.bold: true }
        Text { width: 120 * panel.uiScale; horizontalAlignment: Text.AlignRight; text: "VALORE"; color: theme.secondary; font.pixelSize: 8 * panel.uiScale; font.bold: true }
        Text { width: 46 * panel.uiScale; text: "   UNITÀ"; color: theme.secondary; font.pixelSize: 8 * panel.uiScale; font.bold: true }
        Text { width: 110 * panel.uiScale; horizontalAlignment: Text.AlignRight; text: "GREZZO"; color: theme.secondary; font.pixelSize: 8 * panel.uiScale; font.bold: true }
        Text { width: 84 * panel.uiScale; horizontalAlignment: Text.AlignRight; text: "ETÀ"; color: theme.secondary; font.pixelSize: 8 * panel.uiScale; font.bold: true }
        Text { width: 92 * panel.uiScale; text: "   SORGENTE"; color: theme.secondary; font.pixelSize: 8 * panel.uiScale; font.bold: true }
        Text { width: 200 * panel.uiScale; text: "STATO"; color: theme.secondary; font.pixelSize: 8 * panel.uiScale; font.bold: true }
    }

    Rectangle {
        anchors { left: header.left; top: header.bottom; topMargin: 4 * panel.uiScale }
        width: 842 * panel.uiScale
        height: 1
        color: theme.border
    }

    ListView {
        id: table
        anchors {
            left: parent.left; leftMargin: 18 * panel.uiScale
            right: parent.right; rightMargin: 14 * panel.uiScale
            top: header.bottom; topMargin: 10 * panel.uiScale
            bottom: footer.top; bottomMargin: 8 * panel.uiScale
        }
        clip: true
        model: bench.rows
        spacing: 1 * panel.uiScale

        delegate: Item {
            required property var modelData
            width: table.width
            height: 22 * panel.uiScale

            Rectangle {
                anchors.fill: parent
                color: modelData.usable ? "transparent" : "#140E0C"
            }

            Row {
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    width: 190 * panel.uiScale
                    text: modelData.name
                    color: modelData.usable ? theme.primary : theme.secondary
                    font.pixelSize: 9 * panel.uiScale
                }
                // Il valore compare soltanto quando e' utilizzabile. Mostrarlo
                // in grigio sarebbe comunque mostrarlo, e un numero letto di
                // sfuggita non porta con se' il colore con cui era scritto.
                Text {
                    width: 120 * panel.uiScale
                    horizontalAlignment: Text.AlignRight
                    text: modelData.usable ? modelData.value.toFixed(2) : "———"
                    color: modelData.usable ? theme.primary : theme.secondary
                    font.pixelSize: 10 * panel.uiScale
                    font.bold: modelData.usable
                }
                Text {
                    width: 46 * panel.uiScale
                    text: "   " + modelData.unit
                    color: theme.secondary
                    font.pixelSize: 8 * panel.uiScale
                }
                // Il grezzo si mostra sempre, anche quando il valore convertito
                // non e' utilizzabile: e' cio' che il sensore ha realmente
                // detto, ed e' il dato su cui si fa la taratura.
                Text {
                    width: 110 * panel.uiScale
                    horizontalAlignment: Text.AlignRight
                    text: modelData.raw.toFixed(3)
                    color: theme.secondary
                    font.pixelSize: 8 * panel.uiScale
                }
                Text {
                    width: 84 * panel.uiScale
                    horizontalAlignment: Text.AlignRight
                    text: modelData.ageMs >= 0 ? (modelData.ageMs + " ms") : "—"
                    color: modelData.ageMs > 2000 ? theme.amber : theme.secondary
                    font.pixelSize: 8 * panel.uiScale
                }
                Text {
                    width: 92 * panel.uiScale
                    text: "   " + modelData.source
                    color: modelData.simulated ? theme.amber : theme.secondary
                    font.pixelSize: 8 * panel.uiScale
                }
                Text {
                    width: 200 * panel.uiScale
                    text: modelData.note !== "" ? modelData.note : modelData.validity
                    color: modelData.usable ? theme.green : theme.amber
                    font.pixelSize: 8 * panel.uiScale
                }
            }
        }
    }

    // Salute del collegamento e dei nodi. Sono i numeri che dicono se il
    // silenzio di una grandezza viene dal sensore o dal filo.
    Rectangle {
        id: footer
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 46 * panel.uiScale
        color: "#07131D"
        border.color: theme.border

        Text {
            x: 18 * panel.uiScale
            y: 8 * panel.uiScale
            text: "COLLEGAMENTO  frame " + bench.link.frames +
                  "   checksum errati " + bench.link.crcErrors +
                  "   messaggi non interpretati " + bench.link.unknownMessages +
                  "   byte scartati " + bench.link.droppedBytes
            color: bench.link.crcErrors > 0 ? theme.amber : theme.secondary
            font.pixelSize: 8 * panel.uiScale
        }

        Row {
            x: 18 * panel.uiScale
            y: 26 * panel.uiScale
            spacing: 14 * panel.uiScale
            Text {
                text: "NODI"
                color: theme.secondary
                font.pixelSize: 8 * panel.uiScale
                font.bold: true
            }
            Repeater {
                model: bench.nodes
                delegate: Text {
                    required property var modelData
                    text: modelData.node + " " + modelData.state +
                          " (" + modelData.accepted + " accettati, " +
                          modelData.missed + " persi)"
                    color: modelData.online ? theme.green : theme.amber
                    font.pixelSize: 8 * panel.uiScale
                }
            }
        }
    }
}
