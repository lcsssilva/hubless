import QtQuick

// Desenho próprio e simplificado de um dispositivo (INC-014 parte 3): formas QML, sem imagens nem logotipos de
// terceiros. A arte é feita em coordenadas fixas (artWidth × artHeight) e escalada para caber na caixa
// boxWidth × boxHeight, mantendo a proporção.
Item {
    id: drawing
    objectName: "deviceDrawing"

    // "keyboard", "mouse", "headset" ou "camera".
    property string kind: "keyboard"
    property real boxWidth: 246
    property real boxHeight: 200

    readonly property var artSizes: ({
        keyboard: Qt.size(450, 141),
        mouse: Qt.size(112, 207),
        headset: Qt.size(165, 196),
        camera: Qt.size(140, 160)
    })
    readonly property size artSize: artSizes[kind] ?? Qt.size(100, 100)
    readonly property real fit: Math.min(boxWidth / artSize.width, boxHeight / artSize.height)

    width: artSize.width * fit
    height: artSize.height * fit

    // Forma básica da arte: retângulo suavizado (a arte é escalada, então o contorno precisa de antialiasing).
    component Part: Rectangle {
        antialiasing: true
    }

    Item {
        id: art
        width: drawing.artSize.width
        height: drawing.artSize.height
        scale: drawing.fit
        transformOrigin: Item.TopLeft

        // Teclado: corpo e 5 fileiras de teclas; a última tem a barra de espaço.
        Item {
            anchors.fill: parent
            visible: drawing.kind === "keyboard"

            Part {
                anchors.fill: parent
                radius: 12
                color: Theme.deviceBody
                border.width: 2
                border.color: Theme.deviceEdge
            }

            Repeater {
                model: 75

                delegate: Rectangle {
                    required property int index
                    readonly property int row: Math.floor(index / 15)
                    readonly property int column: index % 15
                    readonly property bool spaceBar: row === 4 && column === 3
                    visible: row < 4 || column < 3 || column > 11 || spaceBar
                    x: 10 + column * 29
                    y: 10 + row * 25
                    width: spaceBar ? 9 * 24 + 8 * 5 : 24
                    height: 20
                    radius: 4
                    color: Theme.deviceDetail
                }
            }
        }

        // Mouse: corpo arredondado, divisão dos botões e roda.
        Item {
            anchors.fill: parent
            visible: drawing.kind === "mouse"

            Part {
                anchors.fill: parent
                radius: 56
                color: Theme.deviceBody
                border.width: 2
                border.color: Theme.deviceEdge
            }

            Part {
                x: 55
                y: 2
                width: 2
                height: 72
                color: Theme.deviceEdge
            }

            Part {
                x: 2
                y: 74
                width: 108
                height: 2
                color: Theme.deviceEdge
            }

            Part {
                x: 50
                y: 26
                width: 12
                height: 32
                radius: 6
                color: Theme.deviceDetail
            }
        }

        // Headset: arco da cabeça, duas conchas e a haste do microfone.
        Item {
            anchors.fill: parent
            visible: drawing.kind === "headset"

            Item {
                x: 0
                y: 0
                width: 165
                height: 96
                clip: true

                Part {
                    x: 12
                    y: 0
                    width: 141
                    height: 150
                    radius: 70
                    color: "transparent"
                    border.width: 12
                    border.color: Theme.deviceBody
                }
            }

            Part {
                x: 0
                y: 66
                width: 50
                height: 90
                radius: 24
                color: Theme.deviceBody
                border.width: 2
                border.color: Theme.deviceEdge
            }

            Part {
                x: 115
                y: 66
                width: 50
                height: 90
                radius: 24
                color: Theme.deviceBody
                border.width: 2
                border.color: Theme.deviceEdge
            }

            Part {
                x: 10
                y: 84
                width: 22
                height: 54
                radius: 11
                color: Theme.deviceDetail
            }

            Part {
                x: 133
                y: 84
                width: 22
                height: 54
                radius: 11
                color: Theme.deviceDetail
            }

            Part {
                x: 18
                y: 150
                width: 7
                height: 40
                radius: 3.5
                color: Theme.deviceBody
                border.width: 1
                border.color: Theme.deviceEdge
            }

            Part {
                x: 12
                y: 184
                width: 19
                height: 12
                radius: 6
                color: Theme.deviceDetail
            }
        }

        // Webcam: corpo, lente concêntrica, pescoço e base.
        Item {
            anchors.fill: parent
            visible: drawing.kind === "camera"

            Part {
                x: 10
                y: 4
                width: 120
                height: 86
                radius: 24
                color: Theme.deviceBody
                border.width: 2
                border.color: Theme.deviceEdge
            }

            Part {
                x: 40
                y: 14
                width: 60
                height: 60
                radius: 30
                color: Theme.deviceDetail
                border.width: 2
                border.color: Theme.deviceEdge
            }

            Part {
                x: 54
                y: 28
                width: 32
                height: 32
                radius: 16
                color: Theme.background
                border.width: 2
                border.color: Theme.deviceEdge
            }

            Part {
                x: 114
                y: 14
                width: 6
                height: 6
                radius: 3
                color: Theme.primary
            }

            Part {
                x: 58
                y: 90
                width: 24
                height: 30
                color: Theme.deviceDetail
            }

            Part {
                x: 30
                y: 120
                width: 80
                height: 28
                radius: 8
                color: Theme.deviceBody
                border.width: 2
                border.color: Theme.deviceEdge
            }
        }
    }
}
