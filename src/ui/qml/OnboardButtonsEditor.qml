import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Editor das atribuições dos botões do perfil onboard do G703 (INC-022): uma linha por botão com o valor atual,
// o tipo (manter, padrão, botão do mouse, tecla) e o valor. Só monta a lista de pedidos para o controlador; a
// validação (botão 1..5, teclas da lista, confirmação do botão 1) é do domínio.
ColumnLayout {
    id: root

    // Do controlador: botões atuais {kind, number, modifiers, key, raw} e teclas permitidas {code, label}.
    property var buttons: []
    property var keyChoices: []
    // Pedidos de edição, um por botão: {kind: "keep"|"default"|"mouse"|"key", number, modifiers, key}.
    property var edited: []
    // O botão 1 recebeu algo diferente de "botão do mouse 1"/padrão/manter e o usuário confirmou.
    readonly property bool confirmPrimary: primaryNeedsConfirmation && confirmPrimaryBox.checked
    readonly property bool primaryNeedsConfirmation: edited.length > 0
        && (edited[0].kind === "key" || (edited[0].kind === "mouse" && edited[0].number !== 1))
    readonly property var kinds: ["keep", "default", "mouse", "key"]

    Layout.fillWidth: true
    spacing: 0

    function resetEdits() {
        const list = [];
        for (let i = 0; i < 6; ++i) {
            list.push({kind: "keep", number: 1, modifiers: 0, key: keyChoices.length > 0 ? keyChoices[0].code : 0});
        }
        edited = list;
        confirmPrimaryBox.checked = false;
    }

    function update(index, changes) {
        const next = edited.slice();
        next[index] = Object.assign({}, next[index], changes);
        edited = next;
    }

    function keyLabel(code) {
        const found = keyChoices.find(choice => choice.code === code);
        return found ? found.label : "0x" + code.toString(16);
    }

    function describe(button) {
        if (button === undefined) {
            return "";
        }
        if (button.kind === "mouse") {
            return qsTr("Botão do mouse ") + button.number;
        }
        if (button.kind === "key") {
            let text = "";
            if (button.modifiers & 1) { text += "Ctrl+"; }
            if (button.modifiers & 2) { text += "Shift+"; }
            if (button.modifiers & 4) { text += "Alt+"; }
            if (button.modifiers & 8) { text += "Super+"; }
            return qsTr("Tecla ") + text + keyLabel(button.key);
        }
        if (button.kind === "none") {
            return qsTr("Sem ação");
        }
        return qsTr("Não interpretado: ") + button.raw;
    }

    onButtonsChanged: resetEdits()
    Component.onCompleted: resetEdits()

    Repeater {
        model: root.edited.length

        delegate: ColumnLayout {
            id: row
            required property int index
            readonly property var request: root.edited[index]
            Layout.fillWidth: true
            spacing: 0

            Label {
                objectName: "onboardButtonLabel"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                font.capitalization: Font.AllUppercase
                color: Theme.foregroundMedium
                text: qsTr("Botão ") + (row.index + 1) + " · " + root.describe(root.buttons[row.index])
            }

            GHubComboBox {
                objectName: "onboardButtonKindBox"
                Layout.fillWidth: true
                model: [qsTr("Manter"), qsTr("Padrão de fábrica"), qsTr("Botão do mouse"), qsTr("Tecla")]
                currentIndex: Math.max(0, root.kinds.indexOf(row.request.kind))
                onActivated: index => root.update(row.index, {kind: root.kinds[index]})
            }

            GHubComboBox {
                objectName: "onboardButtonMouseBox"
                visible: row.request.kind === "mouse"
                Layout.fillWidth: true
                model: [1, 2, 3, 4, 5].map(n => qsTr("Botão ") + n)
                currentIndex: row.request.number - 1
                onActivated: index => root.update(row.index, {number: index + 1})
            }

            GHubComboBox {
                objectName: "onboardButtonKeyBox"
                visible: row.request.kind === "key"
                Layout.fillWidth: true
                model: root.keyChoices.map(choice => choice.label)
                currentIndex: Math.max(0, root.keyChoices.findIndex(choice => choice.code === row.request.key))
                onActivated: index => root.update(row.index, {key: root.keyChoices[index].code})
            }

            RowLayout {
                visible: row.request.kind === "key"
                Layout.fillWidth: true

                Repeater {
                    model: [{label: "Ctrl", bit: 1}, {label: "Shift", bit: 2}, {label: "Alt", bit: 4}, {label: "Super", bit: 8}]

                    delegate: GHubCheckBox {
                        required property var modelData
                        objectName: "onboardButtonModifier" + modelData.label
                        text: modelData.label
                        checked: (row.request.modifiers & modelData.bit) !== 0
                        onClicked: root.update(row.index, {modifiers: checked ? (row.request.modifiers | modelData.bit)
                                                                              : (row.request.modifiers & ~modelData.bit)})
                    }
                }
            }
        }
    }

    GHubCheckBox {
        id: confirmPrimaryBox
        objectName: "onboardPrimaryConfirmBox"
        visible: root.primaryNeedsConfirmation
        Layout.fillWidth: true
        text: qsTr("Confirmo: o botão 1 é o clique principal e deixará de clicar")
    }
}
