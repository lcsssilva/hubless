import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cartão de um dispositivo na tela inicial (INC-014 parte 3, R20): 296×450 (616×450 para o teclado), preenchimento
// de 25 px, título de 24 px, linha de bateria, desenho próprio do dispositivo e a conexão no pé. Só apresenta os
// textos recebidos e emite `clicked`; nenhuma regra de negócio nem acesso a dispositivo.
Rectangle {
    id: card
    objectName: "deviceCard"

    property string deviceName: ""
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

    signal clicked()

    readonly property bool highlighted: pointer.containsMouse || card.activeFocus

    width: wide ? 616 : 296
    height: 450
    radius: Theme.cardRadius
    color: highlighted ? Theme.cardHover : Theme.surface
    border.width: highlighted ? 1 : 0
    border.color: Theme.foreground
    opacity: detected ? 1.0 : 0.5
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: deviceName
    Keys.onReturnPressed: card.clicked()
    Keys.onEnterPressed: card.clicked()
    Keys.onSpacePressed: card.clicked()

    Behavior on color {
        ColorAnimation {
            duration: Theme.shortDuration
            easing.type: Easing.BezierSpline
            easing.bezierCurve: Theme.easingCurve
        }
    }

    Item {
        objectName: "cardContent"
        anchors.fill: parent
        anchors.margins: 25

        Label {
            objectName: "cardTitle"
            color: Theme.foreground
            width: parent.width
            elide: Text.ElideRight
            text: card.deviceName
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
                font.pixelSize: 14
                font.bold: true
                color: Theme.foregroundMedium
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
                anchors.centerIn: parent
                boxWidth: parent.width
                boxHeight: parent.height
            }
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
            color: Theme.foregroundMedium
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
