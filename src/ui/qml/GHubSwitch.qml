import QtQuick
import QtQuick.Controls

// Interruptor 54×24 no estilo do G HUB (REFERENCIA-ESTILO-GHUB.md §4.3): pílula; desligado tem trilho `switchOff` e
// a bolinha branca à esquerda; ligado tem o trilho primário e a bolinha à direita.
// O estado exibido (`checked`) vem sempre de quem usa (ViewModel): o clique não alterna nada sozinho, só emite
// `toggleRequested(wanted)` com o estado pedido; se o ViewModel aceitar, `checked` muda por vínculo.
AbstractButton {
    id: control

    // Estado pedido pelo clique (o oposto do exibido).
    signal toggleRequested(bool wanted)

    implicitWidth: 54
    implicitHeight: 24
    hoverEnabled: true
    checkable: false
    opacity: enabled ? 1 : 0.4
    Accessible.role: Accessible.CheckBox
    Accessible.checked: control.checked
    Accessible.name: text

    onClicked: control.toggleRequested(!control.checked)

    contentItem: Item {}

    background: Rectangle {
        objectName: "switchTrack"
        radius: height / 2
        color: control.checked ? Theme.primary : Theme.switchOff

        Behavior on color {
            ColorAnimation {
                duration: Theme.shortDuration
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easingCurve
            }
        }

        Rectangle {
            objectName: "switchKnob"
            width: 18
            height: 18
            radius: 9
            y: (parent.height - height) / 2
            x: control.checked ? parent.width - width - 3 : 3
            color: Theme.foreground

            Behavior on x {
                NumberAnimation {
                    duration: Theme.shortDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: Theme.easingCurve
                }
            }
        }
    }
}
