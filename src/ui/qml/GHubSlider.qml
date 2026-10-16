import QtQuick
import QtQuick.Controls

// Slider no estilo do G HUB (INC-014 parte 3, R23): trilho de 2 px (vazio `sliderTrack`, cheio `sliderFill`) e
// polegar branco circular de 12 px. Serve nas duas orientações; o comportamento é o do Slider do Qt Quick Controls.
// Opcionalmente (horizontal): o valor em negrito de 14 px à direita, na altura do trilho (`showValue`), e os extremos
// "from"/"to" em 11 px abaixo do trilho (`showExtremes`).
Slider {
    id: control

    property bool showValue: false
    property bool showExtremes: false
    // Texto do valor à direita; o padrão é o valor arredondado.
    property string valueText: String(Math.round(control.value))

    padding: 0
    // Com valor, o trilho ocupa 65 % da linha (CSS computado §5.3).
    rightPadding: horizontal && showValue ? Math.round(width * 0.35) : 0
    bottomPadding: horizontal && showExtremes ? 16 : 0
    implicitWidth: horizontal ? 200 : 20
    implicitHeight: horizontal ? (showExtremes ? 36 : 20) : 200

    background: Rectangle {
        objectName: "sliderTrack"
        x: control.leftPadding + (control.horizontal ? 0 : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal ? (control.availableHeight - height) / 2 : 0)
        width: control.horizontal ? control.availableWidth : 2
        height: control.horizontal ? 2 : control.availableHeight
        color: control.enabled ? Theme.sliderTrack : Theme.outline

        // Trecho cheio: do início do trilho até o centro do polegar.
        Rectangle {
            objectName: "sliderFill"
            readonly property real reach: control.horizontal
                ? control.handle.x + control.handle.width / 2
                : control.handle.y + control.handle.height / 2
            x: 0
            y: control.horizontal ? 0 : reach
            width: control.horizontal ? reach : 2
            height: control.horizontal ? 2 : parent.height - reach
            color: control.enabled ? Theme.sliderFill : Theme.foregroundDisabled
        }
    }

    handle: Rectangle {
        x: control.leftPadding + (control.horizontal
            ? control.visualPosition * (control.availableWidth - width) : (control.availableWidth - width) / 2)
        y: control.topPadding + (control.horizontal
            ? (control.availableHeight - height) / 2 : control.visualPosition * (control.availableHeight - height))
        implicitWidth: 12
        implicitHeight: 12
        width: 12
        height: 12
        radius: 6
        color: control.enabled ? Theme.foreground : Theme.foregroundMedium
    }

    // Sombra do polegar (-4px 0 4px #000): degradê preto à esquerda dele.
    Rectangle {
        visible: control.horizontal
        x: control.handle.x - 6
        y: control.handle.y
        width: 6
        height: control.handle.height
        z: -1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "transparent" }
            GradientStop { position: 1; color: "#000000" }
        }
    }

    Label {
        objectName: "sliderValue"
        visible: control.horizontal && control.showValue
        width: 40
        x: control.width - width
        y: control.topPadding + control.availableHeight / 2 - height / 2
        text: control.valueText
        horizontalAlignment: Text.AlignRight
        font.pixelSize: 14
        font.bold: true
        color: control.enabled ? Theme.foreground : Theme.foregroundDisabled
    }

    Label {
        objectName: "sliderMinLabel"
        visible: control.horizontal && control.showExtremes
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 + 18 - height / 2
        text: String(Math.round(control.from))
        font.pixelSize: 11
        color: Theme.dropdownItem
    }

    Label {
        objectName: "sliderMaxLabel"
        visible: control.horizontal && control.showExtremes
        x: control.leftPadding + control.availableWidth - width
        y: control.topPadding + control.availableHeight / 2 + 18 - height / 2
        text: String(Math.round(control.to))
        font.pixelSize: 11
        color: Theme.dropdownItem
    }
}
