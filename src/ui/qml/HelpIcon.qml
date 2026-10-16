import QtQuick
import QtQuick.Controls

// Ícone "?" de 20 px (REFERENCIA-ESTILO-GHUB.md §4.3): leva ao lado do cabeçalho de uma seção o texto explicativo
// longo, que aparece num balão branco (texto preto, 12 px, raio 5) ao passar o mouse ou clicar. O texto completo
// continua disponível em `text` (e como descrição acessível): nada é apagado, só movido para o balão.
AbstractButton {
    id: icon
    objectName: "helpIcon"

    // O texto explicativo do balão é o `text` do botão (não aparece no ícone, que mostra só "?").
    readonly property alias tip: balloon
    // Estado de UI: o clique abre o balão até o próximo clique ou até o mouse sair.
    property bool pinned: false

    implicitWidth: 20
    implicitHeight: 20
    padding: 0
    hoverEnabled: true
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Ajuda")
    Accessible.description: icon.text

    onClicked: icon.pinned = !icon.pinned
    onHoveredChanged: if (!hovered) icon.pinned = false

    contentItem: Label {
        objectName: "helpMark"
        text: "?"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 13
        font.bold: true
        color: Theme.helpIcon
    }

    background: Rectangle {
        objectName: "helpDisc"
        radius: width / 2
        antialiasing: true
        color: "transparent"
        border.width: 1
        border.color: icon.hovered ? Theme.foreground : Theme.helpIcon
    }

    ToolTip {
        id: balloon
        visible: (icon.hovered || icon.pinned) && icon.text !== ""
        text: icon.text
        delay: 0
        timeout: -1
        margins: 8
        x: 0
        y: icon.height + 6
        width: Math.min(300, metrics.width) + leftPadding + rightPadding
        leftPadding: 10
        rightPadding: 10
        topPadding: 8
        bottomPadding: 8

        contentItem: Text {
            width: Math.min(300, metrics.width)
            text: balloon.text
            wrapMode: Text.Wrap
            font.pixelSize: 12
            color: "#000000"
        }

        background: Rectangle {
            radius: 5
            color: "#ffffff"
        }
    }

    TextMetrics {
        id: metrics
        font.pixelSize: 12
        text: icon.text
    }
}
