import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Editor de um LED do perfil onboard do G703 (INC-020): desligado, fixo (cor) ou ciclo (período e brilho). Só
// repassa o valor editado; a validação é do controlador. Um formato não interpretado ("raw") é mostrado sem edição
// e volta como foi lido.
ColumnLayout {
    id: root

    // Mapa do controlador: {kind, color, periodMs, brightness, raw}.
    property var value: ({kind: "off", color: "#000000", periodMs: 0, brightness: 0, raw: ""})
    property string title: ""
    property string idPrefix: "led"
    // Valor atual de edição (começa igual a `value`; o QML pai lê `edited`).
    property var edited: value

    Layout.fillWidth: true
    spacing: 0

    onValueChanged: edited = value

    function update(changes) {
        edited = Object.assign({}, edited, changes);
    }

    Label {
        text: root.title
        font.pixelSize: 12
        font.capitalization: Font.AllUppercase
        color: Theme.foregroundMedium
    }

    Label {
        objectName: root.idPrefix + "RawLabel"
        visible: root.edited.kind === "raw"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.foregroundMedium
        text: qsTr("Formato não interpretado (não será alterado): ") + root.edited.raw
    }

    GHubComboBox {
        objectName: root.idPrefix + "KindBox"
        visible: root.edited.kind !== "raw"
        Layout.fillWidth: true
        readonly property var kinds: ["off", "fixed", "cycle"]
        model: [qsTr("Desligado"), qsTr("Fixo"), qsTr("Ciclo")]
        currentIndex: Math.max(0, kinds.indexOf(root.edited.kind))
        onActivated: index => {
            const kind = kinds[index];
            if (kind === "fixed") {
                root.update({kind: kind, color: root.edited.color && root.edited.color !== "" ? root.edited.color : "#ffffff"});
            } else if (kind === "cycle") {
                root.update({kind: kind, periodMs: root.edited.periodMs > 0 ? root.edited.periodMs : 5000,
                             brightness: root.edited.brightness > 0 ? root.edited.brightness : 100});
            } else {
                root.update({kind: kind});
            }
        }
    }

    GHubTextField {
        objectName: root.idPrefix + "ColorField"
        visible: root.edited.kind === "fixed"
        Layout.fillWidth: true
        placeholderText: "#rrggbb"
        text: root.edited.color
        maximumLength: 7
        onTextEdited: root.update({color: text})
    }

    GHubSpinBox {
        objectName: root.idPrefix + "PeriodField"
        visible: root.edited.kind === "cycle"
        Layout.fillWidth: true
        from: 1000
        to: 60000
        stepSize: 100
        editable: true
        value: root.edited.periodMs
        onValueModified: root.update({periodMs: value})
    }

    GHubSpinBox {
        objectName: root.idPrefix + "BrightnessField"
        visible: root.edited.kind === "cycle"
        Layout.fillWidth: true
        from: 0
        to: 100
        stepSize: 5
        editable: true
        value: root.edited.brightness
        onValueModified: root.update({brightness: value})
    }
}
