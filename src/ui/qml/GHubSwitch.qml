import QtQuick
import QtQuick.Controls

// Interruptor 48×24 no estilo do G HUB (ESTILO-GHUB-CSS-COMPUTADO.md §5.3): pílula; desligado tem trilho `switchOff` e
// a bolinha branca à esquerda; ligado tem o trilho primário e a bolinha à direita.
// O estado exibido (`checked`) vem sempre de quem usa (ViewModel): o clique não alterna nada sozinho, só emite
// `toggleRequested(wanted)` com o estado pedido; se o ViewModel aceitar, `checked` muda por vínculo.
AbstractButton {
    id: control

    // Estado pedido pelo clique (o oposto do exibido).
    signal toggleRequested(bool wanted)

    implicitWidth: 48
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
                duration: 90
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easingCurve
            }
        }

        // Hover: camada branca a 10 %; foco de teclado: contorno de 3 px.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Theme.foreground
            opacity: control.hovered ? Theme.hoverOpacity : 0
        }

        Rectangle {
            visible: control.visualFocus
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 3
            border.color: Theme.focusRing
        }

        Rectangle {
            objectName: "switchKnob"
            width: 16
            height: 16
            radius: 8
            y: (parent.height - height) / 2
            x: control.checked ? parent.width - width - 4 : 4
            color: Theme.foreground

            Behavior on x {
                NumberAnimation {
                    duration: 90
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: Theme.easingCurve
                }
            }
        }
    }
}
