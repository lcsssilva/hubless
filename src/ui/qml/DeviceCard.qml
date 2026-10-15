import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Cartão de um dispositivo na tela inicial (INC-014 R3). Só apresenta os textos recebidos e emite `clicked`;
// nenhuma regra de negócio nem acesso a dispositivo.
Rectangle {
    id: card
    objectName: "deviceCard"

    property string deviceName: ""
    property string iconName: ""
    property string connectionText: ""
    property string batteryText: ""
    // Nível atual (0–100); -1 esconde a barra de bateria.
    property int batteryPercent: -1
    // Dispositivo não detectado fica esmaecido, mas continua clicável.
    property bool detected: true

    signal clicked()

    width: 210
    height: 250
    radius: Theme.radius
    color: Theme.surface
    border.width: 2
    border.color: pointer.containsMouse || card.activeFocus ? Theme.primary : Theme.outline
    opacity: detected ? 1.0 : 0.5
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: deviceName
    Keys.onReturnPressed: card.clicked()
    Keys.onEnterPressed: card.clicked()
    Keys.onSpacePressed: card.clicked()

    // Camada de estado: realce de hover e de clique, sobre o fundo e sob o conteúdo.
    Rectangle {
        objectName: "stateOverlay"
        anchors.fill: parent
        radius: card.radius
        color: Theme.foreground
        opacity: pointer.pressed ? Theme.pressOpacity : (pointer.containsMouse ? Theme.hoverOpacity : 0)

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.shortDuration
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easingCurve
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        // O ícone vem do tema do sistema; o clique é tratado pela área de mouse do cartão.
        ToolButton {
            Layout.alignment: Qt.AlignHCenter
            focusPolicy: Qt.NoFocus
            hoverEnabled: false
            icon.name: card.iconName
            icon.width: 64
            icon.height: 64
            display: AbstractButton.IconOnly
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: card.deviceName
            font.pixelSize: 20
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: card.connectionText
            color: Theme.foregroundMedium
        }

        Item { Layout.fillHeight: true }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            visible: card.batteryText !== ""
            text: card.batteryText
            font.pixelSize: 13
        }

        ProgressBar {
            objectName: "deviceCardBattery"
            Layout.fillWidth: true
            visible: card.batteryPercent >= 0
            from: 0
            to: 100
            value: Math.max(0, card.batteryPercent)
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
