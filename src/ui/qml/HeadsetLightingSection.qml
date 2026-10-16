import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Seção "Cor e efeito" do G733 (INC-018 R1/R4): zona, efeito, cor, período e brilho; só o botão "Aplicar" envia.
// Só apresenta: a validação e os bytes ficam no HeadsetLightingController e no domínio.
ColumnLayout {
    id: section

    // HeadsetLightingController (módulo headset), fornecido por quem usa a seção.
    property QtObject controller: null

    readonly property int effectIndex: effectCombo.currentIndex
    readonly property bool hasColor: effectIndex === 1 || effectIndex === 2
    readonly property bool hasTiming: effectIndex === 2 || effectIndex === 3

    Layout.fillWidth: true
    spacing: 12

    SectionHeader {
        text: qsTr("Cor e efeito")
        help: qsTr("Cada zona do headset recebe o efeito escolhido só quando você toca em “Aplicar”. O efeito é volátil: o headset volta ao padrão ao ser desligado e religado.")
        helpObjectName: "headsetLightingNoticeLabel"
    }

    RowLayout {
        Layout.fillWidth: true
        Label {
            text: qsTr("Zona")
            color: Theme.foreground
            Layout.fillWidth: true
        }
        GHubComboBox {
            id: zoneCombo
            objectName: "headsetLightingZoneCombo"
            model: [qsTr("Inferior"), qsTr("Superior")]
            currentIndex: 0
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Label {
            text: qsTr("Efeito")
            color: Theme.foreground
            Layout.fillWidth: true
        }
        GHubComboBox {
            id: effectCombo
            objectName: "headsetLightingEffectCombo"
            model: [qsTr("Desligado"), qsTr("Fixo"), qsTr("Respiração"), qsTr("Ciclo")]
            currentIndex: 0
        }
    }

    // Cor: três valores 0-255 e uma amostra do que será enviado.
    ColumnLayout {
        visible: section.hasColor
        Layout.fillWidth: true
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Cor")
                color: Theme.foreground
                Layout.fillWidth: true
            }
            Rectangle {
                objectName: "headsetLightingColorPreview"
                implicitWidth: 20
                implicitHeight: 20
                radius: 10
                color: Qt.rgba(redField.value / 255, greenField.value / 255, blueField.value / 255, 1)
                border.width: 1
                border.color: Theme.sliderFill
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("R"); color: Theme.foreground }
            GHubSpinBox {
                id: redField
                objectName: "headsetLightingRedField"
                Layout.fillWidth: true
                from: 0
                to: 255
                value: 0
                editable: true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("G"); color: Theme.foreground }
            GHubSpinBox {
                id: greenField
                objectName: "headsetLightingGreenField"
                Layout.fillWidth: true
                from: 0
                to: 255
                value: 255
                editable: true
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("B"); color: Theme.foreground }
            GHubSpinBox {
                id: blueField
                objectName: "headsetLightingBlueField"
                Layout.fillWidth: true
                from: 0
                to: 255
                value: 255
                editable: true
            }
        }
    }

    ColumnLayout {
        visible: section.hasTiming
        Layout.fillWidth: true
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Período (ms)")
                color: Theme.foreground
                Layout.fillWidth: true
            }
            GHubSpinBox {
                id: periodField
                objectName: "headsetLightingPeriodField"
                from: 1000
                to: 20000
                stepSize: 100
                value: 5000
                editable: true
            }
        }

        Label {
            text: qsTr("Brilho")
            color: Theme.foreground
        }
        GHubSlider {
            id: brightnessSlider
            objectName: "headsetLightingBrightnessSlider"
            Layout.fillWidth: true
            from: 1
            to: 100
            stepSize: 1
            value: 100
            showValue: true
            showExtremes: true
            Accessible.name: qsTr("Brilho")
        }
    }

    // Exceção de segurança: avisos sempre visíveis, pois o botão escreve no headset.
    Label {
        objectName: "headsetLightingVolatileLabel"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.bold: true
        color: Theme.foreground
        text: qsTr("Efeito volátil: o headset volta ao padrão ao ser religado.")
    }

    Label {
        objectName: "headsetLightingColorNoteLabel"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.foreground
        text: qsTr("Cor enviada sem a correção de cor do G HUB.")
    }

    GHubButton {
        objectName: "headsetLightingApplyButton"
        text: qsTr("Aplicar")
        enabled: section.controller !== null && !section.controller.busy
        onClicked: section.controller.apply(zoneCombo.currentIndex, effectCombo.currentIndex,
                                            redField.value, greenField.value, blueField.value,
                                            periodField.value, Math.round(brightnessSlider.value))
    }

    Label {
        objectName: "headsetLightingStatusLabel"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.foreground
        text: section.controller !== null ? section.controller.status : ""
    }
}
