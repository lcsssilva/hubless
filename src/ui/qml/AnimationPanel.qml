import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Seção "Animações" do painel de iluminação do G915 (INC-021, ADR-0030). Só apresentação: escolhe a animação e os
// parâmetros e encaminha ao controlador (SoftwareAnimationController); nenhuma regra de animação, taxa ou protocolo mora
// aqui. Nada é pedido ao carregar: só "Iniciar" e "Parar" falam com o controlador.
ColumnLayout {
    id: panel

    // SoftwareAnimationController, fornecido pelo composition root.
    required property QtObject controller
    // Cor escolhida na aba "Cores" (usada por estrelas, degradê e visualizador).
    property color accent: "#ffffff"
    // O modo ao vivo usa o mesmo acesso ao teclado: não dá para iniciar uma animação junto.
    property bool liveModeOn: false

    readonly property var kinds: [
        {name: qsTr("Ciclo de cores"), kind: "cycle"},
        {name: qsTr("Onda"), kind: "wave"},
        {name: qsTr("Estrelas"), kind: "stars"},
        {name: qsTr("Degradê"), kind: "keyframes"},
        {name: qsTr("Visualizador de áudio"), kind: "audio"},
        {name: qsTr("Amostragem da tela"), kind: "screen"}
    ]
    readonly property string kind: kinds[Math.max(kindCombo.currentIndex, 0)].kind
    property color secondColor: "#0000ff"
    readonly property bool idle: !controller.running && !controller.busy

    Layout.fillWidth: true
    spacing: 12

    SectionHeader {
        text: qsTr("Animação")
        help: qsTr("Animações calculadas pelo aplicativo e enviadas ao G915 como no modo ao vivo (volátil: nada é gravado no teclado). Ao parar, o efeito salvo volta. Limite de 20 quadros por segundo.")
    }

    GHubComboBox {
        id: kindCombo
        objectName: "animationKindCombo"
        Layout.fillWidth: true
        model: panel.kinds.map(k => k.name)
        currentIndex: 0
        enabled: panel.idle
    }

    SectionHeader {
        text: qsTr("Duração da volta (s)")
        visible: panel.kind === "cycle" || panel.kind === "wave" || panel.kind === "keyframes"
    }

    GHubSpinBox {
        id: periodSpin
        objectName: "animationPeriodSpin"
        visible: panel.kind === "cycle" || panel.kind === "wave" || panel.kind === "keyframes"
        from: 1
        to: 60
        value: 6
        enabled: panel.idle
    }

    GHubCheckBox {
        id: reverseCheck
        objectName: "animationReverseCheck"
        visible: panel.kind === "wave"
        text: qsTr("Da direita para a esquerda")
        enabled: panel.idle
    }

    SectionHeader {
        text: qsTr("Densidade das estrelas (%)")
        visible: panel.kind === "stars"
    }

    GHubSlider {
        id: densitySlider
        objectName: "animationDensitySlider"
        visible: panel.kind === "stars"
        Layout.fillWidth: true
        from: 0
        to: 100
        stepSize: 1
        value: 30
        showValue: true
        enabled: panel.idle
        Accessible.name: qsTr("Densidade das estrelas")
    }

    SectionHeader {
        text: qsTr("Segunda cor do degradê")
        visible: panel.kind === "keyframes"
    }

    Flow {
        Layout.fillWidth: true
        visible: panel.kind === "keyframes"
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
                objectName: "animationSecondSwatch"
                swatchColor: modelData.value
                colorName: modelData.name
                selected: Qt.colorEqual(panel.secondColor, modelData.value)
                enabled: panel.idle
                onClicked: panel.secondColor = modelData.value
            }
        }
    }

    SectionHeader {
        text: qsTr("Quadros por segundo")
    }

    GHubSpinBox {
        id: fpsSpin
        objectName: "animationFpsSpin"
        from: 1
        to: 20
        value: 10
        enabled: panel.idle
    }

    Label {
        objectName: "animationAudioNotice"
        Layout.fillWidth: true
        visible: panel.kind === "audio"
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.foregroundMedium
        text: qsTr("O visualizador captura o áudio da saída padrão apenas enquanto estiver ligado. Nenhum áudio é gravado: só os níveis por banda desenham as barras.")
    }

    Label {
        objectName: "animationScreenNotice"
        Layout.fillWidth: true
        visible: panel.kind === "screen"
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.foregroundMedium
        text: qsTr("A amostragem captura a tela apenas enquanto estiver ligada, e o sistema pede a sua permissão a cada vez que você inicia. Nenhuma imagem é gravada nem guardada: só a cor média de 10 faixas da tela desenha o teclado.")
    }

    Label {
        objectName: "animationLiveNotice"
        Layout.fillWidth: true
        visible: panel.liveModeOn && !panel.controller.running
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.foregroundMedium
        text: qsTr("Desligue o modo ao vivo para iniciar uma animação: os dois usam o mesmo acesso ao teclado.")
    }

    GHubButton {
        objectName: "animationStartButton"
        text: qsTr("Iniciar")
        enabled: panel.idle && !panel.liveModeOn
        onClicked: panel.controller.start(panel.kind, panel.accent, panel.secondColor, periodSpin.value,
                                          reverseCheck.checked, Math.round(densitySlider.value), fpsSpin.value)
    }

    GHubButton {
        objectName: "animationStopButton"
        text: qsTr("Parar")
        enabled: panel.controller.running && !panel.controller.busy
        onClicked: panel.controller.stop()
    }

    Label {
        objectName: "animationStatusLabel"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: Theme.foreground
        text: panel.controller.status
    }
}
