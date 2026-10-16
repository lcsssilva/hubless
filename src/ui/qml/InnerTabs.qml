import QtQuick
import QtQuick.Controls

// Abas internas de um painel (REFERENCIA-ESTILO-GHUB.md §4.3): texto MAIÚSCULO de 14 px, intervalo de 16 px; a ativa
// é branca com sublinhado de 2 px da largura do texto a 8 px abaixo; a inativa fica a 60 % de opacidade e o hover
// leva a 100 % com texto cinza `sectionHeader` (CSS computado §5.3).
// `model` é a lista de textos; o clique muda `currentIndex` e emite `tabSelected(index)`.
Item {
    id: tabs

    property var model: []
    property int currentIndex: 0
    signal tabSelected(int index)

    implicitWidth: row.implicitWidth
    implicitHeight: 40

    Row {
        id: row
        spacing: 16

        Repeater {
            model: tabs.model

            delegate: AbstractButton {
                id: tab
                required property int index
                required property string modelData
                objectName: "innerTab"

                readonly property bool active: tab.index === tabs.currentIndex

                text: tab.modelData
                padding: 0
                hoverEnabled: true
                opacity: tab.active || tab.hovered ? 1 : 0.6
                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.mediumDuration
                        easing.type: Easing.InOutQuad
                    }
                }
                width: label.implicitWidth
                height: tabs.height
                Accessible.name: text
                onClicked: {
                    tabs.currentIndex = tab.index
                    tabs.tabSelected(tab.index)
                }

                contentItem: Label {
                    id: label
                    objectName: "innerTabLabel"
                    text: tab.text
                    verticalAlignment: Text.AlignTop
                    topPadding: 4
                    font.pixelSize: 14
                    font.weight: Font.Thin
                    font.letterSpacing: 1
                    font.capitalization: Font.AllUppercase
                    color: tab.hovered && !tab.active ? Theme.sectionHeader : Theme.foreground
                }

                background: Rectangle {
                    objectName: "innerTabUnderline"
                    x: 0
                    y: label.y + label.contentHeight + 8
                    width: label.contentWidth
                    height: 2
                    visible: tab.active
                    color: Theme.foreground
                }
            }
        }
    }
}
