import QtQuick
import QtQuick.Controls

// Bloco de bateria no estilo do G HUB (REFERENCIA-ESTILO-GHUB.md §4.4 e §5): três linhas alinhadas à esquerda —
// "NÍVEL DE BATERIA" (13 px, secundário), o percentual em negrito de 18 px com o ícone de bateria à direita do
// número, e o estado ("DESCARREGANDO"/"CARREGANDO") em 12 px negrito, secundário e MAIÚSCULO. Só apresenta os textos
// do ViewModel: `summary` tem o formato "63% · descarregando" e `percent` o nível (-1 = desconhecido).
Column {
    id: block

    property string summary: ""
    property int percent: -1

    // Estado = o resumo sem o "NN% · " inicial; sem percentual, é o resumo inteiro.
    readonly property string stateText: percent >= 0
        ? summary.split(" · ").slice(1).join(" · ")
        : summary

    visible: summary !== ""
    spacing: 6

    Label {
        objectName: "batteryHeader"
        text: qsTr("Nível de bateria")
        font.pixelSize: 13
        font.letterSpacing: 0.5
        font.capitalization: Font.AllUppercase
        color: Theme.foregroundMedium
    }

    Row {
        spacing: 8
        visible: block.percent >= 0

        Label {
            objectName: "batteryPercentText"
            anchors.verticalCenter: parent.verticalCenter
            visible: block.percent >= 0
            text: block.percent + "%"
            font.pixelSize: 18
            font.bold: true
            color: Theme.foreground
        }

        BatteryIcon {
            objectName: "batteryIcon"
            anchors.verticalCenter: parent.verticalCenter
            value: block.percent
        }
    }

    Label {
        objectName: "batteryStateText"
        visible: text !== ""
        text: block.stateText
        font.pixelSize: 12
        font.bold: true
        font.capitalization: Font.AllUppercase
        color: Theme.foregroundMedium
    }
}
