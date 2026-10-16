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
        keyboard: Qt.size(564, 177),
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

        // Teclado low-profile de tamanho completo (G915): corpo, fileira de funções, bloco principal, navegação e
        // bloco numérico, calculados em unidades de tecla (u = 23 px). Só formas; nenhum símbolo de marca.
        Item {
            anchors.fill: parent
            visible: drawing.kind === "keyboard"

            readonly property real unit: 23
            // Cada tecla: [x, y, largura, altura] em unidades, a partir do canto do conjunto de teclas.
            readonly property var keys: {
                const list = []
                const add = (x, y, w, h) => list.push([x, y, w, h ?? 1])
                // Fileira de funções (altura 0,75): Esc, F1-F4, F5-F8, F9-F12 e três teclas de sistema.
                add(0, 0, 1, 0.75)
                for (let i = 0; i < 12; ++i)
                    add(2 + i + Math.floor(i / 4) * 0.5, 0, 1, 0.75)
                for (let i = 0; i < 3; ++i)
                    add(15.25 + i, 0, 1, 0.75)
                // Fileiras do bloco principal (y = 1,35 + fileira).
                const rows = [[1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2],
                              [1.5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5],
                              [1.75, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2.25],
                              [2.25, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2.75],
                              [1.25, 1.25, 1.25, 6.25, 1.25, 1.25, 1.25, 1.25]]
                rows.forEach((row, r) => {
                    let x = 0
                    row.forEach(w => { add(x, 1.35 + r, w); x += w })
                })
                // Navegação: seis teclas e as setas.
                for (let i = 0; i < 3; ++i) {
                    add(15.25 + i, 1.35, 1)
                    add(15.25 + i, 2.35, 1)
                    add(15.25 + i, 5.35, 1)
                }
                add(16.25, 4.35, 1)
                // Bloco numérico: três colunas, "+" e Enter altos, zero largo.
                for (let r = 0; r < 4; ++r)
                    for (let c = 0; c < 3; ++c)
                        add(19.5 + c, 1.35 + r, 1)
                add(22.5, 2.35, 1, 2)
                add(22.5, 4.35, 1, 2)
                add(19.5, 5.35, 2)
                add(21.5, 5.35, 1)
                return list
            }

            Part {
                anchors.fill: parent
                radius: 10
                color: Theme.deviceBody
                border.width: 2
                border.color: Theme.deviceEdge
            }

            Repeater {
                model: parent.keys

                delegate: Rectangle {
                    required property var modelData
                    x: 15 + modelData[0] * 23
                    y: 18 + modelData[1] * 23
                    width: modelData[2] * 23 - 3
                    height: modelData[3] * 23 - 3
                    radius: 3
                    antialiasing: true
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
                border.width: 1
                border.color: Theme.deviceEdge
            }

            // Botão de DPI sob a roda e os dois botões laterais do polegar.
            Part {
                x: 51
                y: 84
                width: 10
                height: 6
                radius: 3
                color: Theme.deviceDetail
            }

            Part {
                x: 0
                y: 84
                width: 6
                height: 24
                radius: 3
                color: Theme.deviceDetail
            }

            Part {
                x: 0
                y: 114
                width: 6
                height: 24
                radius: 3
                color: Theme.deviceDetail
            }

            // Faixa de luz na base.
            Part {
                x: 36
                y: 176
                width: 40
                height: 4
                radius: 2
                color: Theme.primary
                opacity: 0.8
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
                border.width: 1
                border.color: Theme.deviceEdge
            }

            // Almofada da haste, no topo do arco.
            Part {
                x: 62
                y: 7
                width: 41
                height: 8
                radius: 4
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
                x: 62
                y: 34
                width: 8
                height: 8
                radius: 4
                color: Theme.deviceEdge
                opacity: 0.7
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
