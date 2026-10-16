import QtQuick
import QtQuick.Controls

// Campo de texto no estilo do G HUB (REFERENCIA-ESTILO-GHUB.md §4.3): sem caixa, só um sublinhado de 1 px;
// o texto de ajuda usa a cor secundária. Comportamento do TextField do Qt Quick Controls.
TextField {
    id: control

    implicitHeight: 32
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 4
    verticalAlignment: Text.AlignVCenter
    font.pixelSize: 14
    color: Theme.foreground
    placeholderTextColor: Theme.foregroundMedium
    selectionColor: Theme.primary
    selectedTextColor: Theme.foreground

    background: Item {
        Rectangle {
            objectName: "fieldUnderline"
            y: parent.height - height
            width: parent.width
            height: 1
            color: control.activeFocus ? Theme.sliderFill : Theme.fieldUnderline
        }
    }
}
