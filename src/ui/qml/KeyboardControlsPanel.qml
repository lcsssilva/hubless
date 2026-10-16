import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Abas "Modo de jogo" e "Perfis M1–M3" do painel de iluminação do G915 (INC-019, ADR-0029). Só apresentação: escolhe
// as teclas e encaminha cada clique ao controlador (KeyboardControlsController); nenhuma regra de protocolo, limite de
// teclas ou validação de slot mora aqui. Nada é pedido ao carregar: só os botões falam com o controlador.
ColumnLayout {
    id: panel

    // KeyboardControlsController, fornecido pelo composition root.
    required property QtObject controller
    // O modo ao vivo e as animações usam o mesmo acesso ao teclado: com um deles ligado, os controles ficam bloqueados.
    property bool liveModeOn: false
    property bool animationRunning: false

    readonly property bool blocked: liveModeOn || animationRunning
    readonly property bool idle: !controller.busy && !blocked
    readonly property var slotNumbers: [1, 2, 3]

    Layout.fillWidth: true
    spacing: 12

    SectionHeader {
        text: qsTr("Modo de jogo")
        help: qsTr("Desativa as teclas escolhidas (por padrão, as teclas Windows e Menu) para evitar toques acidentais durante o jogo. O comando é enviado ao G915 por HID++; ainda é volátil até o teste físico mostrar o contrário: pode não persistir no teclado.")
    }

    Label {
        objectName: "gameModeNotice"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.foregroundMedium
        text: qsTr("O modo de jogo é volátil até o teste físico: o efeito pode não sobreviver ao desligar o teclado. Nada é enviado ao abrir o aplicativo; só os botões abaixo falam com o teclado.")
    }

    Flow {
        Layout.fillWidth: true
        spacing: 8

        Repeater {
            model: panel.controller.gameModeKeys

            delegate: GHubButton {
                required property var modelData
                objectName: "gameModeKeyChip"
                text: modelData + "  ✕"
                enabled: panel.idle
                Accessible.name: qsTr("Remover a tecla %1").arg(modelData)
                onClicked: panel.controller.removeGameKey(modelData)
            }
        }
    }

    GHubComboBox {
        id: addCombo
        objectName: "gameModeAddCombo"
        Layout.fillWidth: true
        model: panel.controller.gameModeChoices
        currentIndex: -1
        placeholderText: qsTr("Adicionar tecla")
        enabled: panel.idle
        onActivated: index => {
            panel.controller.addGameKey(panel.controller.gameModeChoices[index])
            addCombo.currentIndex = -1
        }
    }

    RowLayout {
        spacing: 8

        GHubButton {
            objectName: "gameModeActivateButton"
            text: qsTr("Ativar")
            enabled: panel.idle
            onClicked: panel.controller.activateGameMode()
        }

        GHubButton {
            objectName: "gameModeDeactivateButton"
            text: qsTr("Desativar")
            enabled: panel.idle
            onClicked: panel.controller.deactivateGameMode()
        }

        GHubButton {
            objectName: "gameModeResetButton"
            text: qsTr("Padrão")
            enabled: !panel.controller.busy
            onClicked: panel.controller.resetGameKeys()
        }
    }

    SectionHeader {
        text: qsTr("Perfis do teclado")
        help: qsTr("Troca o perfil onboard ativo do G915 (M1, M2 ou M3). Só os perfis habilitados na memória do teclado podem ser escolhidos; leia os perfis primeiro. Nada é gravado: apenas o perfil ativo muda.")
    }

    RowLayout {
        spacing: 8

        Repeater {
            model: panel.slotNumbers

            delegate: GHubButton {
                required property var modelData
                objectName: "slotButton"
                text: "M" + modelData
                enabled: panel.idle && panel.controller.enabledSlots.indexOf(modelData) >= 0
                onClicked: panel.controller.selectSlot(modelData)
            }
        }

        GHubButton {
            objectName: "slotRefreshButton"
            text: qsTr("Ler perfis")
            enabled: panel.idle
            onClicked: panel.controller.refreshSlots()
        }
    }

    Label {
        objectName: "activeSlotLabel"
        Layout.fillWidth: true
        visible: panel.controller.activeSlot > 0
        font.pixelSize: 12
        color: Theme.foregroundMedium
        text: qsTr("Perfil ativo: M%1").arg(panel.controller.activeSlot)
    }

    Label {
        objectName: "controlsLiveNotice"
        Layout.fillWidth: true
        visible: panel.blocked
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.foregroundMedium
        text: qsTr("Desligue o modo ao vivo e as animações para usar estes controles: todos usam o mesmo acesso ao teclado.")
    }

    Label {
        objectName: "controlsStatusLabel"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.foreground
        text: panel.controller.status
    }
}
