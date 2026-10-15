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

    function isDetected(entry) {
        return entry !== null && entry.summary !== qsTr("não detectado")
    }

    width: 1100
    height: 720
    minimumWidth: 900
    minimumHeight: 600
    // `--autostart` inicia escondido na bandeja; sem bandeja, o controlador não esconde (INC-013 R9).
    visible: !desktopController.startHidden
    title: qsTr("Hubless")

    Material.theme: Material.Dark
    Material.accent: Theme.primary
    Material.background: Theme.background
    Material.primary: Theme.surface
    Material.foreground: Theme.foreground

    // Encerra o stream da câmera ao fechar a janela. Com bandeja (hideOnClose) e fora de um encerramento
    // pedido, recusa o fechamento e esconde a janela: o processo continua e a bandeja reabre (INC-013 R4/R5).
    onClosing: function(close) {
        window.cameraPreviewViewModel.stop()
        if (window.desktopController.hideOnClose && !window.desktopController.quitting) {
            close.accepted = false
            window.hide()
        }
    }

    // Título de seção em maiúsculas pequenas.
    component SectionTitle: Label {
        Layout.topMargin: 6
        font.pixelSize: 12
        font.bold: true
        font.letterSpacing: 1.2
        font.capitalization: Font.AllUppercase
        color: Theme.primary
    }

    // Página de uma aba: rolagem própria e coluna com margens.
    component TabPage: ScrollView {
        id: tabPage
        default property alias content: tabColumn.data
        contentWidth: availableWidth

        ColumnLayout {
            id: tabColumn
            x: 24
            y: 16
            width: tabPage.availableWidth - 48
            spacing: 12
        }
    }

    // Aba Informações de um dispositivo: conexão e bateria só dele (INC-014 R11).
    component InfoTab: TabPage {
        id: infoTab
        property string deviceName: ""
        property var inventoryDevices: []
        property var batteryDevices: []
        // Chave do dispositivo no DeviceIdentityViewModel ("g915", "g703", "g733"); vazia = sem leitura HID++.
        property string identityKey: ""
        readonly property var identity: identityKey !== "" && window.deviceIdentityViewModel.identities[identityKey]
                                        ? window.deviceIdentityViewModel.identities[identityKey]
                                        : ({state: "idle"})
        readonly property bool identityOk: identity.state === "ok"
        // Estado de UI apenas: lista de IDs das features recolhida por padrão.
        property bool featureIdsExpanded: false

        SectionTitle { text: qsTr("Conexão") }

        Repeater {
            model: infoTab.inventoryDevices.filter(d => d.name === infoTab.deviceName)

            delegate: Label {
                required property var modelData
                objectName: "inventoryDeviceLabel"
                text: modelData.name + ": " + modelData.summary
            }
        }

        SectionTitle { text: qsTr("Bateria") }

        Repeater {
            model: infoTab.batteryDevices.filter(d => d.name === infoTab.deviceName)

            delegate: Label {
                required property var modelData
                objectName: "batteryDeviceLabel"
                text: modelData.name + ": " + modelData.summary
            }
        }

        // Identidade por HID++ (INC-015): só leitura, só por clique; o QML apenas encaminha ao ViewModel.
        // A Brio 100 não fala HID++: sem chave, a seção nem é criada.
        Loader {
            Layout.fillWidth: true
            active: infoTab.identityKey !== ""
            visible: active
            sourceComponent: identitySection
        }

        Component {
            id: identitySection

            ColumnLayout {
                spacing: 12

                SectionTitle { text: qsTr("Identidade (HID++)") }

                Button {
                    objectName: "readIdentityButton"
                    enabled: infoTab.identity.state !== "reading"
                    text: infoTab.identity.state === "reading" ? qsTr("Lendo…") : qsTr("Ler identidade")
                    onClicked: window.deviceIdentityViewModel.read(infoTab.identityKey)
                }

                Label {
                    objectName: "identityErrorLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    visible: infoTab.identity.state === "error"
                    color: Theme.error
                    text: infoTab.identity.error || ""
                }

                Label {
                    objectName: "identityNameLabel"
                    visible: infoTab.identityOk
                    text: qsTr("Nome: %1").arg(infoTab.identity.name)
                }

                Label {
                    objectName: "identityTypeLabel"
                    visible: infoTab.identityOk
                    text: qsTr("Tipo: %1").arg(infoTab.identity.type)
                }

                Label {
                    objectName: "identityFirmwareLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    visible: infoTab.identityOk
                    text: qsTr("Firmware: %1").arg(infoTab.identityOk && infoTab.identity.firmwares.length > 0
                                                   ? infoTab.identity.firmwares.join("\n") : qsTr("não informado"))
                }

                Label {
                    objectName: "identityFeatureCountLabel"
                    visible: infoTab.identityOk
                    text: qsTr("Features: %1").arg(infoTab.identity.featureCount)
                }

                Label {
                    objectName: "identityTruncationLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    visible: infoTab.identityOk && infoTab.identity.truncationNote !== ""
                    color: Theme.foregroundMedium
                    text: infoTab.identity.truncationNote || ""
                }

                Button {
                    objectName: "toggleFeatureIdsButton"
                    flat: true
                    visible: infoTab.identityOk && infoTab.identity.featureCount > 0
                    text: infoTab.featureIdsExpanded ? qsTr("Ocultar IDs das features") : qsTr("Mostrar IDs das features")
                    onClicked: infoTab.featureIdsExpanded = !infoTab.featureIdsExpanded
                }

                Label {
                    objectName: "identityFeatureIdsLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    visible: infoTab.identityOk && infoTab.identity.featureCount > 0 && infoTab.featureIdsExpanded
                    text: infoTab.identityOk ? infoTab.identity.featureIds.join(", ") : ""
                }
            }
        }
    }

    // Página de dispositivo: coluna lateral com o nome e as abas; as páginas das abas vão para a pilha à direita.
    component DevicePage: RowLayout {
        id: devicePage
        property string deviceName: ""
        // [{key, text}]
        property var tabs: []
        property string currentTab: ""
        signal tabSelected(string key)
        default property alias pages: tabStack.data
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: Theme.surface

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 4

                Label {
                    Layout.bottomMargin: 12
                    text: devicePage.deviceName
                    font.pixelSize: 22
                    font.bold: true
                }

                Repeater {
                    model: devicePage.tabs

                    delegate: SideTab {
                        required property var modelData
                        tabKey: modelData.key
                        text: modelData.text
                        selected: devicePage.currentTab === modelData.key
                        onClicked: devicePage.tabSelected(modelData.key)
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        StackLayout {
            id: tabStack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: Math.max(0, devicePage.tabs.findIndex(t => t.key === devicePage.currentTab))
        }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8

            ToolButton {
                objectName: "backButton"
                visible: window.currentPage !== window.homePage
                text: qsTr("‹ Voltar")
                onClicked: window.openPage(window.homePage)
            }

            Label {
                text: qsTr("Hubless")
                font.pixelSize: 20
                font.bold: true
                Layout.leftMargin: 8
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                objectName: "profilesPageButton"
                text: qsTr("Perfis")
                highlighted: window.currentPage === window.profilesPage
                onClicked: window.openPage(window.profilesPage)
            }

            ToolButton {
                objectName: "settingsPageButton"
                text: qsTr("Configurações")
                highlighted: window.currentPage === window.settingsPage
                onClicked: window.openPage(window.settingsPage)
            }
        }
    }

    StackLayout {
        id: pageStack
        objectName: "pageStack"
        anchors.fill: parent
        currentIndex: window.currentPage

        // 0 Início: um cartão por dispositivo do catálogo.
        ScrollView {
            id: homeScroll
            contentWidth: availableWidth

            Flow {
                x: 32
                y: 32
                width: homeScroll.availableWidth - 64
                spacing: 20

                Repeater {
                    model: [
                        {name: "G915", icon: "input-keyboard", page: 1},
                        {name: "G703", icon: "input-mouse", page: 2},
                        {name: "G733", icon: "audio-headset", page: 3},
                        {name: "Brio 100", icon: "camera-web", page: 4}
                    ]

                    delegate: DeviceCard {
                        id: card
                        required property var modelData
                        readonly property var inventoryEntry: window.entryFor(window.deviceInventoryViewModel.devices, modelData.name)
                        readonly property var batteryEntry: window.entryFor(window.batteryViewModel.devices, modelData.name)

                        deviceName: modelData.name
                        iconName: modelData.icon
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
            tabs: [{key: "lighting", text: qsTr("Iluminação")}, {key: "info", text: qsTr("Informações")}]
            currentTab: window.g915Tab
            onTabSelected: key => window.g915Tab = key

            TabPage {
            SectionTitle { text: qsTr("Iluminação") }
                Label {
                    font.bold: true
                    text: qsTr("Teclado (G915)")
                }

                Label {
                    objectName: "lightingNoticeLabel"
                    Layout.maximumWidth: window.width - 340
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
                    Layout.maximumWidth: window.width - 340
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
                    Layout.maximumWidth: window.width - 340
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

            InfoTab {
                deviceName: "G915"
                identityKey: "g915"
                inventoryDevices: window.deviceInventoryViewModel.devices
                batteryDevices: window.batteryViewModel.devices
            }
        }


        // 2 G703
        DevicePage {
            deviceName: "G703"
            tabs: [{key: "onboard", text: qsTr("Sensibilidade")}, {key: "ratbag", text: qsTr("Leitura (ratbagd)")},
                   {key: "info", text: qsTr("Informações")}]
            currentTab: window.g703Tab
            onTabSelected: key => window.g703Tab = key

            TabPage {
            SectionTitle { text: qsTr("Sensibilidade (memória do mouse)") }
                // Memória do mouse por HID++ (INC-011): o QML só repassa os valores editados ao controlador,
                // que valida e grava; nada é lido nem gravado na abertura.
                Label {
                    objectName: "onboardNoticeLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    text: qsTr("Grava na memória do mouse (perfil ativo). Antes da primeira gravação é salva uma cópia do perfil. Feche o Piper e não use o ratbagd ao mesmo tempo: um Commit dele sobrescreve estes valores.")
                }

                Button {
                    objectName: "onboardReadButton"
                    text: qsTr("Ler do mouse")
                    enabled: !window.g703OnboardController.busy
                    onClicked: window.g703OnboardController.readFromMouse()
                }

                ColumnLayout {
                    id: onboardEditor
                    objectName: "onboardEditor"
                    visible: window.g703OnboardController.loaded

                    // Estado de edição (UI apenas): copiado do controlador quando ele termina uma operação.
                    property var editStages: []
                    property int editDefault: 0
                    property int editRate: 1000

                    function syncFromController() {
                        if (window.g703OnboardController.busy) {
                            return;
                        }
                        editStages = window.g703OnboardController.dpiStages.slice();
                        editDefault = window.g703OnboardController.defaultStage;
                        editRate = window.g703OnboardController.reportRateHz;
                    }

                    Component.onCompleted: syncFromController()

                    Connections {
                        target: window.g703OnboardController
                        function onBusyChanged() { onboardEditor.syncFromController() }
                        function onReportRateHzChanged() { onboardEditor.syncFromController() }
                        function onDpiStagesChanged() { onboardEditor.syncFromController() }
                        function onDefaultStageChanged() { onboardEditor.syncFromController() }
                    }

                    RowLayout {
                        Label { text: qsTr("Taxa de relatório (Hz)") }
                        ComboBox {
                            objectName: "onboardRateBox"
                            model: [125, 250, 500, 1000]
                            currentIndex: Math.max(0, model.indexOf(onboardEditor.editRate))
                            onActivated: onboardEditor.editRate = model[currentIndex]
                        }
                    }

                    Repeater {
                        model: onboardEditor.editStages.length

                        delegate: RowLayout {
                            id: stageRow
                            required property int index

                            Label { text: qsTr("Estágio ") + (stageRow.index + 1) }
                            SpinBox {
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
                            RadioButton {
                                objectName: "onboardDefaultRadio"
                                text: qsTr("padrão")
                                checked: onboardEditor.editDefault === stageRow.index
                                onClicked: onboardEditor.editDefault = stageRow.index
                            }
                        }
                    }

                    RowLayout {
                        Button {
                            objectName: "onboardAddStageButton"
                            text: qsTr("+ estágio")
                            enabled: onboardEditor.editStages.length < 5
                            onClicked: {
                                const next = onboardEditor.editStages.slice();
                                next.push(next[next.length - 1]);
                                onboardEditor.editStages = next;
                            }
                        }
                        Button {
                            objectName: "onboardRemoveStageButton"
                            text: qsTr("− estágio")
                            enabled: onboardEditor.editStages.length > 1
                            onClicked: {
                                const next = onboardEditor.editStages.slice();
                                next.pop();
                                onboardEditor.editStages = next;
                                onboardEditor.editDefault = Math.min(onboardEditor.editDefault, next.length - 1);
                            }
                        }
                    }

                    RowLayout {
                        Button {
                            objectName: "onboardWriteButton"
                            text: qsTr("Gravar no mouse")
                            enabled: !window.g703OnboardController.busy
                            onClicked: window.g703OnboardController.writeToMouse(
                                onboardEditor.editRate, onboardEditor.editStages, onboardEditor.editDefault)
                        }
                        Button {
                            objectName: "onboardRestoreButton"
                            text: qsTr("Restaurar cópia")
                            enabled: !window.g703OnboardController.busy && window.g703OnboardController.backupExists
                            onClicked: window.g703OnboardController.restoreBackup()
                        }
                    }
                }

                Label {
                    objectName: "onboardStatusLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    text: window.g703OnboardController.status
                }
            }

            TabPage {
            SectionTitle { text: qsTr("Leitura pelo ratbagd") }
                Label {
                    objectName: "pointerNoticeLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    text: qsTr("Ler a configuração ativa o serviço ratbagd (root). Enquanto ele estiver ativo (até ~20 min), a bateria do G733 fica suspensa.")
                }

                Label {
                    objectName: "ratbagBlockWarningLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    font.bold: true
                    text: qsTr("Atenção: esta leitura ativa o ratbagd, que pode bloquear a memória do mouse (aba Sensibilidade) e a bateria do G733 por cerca de 20 minutos.")
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
                        Layout.maximumWidth: window.width - 340
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

            InfoTab {
                deviceName: "G703"
                identityKey: "g703"
                inventoryDevices: window.deviceInventoryViewModel.devices
                batteryDevices: window.batteryViewModel.devices
            }
        }


        // 3 G733
        DevicePage {
            deviceName: "G733"
            tabs: [{key: "headset", text: qsTr("Headset")}, {key: "equalizer", text: qsTr("Equalizador")},
                   {key: "microphone", text: qsTr("Microfone")}, {key: "info", text: qsTr("Informações")}]
            currentTab: window.g733Tab
            onTabSelected: key => window.g733Tab = key

            TabPage {
            SectionTitle { text: qsTr("Headset (G733)") }
                Label {
                    objectName: "headsetNoticeLabel"
                    Layout.maximumWidth: window.width - 340
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

            TabPage {
            SectionTitle { text: qsTr("Equalizador do G733 (no PC)") }
                Label {
                    objectName: "eqNoticeLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    text: qsTr("Processado no PC pelo PipeWire, não no headset. Vale só com o app aberto. Enquanto ligado, a saída padrão passa a ser “G733 + EQ”.")
                }

                // Uma faixa por banda. O QML só mostra a curva e encaminha o movimento do usuário ao controlador.
                RowLayout {
                    spacing: 8

                    Repeater {
                        model: window.hostEqualizerController.bandFrequencies

                        delegate: ColumnLayout {
                            id: eqBand
                            required property int index
                            required property string modelData
                            readonly property int gain: window.hostEqualizerController.gains[index] ?? 0

                            Label {
                                objectName: "eqBandValueLabel"
                                Layout.alignment: Qt.AlignHCenter
                                text: (eqBand.gain > 0 ? "+" : "") + eqBand.gain
                            }

                            Slider {
                                id: eqBandSlider
                                objectName: "eqBandSlider"
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredHeight: 160
                                orientation: Qt.Vertical
                                from: window.hostEqualizerController.minimumGain
                                to: window.hostEqualizerController.maximumGain
                                stepSize: 1
                                snapMode: Slider.SnapAlways
                                Accessible.name: qsTr("Banda %1 Hz").arg(eqBand.modelData)
                                // O valor segue o controlador; o envio acontece só quando o usuário move.
                                onMoved: window.hostEqualizerController.setBand(eqBand.index, Math.round(value))

                                Binding {
                                    target: eqBandSlider
                                    property: "value"
                                    value: eqBand.gain
                                    when: !eqBandSlider.pressed
                                    restoreMode: Binding.RestoreNone
                                }
                            }

                            Label {
                                objectName: "eqBandLabel"
                                Layout.alignment: Qt.AlignHCenter
                                text: eqBand.modelData
                            }
                        }
                    }
                }

                RowLayout {
                    Button {
                        objectName: "eqEnableButton"
                        text: qsTr("Ligar EQ")
                        enabled: !window.hostEqualizerController.busy && !window.hostEqualizerController.enabled
                        onClicked: window.hostEqualizerController.enable()
                    }

                    Button {
                        objectName: "eqDisableButton"
                        text: qsTr("Desligar EQ")
                        enabled: !window.hostEqualizerController.busy && window.hostEqualizerController.enabled
                        onClicked: window.hostEqualizerController.disable()
                    }

                    Button {
                        objectName: "eqFlatButton"
                        text: qsTr("Plano")
                        enabled: !window.hostEqualizerController.busy
                        onClicked: window.hostEqualizerController.flat()
                    }
                }

                Label {
                    objectName: "eqStatusLabel"
                    text: window.hostEqualizerController.status
                }
            }

            TabPage {
            SectionTitle { text: qsTr("Redução de ruído do microfone do G733 (no PC)") }
                Label {
                    objectName: "noiseNoticeLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    text: qsTr("Processado no PC pelo RNNoise, não no headset; não é o Blue VO!CE. Vale só com o app aberto. Enquanto ligado, a entrada padrão passa a ser “G733 + Redução de ruído”.")
                }

                RowLayout {
                    Button {
                        objectName: "noiseEnableButton"
                        text: qsTr("Ligar")
                        enabled: !window.noiseSuppressionController.busy && !window.noiseSuppressionController.enabled
                        onClicked: window.noiseSuppressionController.enable()
                    }

                    Button {
                        objectName: "noiseDisableButton"
                        text: qsTr("Desligar")
                        enabled: !window.noiseSuppressionController.busy && window.noiseSuppressionController.enabled
                        onClicked: window.noiseSuppressionController.disable()
                    }
                }

                // O QML só mostra o limiar e encaminha o movimento do usuário ao controlador.
                RowLayout {
                    Label {
                        objectName: "noiseThresholdLabel"
                        text: qsTr("Sensibilidade: %1 %").arg(window.noiseSuppressionController.threshold)
                    }

                    Slider {
                        id: noiseSlider
                        objectName: "noiseThresholdSlider"
                        Layout.preferredWidth: 280
                        orientation: Qt.Horizontal
                        from: window.noiseSuppressionController.minimumThreshold
                        to: window.noiseSuppressionController.maximumThreshold
                        stepSize: 1
                        snapMode: Slider.SnapAlways
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
                }

                Label {
                    objectName: "noiseStatusLabel"
                    text: window.noiseSuppressionController.status
                }
            }

            InfoTab {
                deviceName: "G733"
                identityKey: "g733"
                inventoryDevices: window.deviceInventoryViewModel.devices
                batteryDevices: window.batteryViewModel.devices
            }
        }


        // 4 Brio 100
        DevicePage {
            deviceName: "Brio 100"
            tabs: [{key: "image", text: qsTr("Imagem")}, {key: "modes", text: qsTr("Modos de vídeo")},
                   {key: "info", text: qsTr("Informações")}]
            currentTab: window.brioTab
            onTabSelected: key => window.brioTab = key

            TabPage {
            SectionTitle { text: qsTr("Câmera") }
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
                    Layout.preferredWidth: Math.min(480, Math.max(1, window.width - 360))
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

            TabPage {
            SectionTitle { text: qsTr("Modos de vídeo") }
                Repeater {
                    model: window.cameraViewModel.modes

                    delegate: Label {
                        required property var modelData
                        objectName: "cameraModeLabel"
                        text: modelData.text
                    }
                }
            }

            InfoTab {
                deviceName: "Brio 100"
                inventoryDevices: window.deviceInventoryViewModel.devices
                batteryDevices: window.batteryViewModel.devices
            }
        }


        // 5 Perfis
        TabPage {
            SectionTitle { text: qsTr("Perfis") }
            Label {
                objectName: "profilesNoticeLabel"
                Layout.maximumWidth: window.width - 340
                wrapMode: Text.WordWrap
                text: qsTr("Aplicar grava os controles da Brio 100 e muda a curva do EQ; o perfil não liga nem desliga o EQ.")
            }

            Label {
                objectName: "profilesLoadFailedLabel"
                Layout.maximumWidth: window.width - 340
                wrapMode: Text.WordWrap
                visible: window.profilesController.loadFailed
                color: Theme.error
                text: qsTr("Arquivo de perfis ilegível: nada será gravado até corrigi-lo.")
            }

            Switch {
                id: profilesAutoSwitch
                objectName: "profilesAutoSwitch"
                text: qsTr("Trocar de perfil pelo aplicativo em foco")
                checked: window.profilesController.automaticSwitching
                enabled: !window.profilesController.busy
                onClicked: {
                    window.profilesController.setAutomaticSwitching(checked)
                    // O estado exibido segue o controlador: a gravação pode falhar.
                    checked = Qt.binding(() => window.profilesController.automaticSwitching)
                }
            }

            Label {
                objectName: "profilesAutoSwitchNotice"
                Layout.maximumWidth: window.width - 340
                wrapMode: Text.WordWrap
                text: qsTr("Com a troca ligada, o app carrega um pequeno script no KWin que informa só o nome do aplicativo em foco; ao trocar de aplicativo, o perfil associado é aplicado (grava na Brio 100 e muda a curva do EQ).")
            }

            Label {
                objectName: "profilesSwitchingStatus"
                text: qsTr("Troca automática: %1").arg(window.profilesController.switchingStatus)
            }

            Label {
                objectName: "profilesLastApplication"
                text: qsTr("Último aplicativo em foco: %1").arg(
                          window.profilesController.lastApplication !== ""
                          ? window.profilesController.lastApplication : qsTr("nenhum"))
            }

            RowLayout {
                TextField {
                    id: profileNameField
                    objectName: "profileNameField"
                    Layout.preferredWidth: 280
                    placeholderText: qsTr("Nome do perfil")
                    Accessible.name: qsTr("Nome do perfil")
                }

                Button {
                    objectName: "profileSaveButton"
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

                delegate: ColumnLayout {
                    id: profileRow
                    required property string modelData

                    RowLayout {
                    Label {
                        objectName: "profileNameLabel"
                        Layout.preferredWidth: 240
                        elide: Text.ElideRight
                        text: profileRow.modelData
                    }

                    Button {
                        objectName: "profileApplyButton"
                        text: qsTr("Aplicar")
                        enabled: !window.profilesController.busy
                        onClicked: window.profilesController.apply(profileRow.modelData)
                    }

                    Button {
                        objectName: "profileUpdateButton"
                        text: qsTr("Atualizar")
                        enabled: !window.profilesController.busy
                        onClicked: {
                            window.profilePendingUpdate = profileRow.modelData
                            profileUpdateDialog.open()
                        }
                    }

                    Button {
                        objectName: "profileRemoveButton"
                        text: qsTr("Remover")
                        enabled: !window.profilesController.busy
                        onClicked: {
                            window.profilePendingRemoval = profileRow.modelData
                            profileRemoveDialog.open()
                        }
                    }

                    Button {
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

                            Label {
                                objectName: "profileApplicationLabel"
                                Layout.leftMargin: 24
                                Layout.preferredWidth: 240
                                elide: Text.ElideRight
                                text: applicationRow.modelData
                            }

                            Button {
                                objectName: "profileUnassignButton"
                                text: qsTr("Remover")
                                enabled: !window.profilesController.busy
                                onClicked: window.profilesController.unassignApplication(applicationRow.modelData)
                            }
                        }
                    }
                }
            }

            Label {
                objectName: "profilesStatusLabel"
                text: window.profilesController.status
            }

            Repeater {
                model: window.profilesController.report

                delegate: Label {
                    required property string modelData
                    objectName: "profileReportLine"
                    text: modelData
                }
            }
        }


        // 6 Configurações
        TabPage {
            objectName: "settingsScrollView"
            SectionTitle { text: qsTr("Inicialização automática") }
            Label {
                objectName: "autostartNoticeLabel"
                Layout.maximumWidth: window.width - 340
                wrapMode: Text.WordWrap
                text: qsTr("Liga o aplicativo junto com a sessão (arquivo XDG em ~/.config/autostart). Nada é gravado até você ligar esta opção; desligar remove o arquivo.")
            }

            // O estado vem do controlador; ligar/desligar só é enviado quando o valor diverge (evita
            // laço entre a alternância e a releitura do arquivo). INC-013 R8.
            Switch {
                id: autostartSwitch
                objectName: "autostartSwitch"
                text: qsTr("Iniciar com a sessão")
                checked: window.desktopController.autostartEnabled
                onToggled: {
                    if (checked !== window.desktopController.autostartEnabled)
                        window.desktopController.setAutostart(checked)
                }
            }

            Label {
                objectName: "autostartStatusLabel"
                Layout.maximumWidth: window.width - 340
                wrapMode: Text.WordWrap
                text: window.desktopController.status
            }
            SectionTitle { text: qsTr("Diagnóstico") }
            Label {
                objectName: "diagnosticsNoticeLabel"
                Layout.maximumWidth: window.width - 340
                wrapMode: Text.WordWrap
                text: qsTr("Últimas mensagens do aplicativo e saúde dos componentes. Ficam só na memória: o relatório só é gravado quando você clica em exportar.")
            }

            // "Atualizar" e "Exportar" só agem por clique; nada é lido nem gravado automaticamente (R14).
            Button {
                objectName: "diagnosticsRefreshButton"
                text: qsTr("Atualizar")
                onClicked: window.diagnosticsViewModel.refresh()
            }

            Repeater {
                model: window.diagnosticsViewModel.backends

                delegate: Label {
                    required property var modelData
                    objectName: "diagnosticsBackendLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    text: modelData.name + ": "
                        + (modelData.available ? qsTr("disponível") : qsTr("indisponível"))
                        + " · " + modelData.detail
                }
            }

            Button {
                objectName: "diagnosticsExportButton"
                text: qsTr("Exportar relatório")
                onClicked: window.diagnosticsViewModel.exportReport()
            }

            Label {
                objectName: "diagnosticsStatusLabel"
                Layout.maximumWidth: window.width - 340
                wrapMode: Text.WordWrap
                text: window.diagnosticsViewModel.status
            }

            Repeater {
                model: window.diagnosticsViewModel.entries

                delegate: Label {
                    required property var modelData
                    objectName: "diagnosticsEntryLabel"
                    Layout.maximumWidth: window.width - 340
                    wrapMode: Text.WordWrap
                    text: modelData.severity + " · " + modelData.category + ": " + modelData.message
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
