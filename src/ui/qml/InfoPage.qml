import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Página Informações de um dispositivo (REFERENCIA-ESTILO-GHUB.md §5): sem painel lateral, três colunas de 250 px
// em x = 67/387/705 (relativos ao bloco de 1022 px, centrado na página). Esquerda: bloco de bateria e conexão;
// centro: o desenho do dispositivo; direita: a identidade HID++ (só leitura, só por clique). Só apresenta os textos
// dos ViewModels e encaminha o clique de "Ler identidade" a `identityViewModel.read(identityKey)`.
ScrollView {
    id: page
    objectName: "infoPage"

    property string deviceName: ""
    property string deviceKind: "keyboard"
    property var inventoryDevices: []
    property var batteryDevices: []
    // Resumo e nível da bateria do dispositivo (vazio = sem bateria).
    property string batterySummary: ""
    property int batteryPercent: -1
    // DeviceIdentityViewModel e a chave do dispositivo ("g915", "g703", "g733"); chave vazia = sem leitura HID++.
    property QtObject identityViewModel: null
    property string identityKey: ""
    // ViewModel de estado HID++ (KeyboardStatus/PointerSensorStatus/HeadsetStatus) e o tipo: "keyboard", "pointer" ou
    // "headset"; vazio (Brio 100) = sem a seção. Só lê por clique.
    property QtObject statusViewModel: null
    property string statusKind: ""

    readonly property var identity: identityKey !== "" && identityViewModel && identityViewModel.identities[identityKey]
                                    ? identityViewModel.identities[identityKey]
                                    : ({state: "idle"})
    readonly property bool identityOk: identity.state === "ok"
    // Estado de UI apenas: lista de IDs das features recolhida por padrão.
    property bool featureIdsExpanded: false

    readonly property var statusMap: statusViewModel && statusViewModel.status ? statusViewModel.status : ({})
    // Linhas da tabela: [rótulo, valor], só com o que o ViewModel preencheu.
    readonly property var statusRows: {
        const m = page.statusMap
        const rows = []
        const add = (label, key) => { if (m[key] !== undefined && m[key] !== "") rows.push([label, m[key]]) }
        if (page.statusKind === "keyboard") {
            add(qsTr("Brilho"), "brightness")
            add(qsTr("Report rate"), "reportRate")
            add(qsTr("Taxas suportadas"), "supportedReportRates")
            add(qsTr("Não interpretado"), "brightnessUnexplained")
        } else if (page.statusKind === "pointer") {
            add(qsTr("DPI"), "dpi")
            add(qsTr("Faixa de DPI"), "dpiRange")
            add(qsTr("Report rate"), "reportRate")
            add(qsTr("Taxas suportadas"), "supportedReportRates")
        } else if (page.statusKind === "headset") {
            add(qsTr("Equalizador"), "equalizer")
            add(qsTr("Não interpretado"), "equalizerUnexplained")
            add(qsTr("Sidetone (0x8300)"), "uninterpreted")
        }
        return rows
    }

    readonly property real blockLeft: Math.max(0, Math.round((availableWidth - 1022) / 2))

    contentWidth: availableWidth
    contentHeight: Math.max(left.implicitHeight, right.implicitHeight) + 140

    // Texto de valor (branco, negrito) e de rótulo (cinza) da tabela de duas colunas.
    component ValueLabel: Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: 13
        font.bold: true
        color: Theme.foreground
    }

    component InfoHeader: SectionTitle {
        color: Theme.sliderFill
    }

    ColumnLayout {
        id: left
        objectName: "infoLeftColumn"
        x: page.blockLeft + 67
        y: 30
        width: 250
        spacing: 10

        BatteryBlock {
            summary: page.batterySummary
            percent: page.batteryPercent
        }

        InfoHeader {
            Layout.topMargin: 24
            text: qsTr("Conexão")
        }

        Repeater {
            model: page.inventoryDevices.filter(d => d.name === page.deviceName)

            delegate: ValueLabel {
                required property var modelData
                objectName: "inventoryDeviceLabel"
                text: modelData.name + ": " + modelData.summary
            }
        }

        Repeater {
            model: page.batteryDevices.filter(d => d.name === page.deviceName)

            delegate: Label {
                required property var modelData
                objectName: "batteryDeviceLabel"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: Theme.dropdownItem
                text: modelData.name + ": " + modelData.summary
            }
        }
    }

    Item {
        id: center
        objectName: "infoCenterColumn"
        x: page.blockLeft + 387
        y: 30
        width: 250
        height: 180

        DeviceDrawing {
            kind: page.deviceKind
            // Centrado na coluna; a caixa é mais larga que a coluna (como a imagem do G HUB) mas não toca as vizinhas.
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            boxWidth: 300
            boxHeight: 170
        }
    }

    // Identidade por HID++ (INC-015): a Brio 100 não fala HID++, então a seção nem é criada.
    ColumnLayout {
        id: right
        objectName: "infoRightColumn"
        x: page.blockLeft + 705
        y: 30
        width: 250
        spacing: 10

        Loader {
            Layout.fillWidth: true
            active: page.identityKey !== ""
            visible: active
            sourceComponent: identitySection
        }

        // Estado lido do dispositivo (G915, G703, G733): só por clique em "Ler estado".
        Loader {
            Layout.fillWidth: true
            Layout.topMargin: 14
            active: page.statusKind !== ""
            visible: active
            sourceComponent: statusSection
        }
    }

    Component {
        id: statusSection

        ColumnLayout {
            spacing: 10

            SectionHeader {
                text: qsTr("Estado (HID++)")
                help: qsTr("Lido do dispositivo só quando você clica. Valores marcados “não interpretado” são bytes que ainda não sabemos decodificar.")
                helpObjectName: "statusHelpIcon"
                titleColor: Theme.sliderFill
            }

            GHubButton {
                objectName: "readStatusButton"
                enabled: !page.statusViewModel.reading
                text: qsTr("Ler estado")
                onClicked: page.statusViewModel.read()
            }

            ValueLabel {
                objectName: "statusErrorLabel"
                visible: page.statusViewModel.error !== ""
                color: Theme.error
                text: page.statusViewModel.error
            }

            Repeater {
                model: page.statusRows

                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 12

                    Label {
                        objectName: "statusRowLabel"
                        Layout.preferredWidth: 100
                        Layout.alignment: Qt.AlignTop
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        font.capitalization: Font.AllUppercase
                        color: Theme.dropdownItem
                        text: modelData[0]
                    }

                    Label {
                        objectName: "statusRowValue"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 13
                        font.bold: true
                        color: Theme.foreground
                        text: modelData[1]
                    }
                }
            }
        }
    }

    Component {
        id: identitySection

        ColumnLayout {
            spacing: 10

            InfoHeader { text: qsTr("Identidade (HID++)") }

            GHubButton {
                objectName: "readIdentityButton"
                enabled: page.identity.state !== "reading"
                text: page.identity.state === "reading" ? qsTr("Lendo…") : qsTr("Ler identidade")
                onClicked: page.identityViewModel.read(page.identityKey)
            }

            ValueLabel {
                objectName: "identityErrorLabel"
                visible: page.identity.state === "error"
                color: Theme.error
                text: page.identity.error || ""
            }

            ValueLabel {
                objectName: "identityNameLabel"
                visible: page.identityOk
                text: qsTr("Nome: %1").arg(page.identity.name)
            }

            ValueLabel {
                objectName: "identityTypeLabel"
                visible: page.identityOk
                text: qsTr("Tipo: %1").arg(page.identity.type)
            }

            ValueLabel {
                objectName: "identityFirmwareLabel"
                visible: page.identityOk
                text: qsTr("Firmware: %1").arg(page.identityOk && page.identity.firmwares.length > 0
                                               ? page.identity.firmwares.join("\n") : qsTr("não informado"))
            }

            ValueLabel {
                objectName: "identityFeatureCountLabel"
                visible: page.identityOk
                text: qsTr("Features: %1").arg(page.identity.featureCount)
            }

            ValueLabel {
                objectName: "identityTruncationLabel"
                visible: page.identityOk && page.identity.truncationNote !== ""
                font.bold: false
                color: Theme.foregroundMedium
                text: page.identity.truncationNote || ""
            }

            GHubButton {
                objectName: "toggleFeatureIdsButton"
                flat: true
                visible: page.identityOk && page.identity.featureCount > 0
                text: page.featureIdsExpanded ? qsTr("Ocultar IDs das features") : qsTr("Mostrar IDs das features")
                onClicked: page.featureIdsExpanded = !page.featureIdsExpanded
            }

            ValueLabel {
                objectName: "identityFeatureIdsLabel"
                visible: page.identityOk && page.identity.featureCount > 0 && page.featureIdsExpanded
                font.bold: false
                text: page.identityOk ? page.identity.featureIds.join(", ") : ""
            }
        }
    }
}
