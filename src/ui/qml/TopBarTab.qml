import QtQuick
import QtQuick.Controls

// Aba de texto da barra do topo (INC-014 parte 3, R19): a ativa fica na cor primária e ganha um sublinhado de
// 24×2 px centrado no texto, colado ao divisor; as demais usam a cor de aba inativa. Só apresenta e emite `clicked`.
AbstractButton {
    id: tab

    property bool active: false

    implicitWidth: label.implicitWidth + 2 * 14
    implicitHeight: 88
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: text

    contentItem: Label {
        id: label
        text: tab.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 14
        font.bold: tab.active
        color: tab.active ? Theme.primary : Theme.tabForeground
        opacity: tab.hovered && !tab.active ? 0.85 : 1
    }

    background: Item {
        Rectangle {
            objectName: "tabUnderline"
            visible: tab.active
            width: 24
            height: 2
            x: (parent.width - width) / 2
            y: parent.height - height
            color: Theme.primary
        }
    }
}
