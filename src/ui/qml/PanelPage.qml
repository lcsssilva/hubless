import QtQuick
import QtQuick.Layouts

// Página de painel único e centralizado (Perfis e Configurações): a mesma superfície do painel lateral de um
// dispositivo, mas com largura de até 720 px. O conteúdo é uma rolagem própria (INC-014 parte 3).
Item {
    id: page

    property string title: ""
    default property alias content: stack.data

    PanelCard {
        objectName: "pagePanel"
        title: page.title
        anchors.horizontalCenter: parent.horizontalCenter
        y: 8
        width: Math.min(720, page.width - 48)
        height: page.height - 13

        StackLayout {
            id: stack
            anchors.fill: parent
        }
    }
}
