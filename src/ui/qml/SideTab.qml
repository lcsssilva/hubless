import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Botão do trilho de ícones de uma página de dispositivo (INC-014 parte 3, R21): 48×48, ícone de 24 px do tema do
// sistema, tooltip com o nome da aba. O selecionado tem fundo primário e ícone branco; os demais, ícone a 60%.
// Só apresenta e emite `clicked`; sem regra de negócio.
ToolButton {
    id: tab
    objectName: "sideTab"

    property string tabKey: ""
    property bool selected: false

    width: 48
    height: 48
    padding: 0
    hoverEnabled: true
    display: AbstractButton.IconOnly
    icon.width: 24
    icon.height: 24
    icon.color: tab.selected ? Theme.foreground : Theme.foregroundMedium
    Accessible.name: text

    ToolTip.visible: hovered
    ToolTip.text: text
    ToolTip.delay: 400

    background: Item {
        // Fundo do item selecionado: cor primária.
        Rectangle {
            objectName: "selectedFill"
            anchors.fill: parent
            radius: Theme.cardRadius
            color: Theme.primary
            opacity: tab.selected ? 1 : 0
        }

        // Camada de estado: realce de hover e de clique.
        Rectangle {
            objectName: "stateOverlay"
            anchors.fill: parent
            radius: Theme.cardRadius
            color: Theme.foreground
            opacity: tab.down ? Theme.pressOpacity : (tab.hovered ? Theme.hoverOpacity : 0)

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.shortDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: Theme.easingCurve
                }
            }
        }
    }
}
