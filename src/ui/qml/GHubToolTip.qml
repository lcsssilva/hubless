import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

// Dica no estilo do G HUB (CSS computado §5.2): fundo branco, raio 6, texto #222425 de 12 px com largura máxima de
// 250 px, preenchimento 8/20 e seta de 8 px voltada ao item. `placement`: "right" (à direita do item pai, centrada
// na vertical) ou "bottom" (abaixo do item pai, alinhada à esquerda), sempre afastada 8 px. Só apresentação.
ToolTip {
    id: tip

    property string placement: "bottom"

    readonly property bool isRight: placement === "right"

    margins: 0
    topPadding: 8
    bottomPadding: 8
    leftPadding: 20
    rightPadding: 20
    x: isRight ? (parent ? parent.width + 8 : 0) : 0
    y: isRight ? (parent ? (parent.height - height) / 2 : 0) : (parent ? parent.height + 8 : 0)
    width: Math.min(250, metrics.width) + leftPadding + rightPadding

    contentItem: Text {
        width: Math.min(250, metrics.width)
        text: tip.text
        wrapMode: Text.Wrap
        font.pixelSize: 12
        color: Theme.tooltipText
    }

    background: Item {
        property alias radius: fill.radius
        property alias color: fill.color

        Rectangle {
            id: fill
            anchors.fill: parent
            radius: Theme.tooltipRadius
            color: "#ffffff"
        }

        // Seta de 8 px no lado voltado ao item.
        Shape {
            objectName: "tipArrow"
            x: tip.isRight ? -8 : 12
            y: tip.isRight ? (parent.height - 16) / 2 : -8
            width: tip.isRight ? 8 : 16
            height: tip.isRight ? 16 : 8
            ShapePath {
                fillColor: fill.color
                strokeColor: "transparent"
                startX: tip.isRight ? 8 : 0
                startY: tip.isRight ? 0 : 8
                PathLine { x: tip.isRight ? 8 : 16; y: tip.isRight ? 16 : 8 }
                PathLine { x: tip.isRight ? 0 : 8; y: tip.isRight ? 8 : 0 }
            }
        }
    }

    TextMetrics {
        id: metrics
        font.pixelSize: 12
        text: tip.text
    }
}
