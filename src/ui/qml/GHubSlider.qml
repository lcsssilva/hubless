import QtQuick
import QtQuick.Controls

// Slider no estilo do G HUB (INC-014 parte 3, R23): trilho de 2 px (vazio `sliderTrack`, cheio `sliderFill`) e
// polegar branco circular de 12 px. Serve nas duas orientações; o comportamento é o do Slider do Qt Quick Controls.
Slider {
    id: control

    padding: 0
    implicitWidth: horizontal ? 200 : 20
    implicitHeight: horizontal ? 20 : 200

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
}
