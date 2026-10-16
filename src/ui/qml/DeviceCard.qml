import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cartão de um dispositivo na tela inicial (INC-014 parte 3, R20): 296×450 (616×450 para o teclado), preenchimento
// de 24 px e borda de 1 px (CSS computado §4), título de 24 px, linha de bateria, desenho próprio do dispositivo e a conexão no pé. Só apresenta os
// textos recebidos e emite `clicked`; nenhuma regra de negócio nem acesso a dispositivo.
Rectangle {
    id: card
    objectName: "deviceCard"

    property string deviceName: ""
    // Apelido do dispositivo (INC-017 R9), só no Hubless; vazio = mostra o nome do catálogo.
    property string displayName: ""
    // "keyboard", "mouse", "headset" ou "camera": escolhe o desenho.
    property string deviceKind: "keyboard"
    property string connectionText: ""
    property string batteryText: ""
    // Nível atual (0–100); -1 esconde o ícone de bateria.
    property int batteryPercent: -1
    // Cartão largo (teclado): ocupa a largura de dois cartões comuns e o intervalo entre eles.
    property bool wide: false
    // Dispositivo não detectado fica esmaecido, mas continua clicável.
    property bool detected: true
    // Carregando (como "Baixando recursos…" no G HUB): fundo mais claro, textos esmaecidos e anel de progresso.
    property bool loading: false

    signal clicked()

    // Desabilitado (não detectado ou baixando recursos): camada de 10 %, texto #6a6a6a, imagem a 0,6, sem hover.
    readonly property bool dimmed: loading || !detected
    readonly property bool highlighted: !dimmed && (pointer.containsMouse || card.activeFocus)

    width: wide ? 616 : 296
    height: 450
    radius: Theme.cardRadius
    color: dimmed ? Theme.cardLoading : (highlighted ? Theme.cardHover : Theme.surface)
    border.width: 1
    border.color: highlighted ? Theme.foreground : "transparent"
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: displayName !== "" ? displayName : deviceName
    Keys.onReturnPressed: card.clicked()
    Keys.onEnterPressed: card.clicked()
    Keys.onSpacePressed: card.clicked()

    Item {
        objectName: "cardContent"
        anchors.fill: parent
        anchors.margins: 24

        Label {
            objectName: "cardTitle"
            color: card.dimmed ? Theme.foregroundDisabled : Theme.foreground
            width: parent.width
            elide: Text.ElideRight
            text: card.displayName !== "" ? card.displayName : card.deviceName
            font.pixelSize: 24
            font.bold: true
        }

        // Porcentagem e ícone; o dispositivo sem bateria (Brio 100) não tem a linha.
        Row {
            objectName: "cardBatteryRow"
            y: 60
            spacing: 8
            visible: card.batteryText !== ""

            Label {
                objectName: "cardBatteryText"
                anchors.verticalCenter: parent.verticalCenter
                text: card.batteryText
                font.pixelSize: 16
                lineHeight: 24
                lineHeightMode: Text.FixedHeight
                color: card.dimmed ? Theme.foregroundDisabled : Theme.foregroundMedium
            }

            BatteryIcon {
                objectName: "deviceCardBattery"
                anchors.verticalCenter: parent.verticalCenter
                value: card.batteryPercent
            }
        }

        // Área da imagem: 246×200 (566×177 no cartão largo), desenho centralizado.
        Item {
            objectName: "cardImageArea"
            y: 112 + (card.wide ? 11 : 0)
            width: parent.width
            height: card.wide ? 177 : 200

            DeviceDrawing {
                kind: card.deviceKind
                opacity: card.dimmed ? 0.6 : 1
                anchors.centerIn: parent
                boxWidth: parent.width
                boxHeight: parent.height
            }
        }

        GHubBusyRing {
            objectName: "cardBusyRing"
            anchors.centerIn: parent
            running: card.loading
        }

        Label {
            objectName: "cardConnection"
            anchors.bottom: parent.bottom
            width: parent.width
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
            text: card.connectionText
            font.pixelSize: 13
            color: card.dimmed ? Theme.foregroundDisabled : Theme.foregroundMedium
        }
    }

    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: card.clicked()
    }
}
