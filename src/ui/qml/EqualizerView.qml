import QtQuick
import QtQuick.Controls

// Equalizador gráfico na área principal (REFERENCIA-ESTILO-GHUB.md §4.4): 10 trilhos verticais de 2 px com passo de
// 35 px e 232 px de altura (do ganho máximo, em cima, ao mínimo, embaixo), frequências em 12 px acima, a escala
// "+12dB / 0 / −12dB" em 11 px dos dois lados, o valor inteiro de cada banda abaixo e o botão reto "REDEFINIR"
// (200×32) centrado a 17 px da base. Só apresenta e encaminha: mover uma banda emite `bandMoved`, o botão emite
// `resetRequested`; quem usa liga isso ao controlador. Os valores (`gains`) vêm do controlador.
Item {
    id: view

    property var bandFrequencies: []
    property var gains: []
    property int minimumGain: -12
    property int maximumGain: 12
    property bool busy: false

    signal bandMoved(int band, int gain)
    signal resetRequested()

    readonly property int railStep: 35
    readonly property int railHeight: 232
    readonly property int railTop: 213
    // Largura do bloco de bandas: 9 passos mais a largura do último trilho (20 px).
    readonly property real blockWidth: (Math.max(1, bandFrequencies.length) - 1) * railStep + 20
    readonly property real blockLeft: Math.round((width - blockWidth) / 2)

    Item {
        id: bands
        x: view.blockLeft
        y: view.railTop
        width: view.blockWidth
        height: view.railHeight

        Repeater {
            model: view.bandFrequencies

            delegate: Item {
                id: band
                required property int index
                required property string modelData
                readonly property int gain: view.gains[band.index] ?? 0

                x: band.index * view.railStep
                width: 20
                height: view.railHeight

                Label {
                    objectName: "eqBandLabel"
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: -26
                    text: band.modelData
                    font.pixelSize: 12
                    font.capitalization: Font.AllUppercase
                    color: "#aaaaaa"
                }

                GHubSlider {
                    id: slider
                    objectName: "eqBandSlider"
                    width: 20
                    height: view.railHeight
                    orientation: Qt.Vertical
                    from: view.minimumGain
                    to: view.maximumGain
                    stepSize: 1
                    snapMode: Slider.SnapAlways
                    Accessible.name: qsTr("Banda %1 Hz").arg(band.modelData)
                    // O valor segue o controlador; o envio acontece só quando o usuário move.
                    onMoved: view.bandMoved(band.index, Math.round(value))

                    Binding {
                        target: slider
                        property: "value"
                        value: band.gain
                        when: !slider.pressed
                        restoreMode: Binding.RestoreNone
                    }
                }

                Label {
                    objectName: "eqBandValueLabel"
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: view.railHeight + 12
                    text: (band.gain > 0 ? "+" : "") + band.gain
                    font.pixelSize: 12
                    color: Theme.sliderFill
                }
            }
        }

        Label {
            objectName: "eqUnitLabel"
            x: view.blockWidth + 12
            y: -26
            text: qsTr("Hz")
            font.pixelSize: 12
            font.capitalization: Font.AllUppercase
            color: "#aaaaaa"
        }

        // Escala dos dois lados, alinhada ao centro do polegar nos extremos e no meio.
        Repeater {
            model: [{text: qsTr("+12dB"), y: 6}, {text: qsTr("0"), y: view.railHeight / 2},
                    {text: qsTr("−12dB"), y: view.railHeight - 6}]

            delegate: Item {
                id: mark
                required property var modelData

                Label {
                    objectName: "eqScaleLabel"
                    x: -12 - width
                    y: mark.modelData.y - height / 2
                    text: mark.modelData.text
                    font.pixelSize: 11
                    color: Theme.dropdownItem
                }

                Label {
                    objectName: "eqScaleLabel"
                    x: view.blockWidth + 12
                    y: mark.modelData.y - height / 2
                    text: mark.modelData.text
                    font.pixelSize: 11
                    color: Theme.dropdownItem
                }
            }
        }
    }

    GHubButton {
        objectName: "eqFlatButton"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 17
        width: 200
        text: qsTr("Redefinir")
        enabled: !view.busy
        onClicked: view.resetRequested()
    }
}
