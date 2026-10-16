import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Seção "Sincronizar cor" (INC-021 R5, ADR-0030). Só apresentação: escolhe a cor e os dispositivos e encaminha ao
// controlador (SyncColorController); nenhuma regra de sincronização mora aqui. Nada é pedido ao carregar: só
// "Aplicar" fala com o controlador. O G703 grava no mouse (persistente) e por isso só entra com a confirmação
// explícita marcada.
ColumnLayout {
    id: panel

    // SyncColorController, fornecido pelo composition root.
    required property QtObject controller
    property color syncColor: "#ffffff"

    Layout.fillWidth: true
    spacing: 12

    SectionHeader {
        text: qsTr("Sincronizar cor")
        help: qsTr("Aplica a mesma cor fixa aos dispositivos marcados. G915 (exige o modo ao vivo) e G733 são voláteis. O G703 grava o LED e o logotipo no perfil do mouse (persistente, com cópia de segurança).")
    }

    Flow {
        Layout.fillWidth: true
        spacing: 18

        Repeater {
            model: [
                {name: qsTr("Vermelho"), value: "#ff0000"},
                {name: qsTr("Verde"), value: "#00ff00"},
                {name: qsTr("Azul"), value: "#0000ff"},
                {name: qsTr("Branco"), value: "#ffffff"},
                {name: qsTr("Ciano"), value: "#00ffff"},
                {name: qsTr("Magenta"), value: "#ff00ff"},
                {name: qsTr("Amarelo"), value: "#ffff00"},
                {name: qsTr("Laranja"), value: "#ff8000"}
            ]

            delegate: ColorSwatch {
                required property var modelData
                objectName: "syncSwatch"
                swatchColor: modelData.value
                colorName: modelData.name
                selected: Qt.colorEqual(panel.syncColor, modelData.value)
                enabled: !panel.controller.busy
                onClicked: panel.syncColor = modelData.value
            }
        }
    }

    GHubCheckBox {
        id: keyboardCheck
        objectName: "syncKeyboardCheck"
        text: qsTr("G915 (teclado, modo ao vivo)")
        checked: true
        enabled: !panel.controller.busy
    }

    GHubCheckBox {
        id: headsetCheck
        objectName: "syncHeadsetCheck"
        text: qsTr("G733 (headset, as duas zonas)")
        checked: true
        enabled: !panel.controller.busy
    }

    GHubCheckBox {
        id: mouseCheck
        objectName: "syncMouseCheck"
        text: qsTr("G703 (mouse, LED e logotipo)")
        checked: false
        enabled: !panel.controller.busy
    }

    GHubCheckBox {
        id: mouseConfirmCheck
        objectName: "syncMouseConfirmCheck"
        visible: mouseCheck.checked
        text: qsTr("Confirmo gravar a cor no perfil do G703 (persistente)")
        checked: false
        enabled: !panel.controller.busy
    }

    GHubButton {
        objectName: "syncApplyButton"
        text: qsTr("Aplicar")
        enabled: !panel.controller.busy && (keyboardCheck.checked || headsetCheck.checked || mouseCheck.checked)
        onClicked: panel.controller.apply(panel.syncColor, keyboardCheck.checked, headsetCheck.checked,
                                          mouseCheck.checked, mouseConfirmCheck.checked)
    }

    Label {
        objectName: "syncStatusLabel"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.foreground
        text: panel.controller.status
    }

    Repeater {
        model: panel.controller.results

        delegate: Label {
            required property string modelData
            objectName: "syncResultLabel"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: Theme.foregroundMedium
            text: modelData
        }
    }
}
