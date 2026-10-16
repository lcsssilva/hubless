import QtQuick
import QtQuick.Controls

// Botão de rádio no estilo do G HUB (REFERENCIA-ESTILO-GHUB.md §4.3): círculo de 16 px com borda de 1 px
// (`sliderFill`); marcado, ganha um miolo preenchido. O texto fica à direita, em 13 px.
RadioButton {
    id: control

    spacing: 8
    leftPadding: 0
    rightPadding: 0
    topPadding: 4
    bottomPadding: 4
    hoverEnabled: true

    indicator: Rectangle {
        objectName: "radioRing"
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 16
        implicitHeight: 16
        radius: width / 2
        color: "transparent"
        border.width: 1
        border.color: control.enabled ? Theme.sliderFill : Theme.foregroundDisabled

        Rectangle {
            objectName: "radioDot"
            anchors.centerIn: parent
            width: 8
            height: 8
            radius: 4
            visible: control.checked
            color: control.enabled ? Theme.sliderFill : Theme.foregroundDisabled
        }
    }

    contentItem: Label {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 13
        color: control.enabled ? Theme.foreground : Theme.foregroundDisabled
    }
}
