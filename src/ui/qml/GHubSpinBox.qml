import QtQuick
import QtQuick.Controls

// Campo numérico no estilo do G HUB (REFERENCIA-ESTILO-GHUB.md §4.3, item 7): sem caixa; o valor em negrito de
// 14 px centrado, botões planos "−" e "+" de 24 px nas pontas e um sublinhado de 1 px. O comportamento (from, to,
// stepSize, value, editable, valueModified) é o do SpinBox do Qt Quick Controls.
SpinBox {
    id: control

    implicitWidth: 132
    implicitHeight: 32
    leftPadding: 24
    rightPadding: 24
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4

    // Número puro, sem separador de milhar (o G HUB mostra "1600").
    textFromValue: function(value) {
        return String(value)
    }

    contentItem: TextInput {
        z: 2
        text: control.textFromValue(control.value, control.locale)
        font.pixelSize: 14
        font.bold: true
        color: Theme.foreground
        selectionColor: Theme.primary
        selectedTextColor: Theme.foreground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }

    down.indicator: Item {
        objectName: "spinMinus"
        x: 0
        width: 24
        height: control.height
        opacity: control.value > control.from ? 1 : 0.3

        Label {
            anchors.centerIn: parent
            text: "−"
            font.pixelSize: 18
            color: Theme.foreground
        }
    }

    up.indicator: Item {
        objectName: "spinPlus"
        x: control.width - width
        width: 24
        height: control.height
        opacity: control.value < control.to ? 1 : 0.3

        Label {
            anchors.centerIn: parent
            text: "+"
            font.pixelSize: 18
            color: Theme.foreground
        }
    }

    background: Rectangle {
        color: "transparent"

        Rectangle {
            objectName: "spinUnderline"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.fieldUnderline
        }
    }
}
