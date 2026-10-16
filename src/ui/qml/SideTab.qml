import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Botão do trilho de ícones de uma página de dispositivo (INC-014 parte 3, R21): 48×48, ícone SVG próprio de 24 px,
// tooltip com o nome da aba. O selecionado tem fundo primário; a opacidade do item marca os estados.
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
    icon.color: Theme.foreground
    Accessible.name: text

    // CSS computado §5.2: o ícone é sempre branco; a cor vem da opacidade do item (o hover escurece).
    opacity: selected ? (down ? 0.8 : (hovered ? 0.9 : 1)) : (down ? 0.2 : (hovered ? 0.4 : 0.6))
    Behavior on opacity {
        NumberAnimation {
            duration: Theme.mediumDuration
            easing.type: Easing.InOutQuad
        }
    }

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

        // Foco de teclado: contorno de 3 px para dentro.
        Rectangle {
            visible: tab.visualFocus
            anchors.fill: parent
            radius: Theme.cardRadius
            color: "transparent"
            border.width: 3
            border.color: Theme.focusRing
        }
    }
}
