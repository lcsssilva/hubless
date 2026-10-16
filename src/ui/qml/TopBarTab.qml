import QtQuick
import QtQuick.Controls

// Aba de texto da barra do topo (INC-014 parte 3, R19): a ativa fica na cor primária e ganha um indicador de
// 24×1 px (40 px no hover) centrado no texto, colado ao divisor; o hover de qualquer aba deixa o texto azul; as demais usam a cor de aba inativa. Só apresenta e emite `clicked`.
AbstractButton {
    id: tab

    property bool active: false

    // CSS computado §3: faixa de navegação de 48 px; preenchimento 10 em cima, 16 nas laterais e 19 embaixo.
    topPadding: 10
    leftPadding: 16
    rightPadding: 16
    bottomPadding: 19
    implicitWidth: label.implicitWidth + 32
    implicitHeight: 48
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: text

    contentItem: Label {
        id: label
        text: tab.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 14
        font.letterSpacing: -0.42
        font.bold: tab.active
        color: tab.active || tab.hovered ? Theme.primary : Theme.tabForeground
    }

    background: Item {
        Rectangle {
            objectName: "tabUnderline"
            visible: tab.active || tab.hovered
            width: tab.hovered ? 40 : 24
            height: 1
            x: (parent.width - width) / 2
            y: parent.height - height
            color: Theme.primary

            Behavior on width {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.InOutQuad
                }
            }
        }

        // Foco de teclado: o indicador some e entra o contorno branco.
        Rectangle {
            visible: tab.visualFocus
            anchors.fill: parent
            anchors.margins: 2
            radius: Theme.radius
            color: "transparent"
            border.width: 2
            border.color: Theme.foreground
        }
    }
}
