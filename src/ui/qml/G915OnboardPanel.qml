import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Seção "Memória do teclado" da aba "Jogo" do painel do G915 (INC-019 R4, INC-022 R1/R4, ADR-0029): efeito gravado do logo e das teclas e teclas
// G1–G5 do perfil onboard ativo. Só apresentação: copia o estado do controlador para os campos de edição e encaminha
// cada clique ao controlador (G915OnboardController), que valida e grava; nenhuma regra de protocolo, faixa de valor ou
// formato de byte mora aqui. Nada é pedido ao carregar: só os botões falam com o controlador.
ColumnLayout {
    id: panel

    // G915OnboardController, fornecido pelo composition root.
    required property QtObject controller
    // O modo ao vivo e as animações usam o mesmo acesso ao teclado: com um deles ligado, os controles ficam bloqueados.
    property bool liveModeOn: false
    property bool animationRunning: false

    readonly property bool blocked: liveModeOn || animationRunning
    readonly property bool idle: !controller.busy && !blocked

    // Estado de edição (só UI): copiado do controlador quando ele termina uma operação.
    property var editLogo: ({kind: "raw", color: "#000000", periodMs: 0, brightness: 0, speed: 0})
    property var editKeys: ({kind: "raw", color: "#000000", periodMs: 0, brightness: 0, speed: 0})
    property var editGKeys: []

    readonly property var logoKinds: [
        {key: "static", text: qsTr("Cor fixa")},
        {key: "breathing", text: qsTr("Respiração")},
        {key: "cycle", text: qsTr("Ciclo de cores")}
    ]
    readonly property var keysKinds: [
        {key: "static", text: qsTr("Cor fixa")},
        {key: "breathing", text: qsTr("Respiração")},
        {key: "cycle", text: qsTr("Ciclo de cores")},
        {key: "wave", text: qsTr("Onda")},
        {key: "ripple", text: qsTr("Ondulação")}
    ]
    readonly property var modifierBits: [
        {bit: 1, text: qsTr("Ctrl")},
        {bit: 2, text: qsTr("Shift")},
        {bit: 4, text: qsTr("Alt")},
        {bit: 8, text: qsTr("Super")}
    ]

    Layout.fillWidth: true
    spacing: 12

    function syncFromController() {
        if (controller.busy) {
            return;
        }
        editLogo = controller.logoEffect;
        editKeys = controller.keysEffect;
        editGKeys = controller.gKeys.slice();
    }

    function updateGKey(index, changes) {
        const next = editGKeys.slice();
        next[index] = Object.assign({}, next[index], changes);
        editGKeys = next;
    }

    Component.onCompleted: syncFromController()

    Connections {
        target: panel.controller
        function onStateChanged() { panel.syncFromController() }
    }

    // Editor de um efeito gravado: tipo, cor, período, brilho e velocidade conforme o tipo. O valor volta por `edited`.
    component EffectFields: ColumnLayout {
        id: fields

        property var kinds: []
        property var value: ({kind: "raw", color: "#000000", periodMs: 0, brightness: 0, speed: 0})
        property string idPrefix: "g915MemEffect"
        property var edited: value
        readonly property bool isRaw: edited.kind === "raw"
        readonly property bool hasColor: ["static", "breathing", "ripple"].indexOf(edited.kind) >= 0
        readonly property bool hasTiming: ["breathing", "cycle", "wave"].indexOf(edited.kind) >= 0

        Layout.fillWidth: true
        spacing: 4

        onValueChanged: edited = value

        function update(changes) {
            edited = Object.assign({}, edited, changes);
        }

        Label {
            objectName: fields.idPrefix + "RawLabel"
            visible: fields.isRaw
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: Theme.foregroundMedium
            text: qsTr("Formato não interpretado (não será alterado).")
        }

        GHubComboBox {
            objectName: fields.idPrefix + "KindBox"
            visible: !fields.isRaw
            Layout.fillWidth: true
            model: fields.kinds.map(k => k.text)
            currentIndex: fields.kinds.findIndex(k => k.key === fields.edited.kind)
            onActivated: index => {
                const kind = fields.kinds[index].key;
                fields.update({
                    kind: kind,
                    color: fields.edited.color && fields.edited.color !== "" ? fields.edited.color : "#ffffff",
                    periodMs: fields.edited.periodMs > 0 ? fields.edited.periodMs : 5000,
                    brightness: fields.edited.brightness > 0 ? fields.edited.brightness : 100,
                    speed: fields.edited.speed > 0 ? fields.edited.speed : 20
                });
            }
        }

        GHubTextField {
            objectName: fields.idPrefix + "ColorField"
            visible: fields.hasColor
            Layout.fillWidth: true
            placeholderText: "#rrggbb"
            text: fields.edited.color
            maximumLength: 7
            onTextEdited: fields.update({color: text})
        }

        GHubSpinBox {
            objectName: fields.idPrefix + "PeriodField"
            visible: fields.hasTiming
            Layout.fillWidth: true
            from: 1000
            to: 20000
            stepSize: 100
            editable: true
            value: fields.edited.periodMs
            onValueModified: fields.update({periodMs: value})
        }

        GHubSpinBox {
            objectName: fields.idPrefix + "BrightnessField"
            visible: fields.hasTiming
            Layout.fillWidth: true
            from: 1
            to: 100
            stepSize: 5
            editable: true
            value: fields.edited.brightness
            onValueModified: fields.update({brightness: value})
        }

        GHubSpinBox {
            objectName: fields.idPrefix + "SpeedField"
            visible: fields.edited.kind === "ripple"
            Layout.fillWidth: true
            from: 2
            to: 200
            stepSize: 1
            editable: true
            value: fields.edited.speed
            onValueModified: fields.update({speed: value})
        }
    }

    SectionHeader {
        text: qsTr("Memória do teclado")
        help: qsTr("Grava no perfil onboard ativo do G915 (M1, M2 ou M3): o efeito de iluminação do logo e das teclas e as teclas G1–G5. Antes da primeira gravação é salva uma cópia do perfil; depois de gravar, o setor é relido e conferido byte a byte. Só os formatos vistos nas cópias reais da memória podem ser editados; o resto é preservado. Desligue o modo ao vivo e as animações antes: todos usam o mesmo acesso ao teclado.")
        helpObjectName: "g915MemNoticeLabel"
    }

    // Exceção de segurança: esta ação grava no dispositivo, então a linha curta fica sempre visível.
    Label {
        objectName: "g915MemWriteWarningLabel"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        font.bold: true
        color: Theme.foreground
        text: qsTr("Grava no perfil ativo do teclado.")
    }

    GHubButton {
        objectName: "g915MemReadButton"
        text: qsTr("Ler do teclado")
        enabled: panel.idle
        onClicked: panel.controller.readFromKeyboard()
    }

    Label {
        objectName: "g915MemLiveNotice"
        Layout.fillWidth: true
        visible: panel.blocked
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.foregroundMedium
        text: qsTr("Desligue o modo ao vivo e as animações para usar a memória do teclado: todos usam o mesmo acesso.")
    }

    ColumnLayout {
        id: editor
        objectName: "g915MemEditor"
        Layout.fillWidth: true
        visible: panel.controller.loaded
        spacing: 8

        Label {
            objectName: "g915MemSectorLabel"
            Layout.fillWidth: true
            font.pixelSize: 12
            color: Theme.foregroundMedium
            text: qsTr("Perfil ativo: M%1").arg(panel.controller.activeSector)
        }

        SectionHeader { text: qsTr("Efeito gravado: logo") }

        EffectFields {
            objectName: "g915MemLogoFields"
            kinds: panel.logoKinds
            value: panel.editLogo
            idPrefix: "g915MemLogo"
            enabled: panel.idle
            onEditedChanged: panel.editLogo = edited
        }

        SectionHeader { text: qsTr("Efeito gravado: teclas") }

        EffectFields {
            objectName: "g915MemKeysFields"
            kinds: panel.keysKinds
            value: panel.editKeys
            idPrefix: "g915MemKeys"
            enabled: panel.idle
            onEditedChanged: panel.editKeys = edited
        }

        SectionHeader {
            text: qsTr("Teclas G")
            help: qsTr("Cada tecla G1–G5 pode voltar ao valor da cópia original (\"Padrão\") ou enviar uma tecla com modificadores (Ctrl, Shift, Alt, Super). Atribuições em outros formatos aparecem como não interpretadas e não são alteradas.")
        }

        Repeater {
            model: panel.editGKeys.length

            delegate: ColumnLayout {
                id: gRow
                required property int index
                readonly property var entry: panel.editGKeys[gRow.index] || ({kind: "raw", modifiers: 0, key: "", raw: ""})
                readonly property bool isRaw: gRow.entry.kind === "raw"
                Layout.fillWidth: true
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        objectName: "g915MemGKeyLabel"
                        text: "G" + (gRow.index + 1)
                        font.pixelSize: 12
                        font.capitalization: Font.AllUppercase
                        color: Theme.foregroundMedium
                    }

                    GHubComboBox {
                        objectName: "g915MemGKeyKindBox"
                        Layout.fillWidth: true
                        visible: !gRow.isRaw
                        enabled: panel.idle
                        readonly property var kinds: ["default", "key"]
                        model: [qsTr("Padrão (cópia)"), qsTr("Tecla")]
                        currentIndex: Math.max(0, kinds.indexOf(gRow.entry.kind))
                        onActivated: index => panel.updateGKey(gRow.index, {kind: kinds[index]})
                    }

                    Label {
                        objectName: "g915MemGKeyRawLabel"
                        Layout.fillWidth: true
                        visible: gRow.isRaw
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                        color: Theme.foregroundMedium
                        text: qsTr("Formato não interpretado (não será alterado): ") + gRow.entry.raw
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    visible: gRow.entry.kind === "key"
                    spacing: 12

                    Repeater {
                        model: panel.modifierBits

                        delegate: GHubCheckBox {
                            required property var modelData
                            objectName: "g915MemGKeyModifier"
                            text: modelData.text
                            enabled: panel.idle
                            checked: (gRow.entry.modifiers & modelData.bit) !== 0
                            onToggled: panel.updateGKey(gRow.index, {
                                modifiers: checked ? (gRow.entry.modifiers | modelData.bit)
                                                   : (gRow.entry.modifiers & ~modelData.bit)
                            })
                        }
                    }
                }

                GHubComboBox {
                    objectName: "g915MemGKeyKeyBox"
                    Layout.fillWidth: true
                    visible: gRow.entry.kind === "key"
                    enabled: panel.idle
                    model: panel.controller.keyChoices
                    currentIndex: panel.controller.keyChoices.indexOf(gRow.entry.key)
                    placeholderText: qsTr("Tecla")
                    onActivated: index => panel.updateGKey(gRow.index, {key: panel.controller.keyChoices[index]})
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true

            GHubButton {
                objectName: "g915MemWriteButton"
                text: qsTr("Gravar no teclado")
                enabled: panel.idle
                onClicked: panel.controller.writeToKeyboard(panel.editLogo, panel.editKeys, panel.editGKeys)
            }

            GHubButton {
                objectName: "g915MemRestoreButton"
                text: qsTr("Restaurar cópia")
                enabled: panel.idle && panel.controller.backupExists
                onClicked: panel.controller.restoreBackup()
            }
        }
    }

    Label {
        objectName: "g915MemStatusLabel"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.foreground
        text: panel.controller.status
    }
}
