import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Aba da coluna lateral de uma página de dispositivo (INC-014 R4). Só mostra o texto e emite `clicked`.
AbstractButton {
    id: tab
    objectName: "sideTab"

    property string tabKey: ""
    property bool selected: false

    Layout.fillWidth: true
    implicitHeight: 40
    hoverEnabled: true

    contentItem: Label {
        text: tab.text
        leftPadding: 16
        verticalAlignment: Text.AlignVCenter
        font.bold: tab.selected
        color: tab.selected ? Theme.primary : Theme.foreground
    }

    background: Item {
        // Fundo da aba selecionada: cor primária com opacidade de seleção.
        Rectangle {
            objectName: "selectedFill"
            anchors.fill: parent
            radius: Theme.radius
            color: Theme.primary
            opacity: tab.selected ? Theme.selectedOpacity : 0
        }

        // Camada de estado: realce de hover e de clique.
        Rectangle {
            objectName: "stateOverlay"
            anchors.fill: parent
            radius: Theme.radius
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

        Rectangle {
            visible: tab.selected
            width: 3
            height: parent.height - 12
            anchors.verticalCenter: parent.verticalCenter
            radius: 1.5
            color: Theme.primary
        }
    }
}
