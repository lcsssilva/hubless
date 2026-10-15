import QtQuick
import QtQuick.Controls

// Caixa de seleção no estilo do G HUB (INC-014 parte 3, R23): quadrado de 14×14 com borda de 2 px e sem raio;
// marcada, fica preenchida com a marca escura. O texto vai à direita, em negrito de 13 px.
CheckBox {
    id: control

    spacing: 9
    leftPadding: 0
    rightPadding: 0
    topPadding: 6
    bottomPadding: 6

    indicator: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 14
        implicitHeight: 14
        radius: 0
        color: control.checked ? (control.enabled ? Theme.sliderFill : Theme.foregroundDisabled) : "transparent"
        border.width: 2
        border.color: control.enabled ? Theme.sliderFill : Theme.foregroundDisabled

        Label {
            anchors.centerIn: parent
            visible: control.checked
            text: "✓"
            font.pixelSize: 11
            font.bold: true
            color: Theme.background
        }
    }

    contentItem: Label {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        wrapMode: Text.WordWrap
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 13
        font.bold: true
        color: control.enabled ? Theme.foreground : Theme.foregroundDisabled
    }
}
