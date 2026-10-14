import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import GHub.Camera

ApplicationWindow {
    id: window

    // BatteryViewModel (módulo battery), fornecido pelo composition root.
    required property QtObject batteryViewModel

    // DeviceInventoryViewModel (módulo device_inventory), fornecido pelo composition root.
    required property QtObject deviceInventoryViewModel

    // CameraViewModel (módulo camera), fornecido pelo composition root.
    required property QtObject cameraViewModel

    // CameraPreviewViewModel (módulo camera), fornecido pelo composition root.
    required property QtObject cameraPreviewViewModel

    // CameraControlEditor (módulo camera), fornecido pelo composition root: grava um controle só por ação do usuário.
    required property QtObject cameraControlEditor

    // PointerViewModel (módulo pointer), fornecido pelo composition root: a leitura do G703 só começa por clique.
    required property QtObject pointerViewModel

    // KeyboardLightingController (módulo lighting), fornecido pelo composition root: o modo ao vivo do G915
    // só é ligado por clique.
    required property QtObject keyboardLightingController

    // HeadsetSettingsController (módulo headset), fornecido pelo composition root: sidetone e luzes do G733 só
    // são gravados por clique (ADR-0015).
    required property QtObject headsetSettingsController

    // Cor usada pelos efeitos de prévia; estado de UI apenas (atualizada ao escolher uma cor).
    property string lightingEffectColor: "#ffffff"

    width: 960
    height: 600
    visible: true
    title: qsTr("G HUB para Linux")

    // Encerra o stream da câmera ao fechar a janela.
    onClosing: window.cameraPreviewViewModel.stop()

    ScrollView {
        id: scrollView
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: scrollView.availableWidth
            spacing: 12

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("G HUB para Linux")
                font.pixelSize: 28
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: window.width - 64
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Versão em desenvolvimento: detecção de dispositivos (G915, G703, G733, Brio 100), bateria do G915, G703 e G733, leitura da câmera Brio 100 e pré-visualização da imagem. Nenhuma configuração é gravada nos dispositivos.")
            }

            GroupBox {
                Layout.alignment: Qt.AlignHCenter
                title: qsTr("Dispositivos")

                ColumnLayout {
                    Repeater {
                        model: window.deviceInventoryViewModel.devices

                        delegate: Label {
                            required property var modelData
                            objectName: "inventoryDeviceLabel"
                            text: modelData.name + ": " + modelData.summary
                        }
                    }
                }
            }

            GroupBox {
                Layout.alignment: Qt.AlignHCenter
                title: qsTr("Bateria")

                ColumnLayout {
                    Repeater {
                        model: window.batteryViewModel.devices

                        delegate: Label {
                            required property var modelData
                            objectName: "batteryDeviceLabel"
                            text: modelData.name + ": " + modelData.summary
                        }
                    }
                }
            }

            GroupBox {
                Layout.alignment: Qt.AlignHCenter
                title: qsTr("Headset (G733)")

                ColumnLayout {
                    Label {
                        objectName: "headsetNoticeLabel"
                        Layout.maximumWidth: window.width - 96
                        wrapMode: Text.WordWrap
                        text: qsTr("Ajustes enviados pelo HeadsetControl. O headset não informa o valor atual. O sidetone fica gravado no headset e só é ouvido enquanto algum programa usa o microfone; as luzes voltam ao padrão ao desligar o headset. “Ligar luzes” aplica o efeito respiração do HeadsetControl.")
                    }

                    // Grava só pelo botão "Aplicar"; mover o controle não grava nada.
                    RowLayout {
                        Label {
                            text: qsTr("Sidetone: %1").arg(Math.round(headsetSidetoneSlider.value))
                        }

                        Slider {
                            id: headsetSidetoneSlider
                            objectName: "headsetSidetoneSlider"
                            from: 0
                            to: 100
                            stepSize: 1
                        }

                        Button {
                            objectName: "headsetSidetoneApplyButton"
                            text: qsTr("Aplicar")
                            enabled: !window.headsetSettingsController.busy
                            onClicked: window.headsetSettingsController.setSidetone(Math.round(headsetSidetoneSlider.value))
                        }
                    }

                    RowLayout {
                        Button {
                            objectName: "headsetLightsOnButton"
                            text: qsTr("Ligar luzes")
                            enabled: !window.headsetSettingsController.busy
                            onClicked: window.headsetSettingsController.setLights(true)
                        }

                        Button {
                            objectName: "headsetLightsOffButton"
                            text: qsTr("Desligar luzes")
                            enabled: !window.headsetSettingsController.busy
                            onClicked: window.headsetSettingsController.setLights(false)
                        }
                    }

                    Label {
                        objectName: "headsetAppliedLabel"
                        readonly property int sidetone: window.headsetSettingsController.lastSidetone
                        readonly property string lights: window.headsetSettingsController.lightsState
                        text: qsTr("Aplicado nesta sessão: sidetone %1; luzes %2")
                            .arg(sidetone < 0 ? qsTr("nenhum") : sidetone)
                            .arg(lights === "on" ? qsTr("ligadas") : lights === "off" ? qsTr("desligadas") : qsTr("nenhum ajuste"))
                    }

                    Label {
                        objectName: "headsetStatusLabel"
                        text: window.headsetSettingsController.status
                    }
                }
            }

            GroupBox {
                Layout.alignment: Qt.AlignHCenter
                title: qsTr("Câmera")

                ColumnLayout {
                    Label {
                        objectName: "cameraStatusLabel"
                        text: window.cameraViewModel.status
                    }

                    // A pré-visualização só começa por clique (R1); o QML apenas encaminha ao ViewModel.
                    Button {
                        objectName: "cameraPreviewToggle"
                        text: window.cameraPreviewViewModel.active ? qsTr("Parar pré-visualização")
                                                                   : qsTr("Iniciar pré-visualização")
                        onClicked: window.cameraPreviewViewModel.toggle()
                    }

                    Label {
                        objectName: "cameraPreviewStatusLabel"
                        text: qsTr("Pré-visualização: ") + window.cameraPreviewViewModel.status
                    }

                    CameraPreviewItem {
                        id: previewItem
                        objectName: "cameraPreviewItem"
                        visible: window.cameraPreviewViewModel.active
                        // Proporção do modo em uso; 16:9 enquanto não há modo (frameWidth 0).
                        readonly property real aspect: window.cameraPreviewViewModel.frameWidth > 0
                            ? window.cameraPreviewViewModel.frameHeight / window.cameraPreviewViewModel.frameWidth
                            : 9 / 16
                        Layout.preferredWidth: Math.min(480, Math.max(1, scrollView.availableWidth - 64))
                        Layout.preferredHeight: Layout.preferredWidth * aspect

                        Connections {
                            target: window.cameraPreviewViewModel

                            function onFrameReady(image) {
                                previewItem.setFrame(image)
                            }

                            function onPreviewChanged() {
                                if (!window.cameraPreviewViewModel.active)
                                    previewItem.clear()
                            }
                        }
                    }

                    Repeater {
                        model: window.cameraViewModel.modes

                        delegate: Label {
                            required property var modelData
                            objectName: "cameraModeLabel"
                            text: modelData.text
                        }
                    }

                    // Uma linha por controle. O QML só encaminha a ação do usuário ao editor; validação e
                    // gravação são do módulo camera. O modelo é reconstruído a cada releitura da câmera.
                    Repeater {
                        model: window.cameraViewModel.controls

                        delegate: RowLayout {
                            id: controlRow
                            required property var modelData

                            Label {
                                objectName: "cameraControlLabel"
                                text: controlRow.modelData.name + ": " + controlRow.modelData.summary
                            }

                            Loader {
                                sourceComponent: controlRow.modelData.kind === "integer" ? sliderComponent
                                    : controlRow.modelData.kind === "boolean" ? switchComponent
                                    : controlRow.modelData.kind === "menu" ? comboComponent
                                    : null
                            }

                            Button {
                                objectName: "cameraControlDefault"
                                text: qsTr("Padrão")
                                enabled: controlRow.modelData.editable === true
                                onClicked: window.cameraControlEditor.restoreDefault(controlRow.modelData.id)
                            }

                            Component {
                                id: sliderComponent
                                Slider {
                                    objectName: "cameraControlSlider"
                                    from: controlRow.modelData.minimum
                                    to: controlRow.modelData.maximum
                                    stepSize: controlRow.modelData.step
                                    value: controlRow.modelData.value ?? controlRow.modelData.minimum
                                    enabled: controlRow.modelData.editable === true
                                    // Grava só ao soltar (arrastar não grava a cada passo) e em movimento por teclado.
                                    onPressedChanged: if (!pressed)
                                        window.cameraControlEditor.setValue(controlRow.modelData.id, Math.round(value))
                                    onMoved: if (!pressed)
                                        window.cameraControlEditor.setValue(controlRow.modelData.id, Math.round(value))
                                }
                            }

                            Component {
                                id: switchComponent
                                Switch {
                                    objectName: "cameraControlSwitch"
                                    checked: (controlRow.modelData.value ?? 0) !== 0
                                    enabled: controlRow.modelData.editable === true
                                    onToggled: window.cameraControlEditor.setValue(controlRow.modelData.id, checked ? 1 : 0)
                                }
                            }

                            Component {
                                id: comboComponent
                                ComboBox {
                                    objectName: "cameraControlCombo"
                                    textRole: "label"
                                    valueRole: "value"
                                    model: controlRow.modelData.options
                                    // `count` na expressão reavalia o índice quando o modelo é carregado.
                                    currentIndex: count > 0 ? indexOfValue(controlRow.modelData.value) : -1
                                    enabled: controlRow.modelData.editable === true
                                    onActivated: window.cameraControlEditor.setValue(controlRow.modelData.id, currentValue)
                                }
                            }
                        }
                    }

                    Label {
                        objectName: "cameraControlEditorStatus"
                        text: window.cameraControlEditor.status
                    }
                }
            }
            GroupBox {
                Layout.alignment: Qt.AlignHCenter
                title: qsTr("Mouse (G703)")

                ColumnLayout {
                    Label {
                        objectName: "pointerNoticeLabel"
                        Layout.maximumWidth: window.width - 96
                        wrapMode: Text.WordWrap
                        text: qsTr("Ler a configuração ativa o serviço ratbagd (root). Enquanto ele estiver ativo (até ~20 min), a bateria do G733 fica suspensa.")
                    }

                    // A leitura só começa por clique; o QML apenas encaminha ao ViewModel.
                    Button {
                        objectName: "pointerReadButton"
                        text: qsTr("Ler configuração do mouse")
                        enabled: !window.pointerViewModel.reading
                        onClicked: window.pointerViewModel.read()
                    }

                    Label {
                        objectName: "pointerStatusLabel"
                        text: window.pointerViewModel.statusText
                    }

                    Label {
                        objectName: "pointerDeviceLabel"
                        visible: window.pointerViewModel.deviceName !== ""
                        text: window.pointerViewModel.deviceName + " · " + window.pointerViewModel.connectionText
                    }

                    Repeater {
                        model: window.pointerViewModel.warnings

                        delegate: Label {
                            required property var modelData
                            objectName: "pointerWarningLabel"
                            Layout.maximumWidth: window.width - 96
                            wrapMode: Text.WordWrap
                            font.bold: true
                            text: modelData
                        }
                    }

                    Repeater {
                        model: window.pointerViewModel.profiles

                        delegate: ColumnLayout {
                            id: profileColumn
                            required property var modelData

                            Label {
                                objectName: "pointerProfileHeader"
                                font.bold: true
                                text: qsTr("Perfil ") + profileColumn.modelData.index
                                    + (profileColumn.modelData.active ? qsTr(" · ativo segundo o ratbagd") : "")
                                    + (profileColumn.modelData.enabled ? "" : qsTr(" · desabilitado"))
                                    + " · " + profileColumn.modelData.reportRate + " Hz ("
                                    + qsTr("taxas: ") + profileColumn.modelData.supportedReportRates.join(", ") + ")"
                            }

                            Repeater {
                                model: profileColumn.modelData.stages

                                delegate: Label {
                                    required property var modelData
                                    objectName: "pointerStageLabel"
                                    text: qsTr("Estágio ") + modelData.index + ": " + modelData.dpiText
                                        + (modelData.active ? qsTr(" · ativo") : "")
                                        + (modelData.isDefault ? qsTr(" · padrão") : "")
                                        + (modelData.disabled ? qsTr(" · desabilitado") : "")
                                        + (modelData.listed ? "" : qsTr(" · fora da lista do ratbagd"))
                                }
                            }

                            Repeater {
                                model: profileColumn.modelData.buttons

                                delegate: Label {
                                    required property var modelData
                                    objectName: "pointerButtonLabel"
                                    text: qsTr("Botão ") + modelData.index + ": " + modelData.text
                                }
                            }

                            Repeater {
                                model: profileColumn.modelData.leds

                                delegate: Label {
                                    required property var modelData
                                    objectName: "pointerLedLabel"
                                    text: qsTr("LED ") + modelData.index + ": " + modelData.mode + " · "
                                        + modelData.color + " · " + qsTr("brilho ") + modelData.brightness
                                }
                            }
                        }
                    }
                }
            }
            GroupBox {
                Layout.alignment: Qt.AlignHCenter
                title: qsTr("Iluminação")

                ColumnLayout {
                    Label {
                        font.bold: true
                        text: qsTr("Teclado (G915)")
                    }

                    Label {
                        objectName: "lightingNoticeLabel"
                        Layout.maximumWidth: window.width - 96
                        wrapMode: Text.WordWrap
                        text: qsTr("No modo ao vivo o aplicativo controla as cores do G915. Nada é gravado no teclado: ao desligar o modo ou fechar o aplicativo, o efeito salvo volta. Os efeitos são uma prévia: somem ao desligar o modo ao vivo, como observado neste teclado.")
                    }

                    // O modo ao vivo e cada cor só são pedidos por clique; o QML apenas encaminha ao controlador.
                    Button {
                        objectName: "lightingLiveButton"
                        text: window.keyboardLightingController.live ? qsTr("Desligar modo ao vivo")
                                                                     : qsTr("Ligar modo ao vivo")
                        enabled: !window.keyboardLightingController.busy
                        onClicked: window.keyboardLightingController.live
                            ? window.keyboardLightingController.stopLive()
                            : window.keyboardLightingController.startLive()
                    }

                    Flow {
                        Layout.maximumWidth: window.width - 96
                        spacing: 6

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

                            delegate: Button {
                                required property var modelData
                                objectName: "lightingSwatch"
                                text: modelData.name
                                Accessible.name: modelData.name
                                enabled: window.keyboardLightingController.live && !window.keyboardLightingController.busy
                                onClicked: {
                                    window.lightingEffectColor = modelData.value
                                    window.keyboardLightingController.paintAll(modelData.value)
                                }
                            }
                        }

                        Button {
                            objectName: "lightingCustomColorButton"
                            text: qsTr("Outra cor…")
                            enabled: window.keyboardLightingController.live && !window.keyboardLightingController.busy
                            onClicked: lightingColorDialog.open()
                        }
                    }

                    // Prévia de efeito de hardware: só enquanto o modo ao vivo está ligado; some ao desligar.
                    Flow {
                        Layout.maximumWidth: window.width - 96
                        spacing: 6

                        Repeater {
                            model: [
                                {name: qsTr("Onda"), kind: "wave"},
                                {name: qsTr("Ciclo de cores"), kind: "cycle"},
                                {name: qsTr("Respiração"), kind: "breathing"},
                                {name: qsTr("Ondulação"), kind: "ripple"}
                            ]

                            delegate: Button {
                                required property var modelData
                                objectName: "lightingEffectButton"
                                text: modelData.name
                                Accessible.name: modelData.name
                                enabled: window.keyboardLightingController.live && !window.keyboardLightingController.busy
                                onClicked: window.keyboardLightingController.applyEffect(modelData.kind, window.lightingEffectColor)
                            }
                        }
                    }

                    Label {
                        objectName: "lightingStatusLabel"
                        text: window.keyboardLightingController.status
                    }
                }
            }
        }
    }

    ColorDialog {
        id: lightingColorDialog
        title: qsTr("Cor do teclado")
        onAccepted: {
            window.lightingEffectColor = selectedColor
            window.keyboardLightingController.paintAll(selectedColor)
        }
    }
}
