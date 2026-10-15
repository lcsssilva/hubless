import QtQuick
import QtQuick.Controls

// Painel de superfície com título de 24 px (INC-014 parte 3, R22): raio de cartão e conteúdo abaixo do título.
// Quem usa declara um único filho (uma pilha ou uma rolagem) que preenche a área de conteúdo.
Rectangle {
    id: panel

    property string title: ""
    default property alias content: contentArea.data

    color: Theme.surface
    radius: Theme.cardRadius

    Label {
        id: titleLabel
        objectName: "panelTitle"
        x: 16
        y: 14
        width: parent.width - 32
        elide: Text.ElideRight
        text: panel.title
        font.pixelSize: 24
        font.weight: Font.Medium
    }

    Item {
        id: contentArea
        y: titleLabel.y + titleLabel.height + 8
        width: parent.width
        height: parent.height - y
    }
}
