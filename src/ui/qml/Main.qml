import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
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

    // G703OnboardController (módulo pointer), fornecido pelo composition root: ler, gravar e restaurar a memória
    // onboard do G703 só por clique (ADR-0017).
    required property QtObject g703OnboardController

    // KeyboardLightingController (módulo lighting), fornecido pelo composition root: o modo ao vivo do G915
    // só é ligado por clique.
    required property QtObject keyboardLightingController

    // HeadsetSettingsController (módulo headset), fornecido pelo composition root: sidetone e luzes do G733 só
    // são gravados por clique (ADR-0015).
    required property QtObject headsetSettingsController

    // HostEqualizerController (módulo audio_processing), fornecido pelo composition root: o EQ do G733 roda no PC
    // (PipeWire) e só é ligado por clique (ADR-0016).
    required property QtObject hostEqualizerController

    // NoiseSuppressionController (módulo audio_processing), fornecido pelo composition root: a redução de ruído do
    // microfone do G733 roda no PC (RNNoise via PipeWire) e só é ligada por clique (ADR-0018).
    required property QtObject noiseSuppressionController

    // DesktopController (src/app), fornecido pelo composition root: política da bandeja, do fechamento e do
    // autostart (INC-013 R2–R5/R8/R9).
    required property QtObject desktopController

    // DiagnosticsViewModel (módulo diagnostics), fornecido pelo composition root: saúde dos backends, últimas
    // entradas e exportação do relatório só por clique (INC-013 R14).
    required property QtObject diagnosticsViewModel

    // ProfilesController (módulo profiles), fornecido pelo composition root: salvar, aplicar e remover perfis só por
    // clique (ADR-0020). Nada é aplicado na abertura.
    required property QtObject profilesController

    // DeviceIdentityViewModel (módulo device_inventory), fornecido pelo composition root: a identidade HID++ de
    // G915, G703 e G733 só é lida por clique na aba Informações (INC-015, ADR-0024).
    required property QtObject deviceIdentityViewModel

    // KeyboardStatusViewModel, PointerSensorStatusViewModel e HeadsetStatusViewModel, fornecidos pelo composition
    // root: o estado HID++ de G915, G703 e G733 só é lido por clique na página Informações.
    required property QtObject keyboardStatusViewModel
    required property QtObject pointerStatusViewModel
    required property QtObject headsetStatusViewModel

    // Nome do perfil à espera da confirmação de remoção; estado de UI apenas.
    property string profilePendingRemoval: ""
    // Nome do perfil à espera da confirmação de atualização; estado de UI apenas.
    property string profilePendingUpdate: ""

    // Cor usada pelos efeitos de prévia; estado de UI apenas (atualizada ao escolher uma cor).
    property string lightingEffectColor: "#ffffff"


    // Página aberta (0 Início, 1 G915, 2 G703, 3 G733, 4 Brio 100, 5 Perfis, 6 Configurações) e aba de cada
    // página de dispositivo; estado de UI apenas.
    property int currentPage: 0
    property string g915Tab: "lighting"
    property string g703Tab: "onboard"
    property string g733Tab: "headset"
    property string brioTab: "image"

    readonly property int homePage: 0
    // Catálogo da tela inicial, na ordem dos cartões; `wide` marca o cartão largo do teclado.
    readonly property var deviceCatalog: [
        {name: "G915", kind: "keyboard", page: 1, wide: true},
        {name: "G703", kind: "mouse", page: 2, wide: false},
        {name: "G733", kind: "headset", page: 3, wide: false},
        {name: "Brio 100", kind: "camera", page: 4, wide: false}
    ]
    // Páginas de dispositivo (1 a 4) e seus nomes, usados no título do cabeçalho.
    readonly property var deviceNames: ({1: "G915", 2: "G703", 3: "G733", 4: "Brio 100"})
    readonly property bool deviceOpen: currentPage >= 1 && currentPage <= 4
    readonly property int brioPage: 4
    readonly property int profilesPage: 5
    readonly property int settingsPage: 6

    // Abre uma página. Sair da página da Brio 100 sempre para a pré-visualização: o stream nunca fica ligado numa
    // página invisível (INC-014 R7). Navegar não lê nem grava nada em dispositivo.
    function openPage(page) {
        if (window.currentPage === window.brioPage && page !== window.brioPage)
            window.cameraPreviewViewModel.stop()
        window.currentPage = page
    }

    function openDevice(page) {
        if (page === 1) window.g915Tab = "lighting"
        else if (page === 2) window.g703Tab = "onboard"
        else if (page === 3) window.g733Tab = "headset"
        else if (page === 4) window.brioTab = "image"
        window.openPage(page)
    }

    // Item da lista de um ViewModel com o nome dado, ou null.
    function entryFor(list, name) {
        for (let i = 0; i < list.length; ++i) {
            if (list[i].name === name)
                return list[i]
        }
        return null
    }

    // Resumo e nível da bateria de um dispositivo para a área principal da página; sem item = sem bateria.
    function batteryTextOf(name) {
        const entry = entryFor(batteryViewModel.devices, name)
        return entry !== null ? entry.summary : ""
    }

    function batteryPercentOf(name) {
        const entry = entryFor(batteryViewModel.devices, name)
        return entry !== null && entry.percent !== undefined ? entry.percent : -1
    }

    function isDetected(entry) {
        return entry !== null && entry.summary !== qsTr("não detectado")
    }

    width: 1100
    height: 800
    minimumWidth: 1024
    minimumHeight: 768
    // `--autostart` inicia escondido na bandeja; sem bandeja, o controlador não esconde (INC-013 R9).
    visible: !desktopController.startHidden
    title: qsTr("Hubless")

    Material.theme: Material.Dark
    Material.accent: Theme.primary
    Material.background: Theme.background
    Material.primary: Theme.surface
    Material.foreground: Theme.foreground

    // A aparência não pode depender da paleta do sistema (o app pode rodar sem o plugin de tema do KDE nem com o
    // estilo Material): a paleta da janela vem dos tokens do Theme e é herdada por todos os controles.
    palette.window: Theme.background
    palette.windowText: Theme.foreground
    palette.base: Theme.surface
    palette.alternateBase: Theme.surface
    palette.text: Theme.foreground
    palette.button: Theme.buttonSurface
    palette.buttonText: Theme.foreground
    palette.brightText: Theme.foreground
    palette.placeholderText: Theme.foregroundMedium
    palette.highlight: Theme.primary
    palette.highlightedText: Theme.foreground
    palette.toolTipBase: Theme.foreground
    palette.toolTipText: "#000000"
    palette.link: Theme.primary
    palette.mid: Theme.sliderTrack
    palette.dark: Theme.surface
    palette.light: Theme.sliderFill

    // Encerra o stream da câmera ao fechar a janela. Com bandeja (hideOnClose) e fora de um encerramento
    // pedido, recusa o fechamento e esconde a janela: o processo continua e a bandeja reabre (INC-013 R4/R5).
    onClosing: function(close) {
        window.cameraPreviewViewModel.stop()
        if (window.desktopController.hideOnClose && !window.desktopController.quitting) {
            close.accepted = false
            window.hide()
        }
    }

    // Texto corrido de um painel: ocupa a largura da coluna e quebra a linha.
    component BodyLabel: Label {
        color: Theme.foreground
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
    }

    // Página de uma aba: rolagem própria e coluna com a margem de 16 px do painel (278 px úteis no painel de 310).
    component TabPage: ScrollView {
        id: tabPage
        default property alias content: tabColumn.data
        contentWidth: availableWidth
        contentHeight: tabColumn.implicitHeight + 32

        ColumnLayout {
            id: tabColumn
            x: 16
            y: 8
            width: tabPage.availableWidth - 32
            spacing: 12
        }
    }

    // Barra do topo (INC-014 parte 3, R19): 88 px, preta, divisor de 1 px na base. À esquerda, a marca própria
    // (tela inicial) ou a seta e o nome do dispositivo (páginas de dispositivo); depois as abas e a engrenagem.
    header: Item {
        height: 88

        Rectangle {
            id: topBar
            objectName: "topBar"
            anchors.fill: parent
            color: Theme.background

            Label {
                objectName: "brandLabel"
                color: Theme.foreground
                visible: !window.deviceOpen
                // Com a seta de voltar à esquerda (Perfis e Configurações), a marca vem depois dela.
                x: window.currentPage === window.homePage ? 39 : 81
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Hubless")
                font.pixelSize: 24
                font.bold: true
            }

            AbstractButton {
                id: backButton
                objectName: "backButton"
                visible: window.currentPage !== window.homePage
                // Na página do dispositivo a seta fica em x≈25, y≈48 (REFERENCIA-ESTILO-GHUB.md §4.1), sem fundo.
                x: window.deviceOpen ? 12 : 13
                y: window.deviceOpen ? 32 : 20
                width: 48
                height: 48
                hoverEnabled: true
                Accessible.name: qsTr("Voltar")
                onClicked: window.openPage(window.homePage)

                contentItem: Label {
                    text: "←"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 28
                    color: backButton.hovered ? Theme.primary : Theme.foreground
                }

                background: null
            }

            Label {
                objectName: "deviceTitle"
                color: Theme.foreground
                visible: window.deviceOpen
                x: 81
                y: 27
                text: window.deviceNames[window.currentPage] ?? ""
                font.pixelSize: 36
                font.bold: true
            }

            // Abas, engrenagem e divisor só existem na tela inicial e nas páginas Perfis e Configurações:
            // a página do dispositivo tem só a seta e o título (item 1 do pedido de estilo).
            Row {
                visible: !window.deviceOpen
                x: 280
                height: parent.height
                spacing: 16

                TopBarTab {
                    objectName: "devicesTabButton"
                    text: qsTr("Dispositivos")
                    active: window.currentPage !== window.profilesPage && window.currentPage !== window.settingsPage
                    onClicked: window.openPage(window.homePage)
                }

                TopBarTab {
                    objectName: "profilesPageButton"
                    text: qsTr("Perfis")
                    active: window.currentPage === window.profilesPage
                    onClicked: window.openPage(window.profilesPage)
                }
            }

            ToolButton {
                objectName: "settingsPageButton"
                visible: !window.deviceOpen
                anchors.right: parent.right
                anchors.rightMargin: 40
                anchors.verticalCenter: parent.verticalCenter
                display: AbstractButton.IconOnly
                icon.name: "applications-system"
                icon.width: 20
                icon.height: 20
                icon.color: window.currentPage === window.settingsPage ? Theme.primary : Theme.tabForeground
                Accessible.name: qsTr("Configurações")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Configurações")
                onClicked: window.openPage(window.settingsPage)
            }

            Rectangle {
                objectName: "topBarDivider"
                visible: !window.deviceOpen
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.divider
            }
        }
    }

    StackLayout {
        id: pageStack
        objectName: "pageStack"
        anchors.fill: parent
        currentIndex: window.currentPage

        // 0 Início: um cartão por dispositivo do catálogo, em grade centralizada (intervalo de 24 px).
        ScrollView {
            id: homeScroll
            objectName: "homeScroll"
            contentWidth: availableWidth
            contentHeight: cardGrid.height + 2 * 32

            Flow {
                id: cardGrid
                objectName: "cardGrid"
                y: 32
                spacing: 24
                // Largura da maior linha ao empacotar os cartões na largura disponível; assim a grade fica
                // centralizada mesmo com linhas incompletas.
                width: {
                    const available = homeScroll.availableWidth - 2 * 24
                    let row = 0, widest = 0
                    for (const entry of window.deviceCatalog) {
                        const cardWidth = entry.wide ? 616 : 296
                        if (row > 0 && row + cardGrid.spacing + cardWidth > available) {
                            widest = Math.max(widest, row)
                            row = cardWidth
                        } else {
                            row = row === 0 ? cardWidth : row + cardGrid.spacing + cardWidth
                        }
                    }
                    return Math.max(widest, row)
                }
                x: Math.max(0, (homeScroll.availableWidth - width) / 2)

                Repeater {
                    model: window.deviceCatalog

                    delegate: DeviceCard {
                        id: card
                        required property var modelData
                        readonly property var inventoryEntry: window.entryFor(window.deviceInventoryViewModel.devices, modelData.name)
                        readonly property var batteryEntry: window.entryFor(window.batteryViewModel.devices, modelData.name)

                        deviceName: modelData.name
                        deviceKind: modelData.kind
                        wide: modelData.wide
                        detected: window.isDetected(inventoryEntry)
                        connectionText: inventoryEntry !== null ? inventoryEntry.summary : qsTr("não detectado")
                        batteryText: batteryEntry !== null ? batteryEntry.summary : ""
                        batteryPercent: batteryEntry !== null && batteryEntry.percent !== undefined ? batteryEntry.percent : -1
                        onClicked: window.openDevice(modelData.page)
                    }
                }
            }
        }


        // 1 G915
        DevicePage {
            deviceName: "G915"
            deviceKind: "keyboard"
            batteryText: window.batteryTextOf("G915")
            batteryPercent: window.batteryPercentOf("G915")
            tabs: [{key: "lighting", text: qsTr("Iluminação"), icon: "brightness-high"},
                   {key: "info", text: qsTr("Informações"), icon: "applications-system"}]
            currentTab: window.g915Tab
            onTabSelected: key => window.g915Tab = key

            TabPage {
                // O modo ao vivo e cada cor só são pedidos por clique; o QML apenas encaminha ao controlador e
                // mostra o estado que o controlador informa.
                SwitchRow {
                    text: qsTr("Modo ao vivo")
                    help: qsTr("No modo ao vivo o aplicativo controla as cores do G915. Nada é gravado no teclado: ao desligar o modo ou fechar o aplicativo, o efeito salvo volta. Os efeitos são uma prévia: somem ao desligar o modo ao vivo, como observado neste teclado.")
                    helpObjectName: "lightingNoticeLabel"
                    switchObjectName: "lightingLiveSwitch"
                    checked: window.keyboardLightingController.live
                    switchEnabled: !window.keyboardLightingController.busy
                    onToggleRequested: wanted => wanted ? window.keyboardLightingController.startLive()
                                                        : window.keyboardLightingController.stopLive()
                }

                InnerTabs {
                    id: lightingTabs
                    objectName: "lightingTabs"
                    model: [qsTr("Cores"), qsTr("Efeitos")]
                }

                SectionHeader {
                    text: qsTr("Cor")
                    visible: lightingTabs.currentIndex === 0
                }

                // Amostras circulares de 20 px; o "+" abre o seletor de cor.
                Flow {
                    Layout.fillWidth: true
                    visible: lightingTabs.currentIndex === 0
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
                            objectName: "lightingSwatch"
                            swatchColor: modelData.value
                            colorName: modelData.name
                            selected: Qt.colorEqual(window.lightingEffectColor, modelData.value)
                            enabled: window.keyboardLightingController.live && !window.keyboardLightingController.busy
                            onClicked: {
                                window.lightingEffectColor = modelData.value
                                window.keyboardLightingController.paintAll(modelData.value)
                            }
                        }
                    }

                    ColorSwatch {
                        objectName: "lightingCustomColorButton"
                        plus: true
                        colorName: qsTr("Outra cor…")
                        enabled: window.keyboardLightingController.live && !window.keyboardLightingController.busy
                        onClicked: lightingColorDialog.open()
                    }
                }

                // Prévia de efeito de hardware: só enquanto o modo ao vivo está ligado; escolher o item faz o mesmo
                // pedido que o botão de cada efeito fazia.
                SectionHeader {
                    objectName: "lightingEffectHeader"
                    text: qsTr("Efeito")
                    visible: lightingTabs.currentIndex === 1
                }

                GHubComboBox {
                    id: lightingEffectCombo
                    objectName: "lightingEffectCombo"
                    readonly property var effects: [
                        {name: qsTr("Onda"), kind: "wave"},
                        {name: qsTr("Ciclo de cores"), kind: "cycle"},
                        {name: qsTr("Respiração"), kind: "breathing"},
                        {name: qsTr("Ondulação"), kind: "ripple"}
                    ]
                    visible: lightingTabs.currentIndex === 1
                    model: effects.map(e => e.name)
                    currentIndex: -1
                    placeholderText: qsTr("Escolher efeito")
                    enabled: window.keyboardLightingController.live && !window.keyboardLightingController.busy
                    onActivated: index => window.keyboardLightingController.applyEffect(effects[index].kind,
                                                                                         window.lightingEffectColor)
                }

                BodyLabel {
                    objectName: "lightingEffectPreviewNote"
                    visible: lightingTabs.currentIndex === 1
                    font.pixelSize: 12
                    color: Theme.foregroundMedium
                    text: qsTr("Prévia: some ao desligar o modo ao vivo.")
                }

                BodyLabel {
                    objectName: "lightingStatusLabel"
                    text: window.keyboardLightingController.status
                }
            }

            infoContent: InfoPage {
                anchors.fill: parent
                deviceName: "G915"
                deviceKind: "keyboard"
                identityKey: "g915"
                statusKind: "keyboard"
                statusViewModel: window.keyboardStatusViewModel
                identityViewModel: window.deviceIdentityViewModel
                inventoryDevices: window.deviceInventoryViewModel.devices
                batteryDevices: window.batteryViewModel.devices
                batterySummary: window.batteryTextOf("G915")
                batteryPercent: window.batteryPercentOf("G915")
            }
        }


        // 2 G703
        DevicePage {
            deviceName: "G703"
            deviceKind: "mouse"
            batteryText: window.batteryTextOf("G703")
            batteryPercent: window.batteryPercentOf("G703")
            tabs: [{key: "onboard", text: qsTr("Sensibilidade (DPI)"), icon: "speedometer"},
                   {key: "ratbag", text: qsTr("Leitura (ratbagd)"), icon: "view-refresh"},
                   {key: "info", text: qsTr("Informações"), icon: "applications-system"}]
            currentTab: window.g703Tab
            onTabSelected: key => window.g703Tab = key

            TabPage {
                // Memória do mouse por HID++ (INC-011): o QML só repassa os valores editados ao controlador,
                // que valida e grava; nada é lido nem gravado na abertura.
                SectionHeader {
                    text: qsTr("Memória do mouse")
                    help: qsTr("Grava na memória do mouse (perfil ativo): taxa, estágios de DPI, estágio do DPI shift e LEDs. Antes da primeira gravação é salva uma cópia do perfil. Feche o Piper e não use o ratbagd ao mesmo tempo: um Commit dele sobrescreve estes valores.")
                    helpObjectName: "onboardNoticeLabel"
                }

                // Exceção de segurança: esta ação grava no dispositivo, então a linha curta fica sempre visível.
                BodyLabel {
                    objectName: "onboardWriteWarningLabel"
                    font.bold: true
                    text: qsTr("Grava no perfil ativo do mouse.")
                }

                GHubButton {
                    objectName: "onboardReadButton"
                    text: qsTr("Ler do mouse")
                    enabled: !window.g703OnboardController.busy
                    onClicked: window.g703OnboardController.readFromMouse()
                }

                ColumnLayout {
                    id: onboardEditor
                    objectName: "onboardEditor"
                    Layout.fillWidth: true
                    visible: window.g703OnboardController.loaded

                    // Estado de edição (UI apenas): copiado do controlador quando ele termina uma operação.
                    property var editStages: []
                    property int editDefault: 0
                    property int editRate: 1000
                    property int editShift: -1
                    property var editPrimaryLed: ({kind: "off"})
                    property var editLogoLed: ({kind: "off"})

                    function syncFromController() {
                        if (window.g703OnboardController.busy) {
                            return;
                        }
                        editStages = window.g703OnboardController.dpiStages.slice();
                        editDefault = window.g703OnboardController.defaultStage;
                        editRate = window.g703OnboardController.reportRateHz;
                        editShift = window.g703OnboardController.dpiShiftStage;
                        editPrimaryLed = window.g703OnboardController.primaryLed;
                        editLogoLed = window.g703OnboardController.logoLed;
                    }

                    Component.onCompleted: syncFromController()

                    Connections {
                        target: window.g703OnboardController
                        function onBusyChanged() { onboardEditor.syncFromController() }
                        function onReportRateHzChanged() { onboardEditor.syncFromController() }
                        function onDpiStagesChanged() { onboardEditor.syncFromController() }
                        function onDefaultStageChanged() { onboardEditor.syncFromController() }
                        function onDpiShiftStageChanged() { onboardEditor.syncFromController() }
                        function onPrimaryLedChanged() { onboardEditor.syncFromController() }
                        function onLogoLedChanged() { onboardEditor.syncFromController() }
                    }

                    SectionHeader { text: qsTr("Report rate (por segundo)") }

                    ColumnLayout {
                        Layout.fillWidth: true

                        GHubComboBox {
                            objectName: "onboardRateBox"
                            model: [125, 250, 500, 1000]
                            currentIndex: Math.max(0, model.indexOf(onboardEditor.editRate))
                            onActivated: index => onboardEditor.editRate = model[index]
                        }
                    }

                    Repeater {
                        model: onboardEditor.editStages.length

                        // Dois níveis por estágio: nome e "padrão" em cima, o campo de DPI embaixo (cabe nos 278 px).
                        delegate: ColumnLayout {
                            id: stageRow
                            required property int index
                            Layout.fillWidth: true
                            spacing: 0

                            RowLayout {
                                Layout.fillWidth: true

                                Label {
                                    objectName: "onboardStageLabel"
                                    Layout.fillWidth: true
                                    text: qsTr("Estágio ") + (stageRow.index + 1)
                                    font.pixelSize: 12
                                    font.capitalization: Font.AllUppercase
                                    color: Theme.foregroundMedium
                                }

                                GHubRadioButton {
                                    objectName: "onboardDefaultRadio"
                                    text: qsTr("padrão")
                                    checked: onboardEditor.editDefault === stageRow.index
                                    onClicked: onboardEditor.editDefault = stageRow.index
                                }
                            }

                            GHubSpinBox {
                                Layout.fillWidth: true
                                objectName: "onboardDpiField"
                                from: 100
                                to: 25500
                                stepSize: 50
                                editable: true
                                value: onboardEditor.editStages[stageRow.index]
                                onValueModified: {
                                    const next = onboardEditor.editStages.slice();
                                    next[stageRow.index] = value;
                                    onboardEditor.editStages = next;
                                }
                            }
                        }
                    }

                    SectionHeader { text: qsTr("Estágio do DPI shift") }

                    GHubComboBox {
                        objectName: "onboardShiftBox"
                        Layout.fillWidth: true
                        model: [qsTr("Nenhum")].concat(onboardEditor.editStages.map((dpi, i) => qsTr("Estágio ") + (i + 1)))
                        currentIndex: Math.min(onboardEditor.editShift + 1, onboardEditor.editStages.length)
                        onActivated: index => onboardEditor.editShift = index - 1
                    }

                    SectionHeader { text: qsTr("LEDs") }

                    OnboardLedEditor {
                        id: primaryLedEditor
                        title: qsTr("Principal")
                        idPrefix: "onboardPrimaryLed"
                        value: onboardEditor.editPrimaryLed
                        onEditedChanged: onboardEditor.editPrimaryLed = edited
                    }

                    OnboardLedEditor {
                        id: logoLedEditor
                        title: qsTr("Logotipo")
                        idPrefix: "onboardLogoLed"
                        value: onboardEditor.editLogoLed
                        onEditedChanged: onboardEditor.editLogoLed = edited
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        GHubButton {
                            objectName: "onboardAddStageButton"
                            text: qsTr("+ estágio")
                            enabled: onboardEditor.editStages.length < 5
                            onClicked: {
                                const next = onboardEditor.editStages.slice();
                                next.push(next[next.length - 1]);
                                onboardEditor.editStages = next;
                            }
                        }
                        GHubButton {
                            objectName: "onboardRemoveStageButton"
                            text: qsTr("− estágio")
                            enabled: onboardEditor.editStages.length > 1
                            onClicked: {
                                const next = onboardEditor.editStages.slice();
                                next.pop();
                                onboardEditor.editStages = next;
                                onboardEditor.editDefault = Math.min(onboardEditor.editDefault, next.length - 1);
                                onboardEditor.editShift = Math.min(onboardEditor.editShift, next.length - 1);
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true

                        GHubButton {
                            objectName: "onboardWriteButton"
                            text: qsTr("Gravar no mouse")
                            enabled: !window.g703OnboardController.busy
                            onClicked: window.g703OnboardController.writeToMouse(
                                onboardEditor.editRate, onboardEditor.editStages, onboardEditor.editDefault,
                                onboardEditor.editShift, onboardEditor.editPrimaryLed, onboardEditor.editLogoLed)
                        }
                        GHubButton {
                            objectName: "onboardRestoreButton"
                            text: qsTr("Restaurar cópia")
                            enabled: !window.g703OnboardController.busy && window.g703OnboardController.backupExists
                            onClicked: window.g703OnboardController.restoreBackup()
                        }
                    }
                }

                BodyLabel {
                    objectName: "onboardStatusLabel"
                    text: window.g703OnboardController.status
                }
            }

            TabPage {
                SectionHeader {
                    text: qsTr("Leitura (ratbagd)")
                    help: qsTr("Ler a configuração ativa o serviço ratbagd (root). Enquanto ele estiver ativo (até ~20 min), a bateria do G733 fica suspensa.\n\nAtenção: esta leitura ativa o ratbagd, que pode bloquear a memória do mouse (aba Sensibilidade) e a bateria do G733 por cerca de 20 minutos.")
                    helpObjectName: "pointerNoticeLabel"
                }

                // Exceção de segurança: ativar o ratbagd tem efeito fora do app, então o aviso curto fica visível.
                BodyLabel {
                    objectName: "ratbagBlockWarningLabel"
                    font.bold: true
                    text: qsTr("Atenção: ativa o ratbagd e suspende a bateria do G733 por cerca de 20 minutos.")
                }
                // A leitura só começa por clique; o QML apenas encaminha ao ViewModel.
                GHubButton {
                    objectName: "pointerReadButton"
                    text: qsTr("Ler configuração do mouse")
                    enabled: !window.pointerViewModel.reading
                    onClicked: window.pointerViewModel.read()
                }

                BodyLabel {
                    objectName: "pointerStatusLabel"
                    text: window.pointerViewModel.statusText
                }

                BodyLabel {
                    objectName: "pointerDeviceLabel"
                    visible: window.pointerViewModel.deviceName !== ""
                    text: window.pointerViewModel.deviceName + " · " + window.pointerViewModel.connectionText
                }

                Repeater {
                    model: window.pointerViewModel.warnings

                    delegate: BodyLabel {
                        required property var modelData
                        objectName: "pointerWarningLabel"
                        font.bold: true
                        text: modelData
                    }
                }

                Repeater {
                    model: window.pointerViewModel.profiles

                    delegate: ColumnLayout {
                        id: profileColumn
                        required property var modelData
                        Layout.fillWidth: true

                        BodyLabel {
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

                            delegate: BodyLabel {
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

                            delegate: BodyLabel {
                                required property var modelData
                                objectName: "pointerButtonLabel"
                                text: qsTr("Botão ") + modelData.index + ": " + modelData.text
                            }
                        }

                        Repeater {
                            model: profileColumn.modelData.leds

                            delegate: BodyLabel {
                                required property var modelData
                                objectName: "pointerLedLabel"
                                text: qsTr("LED ") + modelData.index + ": " + modelData.mode + " · "
                                    + modelData.color + " · " + qsTr("brilho ") + modelData.brightness
                            }
                        }
                    }
                }
            }

            infoContent: InfoPage {
                anchors.fill: parent
                deviceName: "G703"
                deviceKind: "mouse"
                identityKey: "g703"
                statusKind: "pointer"
                statusViewModel: window.pointerStatusViewModel
                identityViewModel: window.deviceIdentityViewModel
                inventoryDevices: window.deviceInventoryViewModel.devices
                batteryDevices: window.batteryViewModel.devices
                batterySummary: window.batteryTextOf("G703")
                batteryPercent: window.batteryPercentOf("G703")
            }
        }


        // 3 G733
        DevicePage {
            deviceName: "G733"
            deviceKind: "headset"
            batteryText: window.batteryTextOf("G733")
            batteryPercent: window.batteryPercentOf("G733")
            tabs: [{key: "headset", text: qsTr("Headset"), icon: "audio-headset"},
                   {key: "equalizer", text: qsTr("Equalizador"), icon: "view-media-equalizer"},
                   {key: "microphone", text: qsTr("Microfone"), icon: "audio-input-microphone"},
                   {key: "info", text: qsTr("Informações"), icon: "applications-system"}]
            currentTab: window.g733Tab
            onTabSelected: key => window.g733Tab = key

            TabPage {
                // Grava só pelo botão "Aplicar"; mover o controle não grava nada.
                SectionHeader {
                    text: qsTr("Sidetone")
                    help: qsTr("Ajustes enviados pelo HeadsetControl. O headset não informa o valor atual. O sidetone fica gravado no headset e só é ouvido enquanto algum programa usa o microfone; as luzes voltam ao padrão ao desligar o headset. “Ligar luzes” aplica o efeito respiração do HeadsetControl.")
                    helpObjectName: "headsetNoticeLabel"
                }

                GHubSlider {
                    id: headsetSidetoneSlider
                    objectName: "headsetSidetoneSlider"
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    stepSize: 1
                    showValue: true
                    showExtremes: true
                    Accessible.name: qsTr("Sidetone")
                }

                // Exceção de segurança: esta ação grava no headset, então a linha curta fica sempre visível.
                BodyLabel {
                    objectName: "headsetSidetoneWarningLabel"
                    font.bold: true
                    text: qsTr("O sidetone fica gravado no headset.")
                }

                GHubButton {
                    objectName: "headsetSidetoneApplyButton"
                    text: qsTr("Aplicar")
                    enabled: !window.headsetSettingsController.busy
                    onClicked: window.headsetSettingsController.setSidetone(Math.round(headsetSidetoneSlider.value))
                }

                // O headset não informa se as luzes estão ligadas: por honestidade são dois botões, não um interruptor.
                SectionHeader { text: qsTr("Luzes") }

                GHubButton {
                    objectName: "headsetLightsOnButton"
                    text: qsTr("Ligar luzes")
                    enabled: !window.headsetSettingsController.busy
                    onClicked: window.headsetSettingsController.setLights(true)
                }

                GHubButton {
                    objectName: "headsetLightsOffButton"
                    text: qsTr("Desligar luzes")
                    enabled: !window.headsetSettingsController.busy
                    onClicked: window.headsetSettingsController.setLights(false)
                }

                BodyLabel {
                    objectName: "headsetAppliedLabel"
                    readonly property int sidetone: window.headsetSettingsController.lastSidetone
                    readonly property string lights: window.headsetSettingsController.lightsState
                    text: qsTr("Aplicado nesta sessão: sidetone %1; luzes %2")
                        .arg(sidetone < 0 ? qsTr("nenhum") : sidetone)
                        .arg(lights === "on" ? qsTr("ligadas") : lights === "off" ? qsTr("desligadas") : qsTr("nenhum ajuste"))
                }

                BodyLabel {
                    objectName: "headsetStatusLabel"
                    text: window.headsetSettingsController.status
                }
            }

            TabPage {
                // As 10 bandas ficam na área principal (mainView); o painel guarda o interruptor, o "?" e o estado.
                SwitchRow {
                    text: qsTr("Equalizador")
                    help: qsTr("Processado no PC pelo PipeWire, não no headset. Vale só com o app aberto. Enquanto ligado, a saída padrão passa a ser “G733 + EQ”.")
                    helpObjectName: "eqNoticeLabel"
                    switchObjectName: "eqEnableSwitch"
                    checked: window.hostEqualizerController.enabled
                    switchEnabled: !window.hostEqualizerController.busy
                    onToggleRequested: wanted => wanted ? window.hostEqualizerController.enable()
                                                        : window.hostEqualizerController.disable()
                }

                // Exceção de segurança: ligar troca a saída padrão de áudio do sistema.
                BodyLabel {
                    objectName: "eqOutputWarningLabel"
                    font.bold: true
                    text: qsTr("Troca a saída padrão para “G733 + EQ”.")
                }

                BodyLabel {
                    objectName: "eqStatusLabel"
                    text: window.hostEqualizerController.status
                }
            }

            TabPage {
                SwitchRow {
                    text: qsTr("Redução de ruído")
                    help: qsTr("Processado no PC pelo RNNoise, não no headset; não é o Blue VO!CE. Vale só com o app aberto. Enquanto ligado, a entrada padrão passa a ser “G733 + Redução de ruído”.")
                    helpObjectName: "noiseNoticeLabel"
                    switchObjectName: "noiseEnableSwitch"
                    checked: window.noiseSuppressionController.enabled
                    switchEnabled: !window.noiseSuppressionController.busy
                    onToggleRequested: wanted => wanted ? window.noiseSuppressionController.enable()
                                                        : window.noiseSuppressionController.disable()
                }

                // Exceção de segurança: ligar troca a entrada padrão de áudio do sistema.
                BodyLabel {
                    objectName: "noiseInputWarningLabel"
                    font.bold: true
                    text: qsTr("Troca a entrada padrão para “G733 + Redução de ruído”.")
                }

                // O QML só mostra o limiar e encaminha o movimento do usuário ao controlador.
                SectionTitle {
                    objectName: "noiseThresholdLabel"
                    text: qsTr("Sensibilidade: %1 %").arg(window.noiseSuppressionController.threshold)
                }

                GHubSlider {
                    id: noiseSlider
                    objectName: "noiseThresholdSlider"
                    Layout.fillWidth: true
                    orientation: Qt.Horizontal
                    from: window.noiseSuppressionController.minimumThreshold
                    to: window.noiseSuppressionController.maximumThreshold
                    stepSize: 1
                    snapMode: Slider.SnapAlways
                    showExtremes: true
                    Accessible.name: qsTr("Sensibilidade da redução de ruído")
                    // O valor segue o controlador; o envio acontece só quando o usuário move.
                    onMoved: window.noiseSuppressionController.setThreshold(Math.round(value))

                    Binding {
                        target: noiseSlider
                        property: "value"
                        value: window.noiseSuppressionController.threshold
                        when: !noiseSlider.pressed
                        restoreMode: Binding.RestoreNone
                    }
                }

                BodyLabel {
                    objectName: "noiseStatusLabel"
                    text: window.noiseSuppressionController.status
                }
            }

            infoContent: InfoPage {
                anchors.fill: parent
                deviceName: "G733"
                deviceKind: "headset"
                identityKey: "g733"
                statusKind: "headset"
                statusViewModel: window.headsetStatusViewModel
                identityViewModel: window.deviceIdentityViewModel
                inventoryDevices: window.deviceInventoryViewModel.devices
                batteryDevices: window.batteryViewModel.devices
                batterySummary: window.batteryTextOf("G733")
                batteryPercent: window.batteryPercentOf("G733")
            }

            // O equalizador gráfico ocupa a área principal no lugar do desenho do headset.
            mainViewTab: "equalizer"
            mainView: EqualizerView {
                anchors.fill: parent
                bandFrequencies: window.hostEqualizerController.bandFrequencies
                gains: window.hostEqualizerController.gains
                minimumGain: window.hostEqualizerController.minimumGain
                maximumGain: window.hostEqualizerController.maximumGain
                busy: window.hostEqualizerController.busy
                onBandMoved: (band, gain) => window.hostEqualizerController.setBand(band, gain)
                onResetRequested: window.hostEqualizerController.flat()
            }
        }


        // 4 Brio 100
        DevicePage {
            deviceName: "Brio 100"
            deviceKind: "camera"
            batteryText: window.batteryTextOf("Brio 100")
            batteryPercent: window.batteryPercentOf("Brio 100")
            tabs: [{key: "image", text: qsTr("Imagem"), icon: "camera-photo"},
                   {key: "modes", text: qsTr("Modos de vídeo"), icon: "camera-video"},
                   {key: "info", text: qsTr("Informações"), icon: "applications-system"}]
            currentTab: window.brioTab
            onTabSelected: key => window.brioTab = key

            TabPage {
                SectionHeader { text: qsTr("Câmera") }
                BodyLabel {
                    objectName: "cameraStatusLabel"
                    text: window.cameraViewModel.status
                }

                // A pré-visualização só começa por clique (R1); o QML apenas encaminha ao ViewModel.
                GHubButton {
                    objectName: "cameraPreviewToggle"
                    text: window.cameraPreviewViewModel.active ? qsTr("Parar pré-visualização")
                                                               : qsTr("Iniciar pré-visualização")
                    onClicked: window.cameraPreviewViewModel.toggle()
                }

                BodyLabel {
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
                    // Ocupa a largura do painel; a altura segue a proporção do modo.
                    Layout.fillWidth: true
                    Layout.preferredHeight: width * aspect

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


                // Uma linha por controle. O QML só encaminha a ação do usuário ao editor; validação e
                // gravação são do módulo camera. O modelo é reconstruído a cada releitura da câmera.
                Repeater {
                    model: window.cameraViewModel.controls

                    // Em cada controle: o rótulo em cima; embaixo o controle (largura toda) e o botão "Padrão".
                    delegate: ColumnLayout {
                        id: controlRow
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 4

                        BodyLabel {
                            objectName: "cameraControlLabel"
                            text: controlRow.modelData.name + ": " + controlRow.modelData.summary
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            // Só o slider estica; interruptor e dropdown ficam no tamanho natural e um espaço empurra
                            // o botão "Padrão" para a direita.
                            Loader {
                                Layout.fillWidth: controlRow.modelData.kind === "integer"
                                sourceComponent: controlRow.modelData.kind === "integer" ? sliderComponent
                                    : controlRow.modelData.kind === "boolean" ? switchComponent
                                    : controlRow.modelData.kind === "menu" ? comboComponent
                                    : null
                            }

                            Item {
                                Layout.fillWidth: true
                                visible: controlRow.modelData.kind !== "integer"
                            }

                            GHubButton {
                                objectName: "cameraControlDefault"
                                Layout.fillWidth: false
                                text: qsTr("Padrão")
                                enabled: controlRow.modelData.editable === true
                                onClicked: window.cameraControlEditor.restoreDefault(controlRow.modelData.id)
                            }
                        }

                        Component {
                            id: sliderComponent
                            GHubSlider {
                                objectName: "cameraControlSlider"
                                showValue: true
                                showExtremes: true
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
                            GHubSwitch {
                                objectName: "cameraControlSwitch"
                                checked: (controlRow.modelData.value ?? 0) !== 0
                                enabled: controlRow.modelData.editable === true
                                // O estado exibido segue o modelo; o clique só pede o valor oposto ao editor.
                                onToggleRequested: wanted => window.cameraControlEditor.setValue(controlRow.modelData.id,
                                                                                                 wanted ? 1 : 0)
                            }
                        }

                        Component {
                            id: comboComponent
                            GHubComboBox {
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

                BodyLabel {
                    objectName: "cameraControlEditorStatus"
                    text: window.cameraControlEditor.status
                }
            }

            TabPage {
                Repeater {
                    model: window.cameraViewModel.modes

                    delegate: BodyLabel {
                        required property var modelData
                        objectName: "cameraModeLabel"
                        text: modelData.text
                    }
                }
            }

            infoContent: InfoPage {
                anchors.fill: parent
                deviceName: "Brio 100"
                deviceKind: "camera"
                identityViewModel: window.deviceIdentityViewModel
                inventoryDevices: window.deviceInventoryViewModel.devices
                batteryDevices: window.batteryViewModel.devices
                batterySummary: window.batteryTextOf("Brio 100")
                batteryPercent: window.batteryPercentOf("Brio 100")
            }
        }


        // 5 Perfis
        PanelPage {
            title: qsTr("Perfis")

            TabPage {
                BodyLabel {
                    objectName: "profilesNoticeLabel"
                    text: qsTr("Aplicar grava os controles da Brio 100 e muda a curva do EQ; o perfil não liga nem desliga o EQ.")
                }

                BodyLabel {
                    objectName: "profilesLoadFailedLabel"
                    visible: window.profilesController.loadFailed
                    color: Theme.error
                    text: qsTr("Arquivo de perfis ilegível: nada será gravado até corrigi-lo.")
                }

                // O estado exibido segue o controlador (a gravação pode falhar); o clique só pede o estado oposto.
                SwitchRow {
                    text: qsTr("Trocar de perfil pelo aplicativo em foco")
                    switchObjectName: "profilesAutoSwitch"
                    checked: window.profilesController.automaticSwitching
                    switchEnabled: !window.profilesController.busy
                    onToggleRequested: wanted => window.profilesController.setAutomaticSwitching(wanted)
                }

                BodyLabel {
                    objectName: "profilesAutoSwitchNotice"
                    text: qsTr("Com a troca ligada, o app carrega um pequeno script no KWin que informa só o nome do aplicativo em foco; ao trocar de aplicativo, o perfil associado é aplicado (grava na Brio 100 e muda a curva do EQ).")
                }

                BodyLabel {
                    objectName: "profilesSwitchingStatus"
                    text: qsTr("Troca automática: %1").arg(window.profilesController.switchingStatus)
                }

                BodyLabel {
                    objectName: "profilesLastApplication"
                    text: qsTr("Último aplicativo em foco: %1").arg(
                              window.profilesController.lastApplication !== ""
                              ? window.profilesController.lastApplication : qsTr("nenhum"))
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    GHubTextField {
                        id: profileNameField
                        objectName: "profileNameField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Nome do perfil")
                        Accessible.name: qsTr("Nome do perfil")
                    }

                    GHubButton {
                        objectName: "profileSaveButton"
                        Layout.fillWidth: false
                        text: qsTr("Salvar estado atual")
                        enabled: !window.profilesController.busy
                        onClicked: {
                            if (window.profilesController.exists(profileNameField.text))
                                profileReplaceDialog.open()
                            else
                                window.profilesController.saveCurrent(profileNameField.text, false)
                        }
                    }
                }

                Repeater {
                    model: window.profilesController.profileNames

                    // Por perfil: o nome em cima e, embaixo, os botões em linha (quebram se faltar espaço).
                    delegate: ColumnLayout {
                        id: profileRow
                        required property string modelData
                        Layout.fillWidth: true
                        spacing: 6

                        BodyLabel {
                            objectName: "profileNameLabel"
                            font.bold: true
                            elide: Text.ElideRight
                            wrapMode: Text.NoWrap
                            text: profileRow.modelData
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 8

                            GHubButton {
                                objectName: "profileApplyButton"
                                text: qsTr("Aplicar")
                                enabled: !window.profilesController.busy
                                onClicked: window.profilesController.apply(profileRow.modelData)
                            }

                            GHubButton {
                                objectName: "profileUpdateButton"
                                text: qsTr("Atualizar")
                                enabled: !window.profilesController.busy
                                onClicked: {
                                    window.profilePendingUpdate = profileRow.modelData
                                    profileUpdateDialog.open()
                                }
                            }

                            GHubButton {
                                objectName: "profileRemoveButton"
                                text: qsTr("Remover")
                                enabled: !window.profilesController.busy
                                onClicked: {
                                    window.profilePendingRemoval = profileRow.modelData
                                    profileRemoveDialog.open()
                                }
                            }

                            GHubButton {
                                objectName: "profileAssignButton"
                                text: qsTr("Associar o último aplicativo")
                                enabled: !window.profilesController.busy
                                         && window.profilesController.lastApplication !== ""
                                onClicked: window.profilesController.assignLastApplication(profileRow.modelData)
                            }
                        }

                        Repeater {
                            model: window.profilesController.profileApplications[profileRow.modelData] || []

                            delegate: RowLayout {
                                id: applicationRow
                                required property string modelData
                                Layout.fillWidth: true
                                spacing: 12

                                BodyLabel {
                                    objectName: "profileApplicationLabel"
                                    Layout.leftMargin: 24
                                    elide: Text.ElideRight
                                    wrapMode: Text.NoWrap
                                    text: applicationRow.modelData
                                }

                                GHubButton {
                                    objectName: "profileUnassignButton"
                                    Layout.fillWidth: false
                                    text: qsTr("Remover")
                                    enabled: !window.profilesController.busy
                                    onClicked: window.profilesController.unassignApplication(applicationRow.modelData)
                                }
                            }
                        }
                    }
                }

                BodyLabel {
                    objectName: "profilesStatusLabel"
                    text: window.profilesController.status
                }

                Repeater {
                    model: window.profilesController.report

                    delegate: BodyLabel {
                        required property string modelData
                        objectName: "profileReportLine"
                        text: modelData
                    }
                }

            }
        }


        // 6 Configurações
        PanelPage {
            title: qsTr("Configurações")

            TabPage {
                objectName: "settingsScrollView"
                SectionTitle { text: qsTr("Inicialização automática") }
                BodyLabel {
                    objectName: "autostartNoticeLabel"
                    text: qsTr("Liga o aplicativo junto com a sessão (arquivo XDG em ~/.config/autostart). Nada é gravado até você ligar esta opção; desligar remove o arquivo.")
                }

                // O estado vem do controlador; ligar/desligar só é enviado quando o valor diverge (evita
                // laço entre a alternância e a releitura do arquivo). INC-013 R8.
                GHubCheckBox {
                    id: autostartSwitch
                    objectName: "autostartSwitch"
                    text: qsTr("Iniciar com a sessão")
                    checked: window.desktopController.autostartEnabled
                    onToggled: {
                        if (checked !== window.desktopController.autostartEnabled)
                            window.desktopController.setAutostart(checked)
                    }
                }

                BodyLabel {
                    objectName: "autostartStatusLabel"
                    text: window.desktopController.status
                }
                SectionTitle { text: qsTr("Diagnóstico") }
                BodyLabel {
                    objectName: "diagnosticsNoticeLabel"
                    text: qsTr("Últimas mensagens do aplicativo e saúde dos componentes. Ficam só na memória: o relatório só é gravado quando você clica em exportar.")
                }

                // "Atualizar" e "Exportar" só agem por clique; nada é lido nem gravado automaticamente (R14).
                GHubButton {
                    objectName: "diagnosticsRefreshButton"
                    text: qsTr("Atualizar")
                    onClicked: window.diagnosticsViewModel.refresh()
                }

                Repeater {
                    model: window.diagnosticsViewModel.backends

                    delegate: Label {
                        required property var modelData
                        objectName: "diagnosticsBackendLabel"
                        color: Theme.foreground
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: modelData.name + ": "
                            + (modelData.available ? qsTr("disponível") : qsTr("indisponível"))
                            + " · " + modelData.detail
                    }
                }

                GHubButton {
                    objectName: "diagnosticsExportButton"
                    text: qsTr("Exportar relatório")
                    onClicked: window.diagnosticsViewModel.exportReport()
                }

                BodyLabel {
                    objectName: "diagnosticsStatusLabel"
                    text: window.diagnosticsViewModel.status
                }

                Repeater {
                    model: window.diagnosticsViewModel.entries

                    delegate: Label {
                        required property var modelData
                        objectName: "diagnosticsEntryLabel"
                        color: Theme.foreground
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: modelData.severity + " · " + modelData.category + ": " + modelData.message
                    }
                }

            }
        }
    }

    Dialog {
        id: profileReplaceDialog
        objectName: "profileReplaceDialog"
        anchors.centerIn: Overlay.overlay
        width: 400
        modal: true
        title: qsTr("Substituir?")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: window.profilesController.saveCurrent(profileNameField.text, true)

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Já existe um perfil com esse nome. Substituir pelo estado atual?")
        }
    }

    Dialog {
        id: profileUpdateDialog
        objectName: "profileUpdateDialog"
        anchors.centerIn: Overlay.overlay
        width: 400
        modal: true
        title: qsTr("Atualizar perfil?")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: window.profilesController.saveCurrent(window.profilePendingUpdate, true)

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Salvar o estado atual da câmera e do EQ no perfil “%1”?").arg(window.profilePendingUpdate)
        }
    }

    Dialog {
        id: profileRemoveDialog
        objectName: "profileRemoveDialog"
        anchors.centerIn: Overlay.overlay
        width: 400
        modal: true
        title: qsTr("Remover perfil?")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: window.profilesController.remove(window.profilePendingRemoval)

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Remover o perfil “%1”?").arg(window.profilePendingRemoval)
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
