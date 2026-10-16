import QtQuick
import QtQuick.Controls.Material
import QtTest
import "../../src/ui/qml"

TestCase {
    id: testCase
    name: "MainQmlBinding"

    Component {
        id: fakeViewModelComponent
        QtObject {
            property var devices: []
        }
    }

    Component {
        id: fakeCameraComponent
        QtObject {
            property string status: ""
            property var modes: []
            property var controls: []
        }
    }

    Component {
        id: fakePreviewComponent
        QtObject {
            property bool active: false
            property string status: "parada"
            property int frameWidth: 0
            property int frameHeight: 0
            property int toggleCalls: 0
            property int stopCalls: 0
            signal frameReady(var image)
            signal previewChanged()
            function toggle() { toggleCalls++ }
            function stop() { stopCalls++ }
        }
    }

    // Editor falso: registra os pedidos; nada chega a uma câmera.
    Component {
        id: fakeEditorComponent
        QtObject {
            property bool busy: false
            property string status: ""
            property var setCalls: []
            property var defaultCalls: []
            function setValue(id, value) { setCalls = setCalls.concat([[id, value]]) }
            function restoreDefault(id) { defaultCalls = defaultCalls.concat([id]) }
        }
    }

    // ViewModel do mouse falso: conta os pedidos de leitura; nada chega ao ratbagd.
    Component {
        id: fakePointerComponent
        QtObject {
            property bool reading: false
            property string state: "idle"
            property string statusText: "nenhuma leitura feita"
            property string deviceName: ""
            property string connectionText: ""
            property var profiles: []
            property var warnings: []
            property int readCalls: 0
            function read() { readCalls++ }
        }
    }

    // Controlador de iluminação falso: registra os pedidos; nada chega ao teclado.
    Component {
        id: fakeLightingComponent
        QtObject {
            property bool live: false
            property bool busy: false
            property string status: ""
            property int startCalls: 0
            property int stopCalls: 0
            property var paintCalls: []
            property var effectCalls: []
            function startLive() { startCalls++ }
            function stopLive() { stopCalls++ }
            function paintAll(color) { paintCalls = paintCalls.concat([String(color)]) }
            function applyEffect(kind, color) { effectCalls = effectCalls.concat([[kind, String(color)]]) }
        }
    }

    // Controlador do headset falso: registra os pedidos; nada chega ao HeadsetControl.
    Component {
        id: fakeHeadsetComponent
        QtObject {
            property bool busy: false
            property string status: ""
            property int lastSidetone: -1
            property string lightsState: ""
            property var sidetoneCalls: []
            property var lightsCalls: []
            function setSidetone(level) { sidetoneCalls = sidetoneCalls.concat([level]) }
            function setLights(on) { lightsCalls = lightsCalls.concat([on]) }
        }
    }

    // Controlador de cor/efeito das luzes do G733 falso: registra os pedidos; nada chega ao headset.
    Component {
        id: fakeHeadsetLightingComponent
        QtObject {
            property bool busy: false
            property string status: ""
            property var applyCalls: []
            function apply(zone, effect, red, green, blue, periodMs, brightness) {
                applyCalls = applyCalls.concat([[zone, effect, red, green, blue, periodMs, brightness]])
            }
        }
    }

    // Controlador do EQ do G733 falso: registra os pedidos; nada chega ao PipeWire.
    Component {
        id: fakeEqualizerComponent
        QtObject {
            property bool busy: false
            property bool enabled: false
            property var gains: [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
            property var bandFrequencies: ["32", "64", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]
            property int minimumGain: -12
            property int maximumGain: 12
            property string status: ""
            property int enableCalls: 0
            property int disableCalls: 0
            property int flatCalls: 0
            property var bandCalls: []
            function enable() { enableCalls++ }
            function disable() { disableCalls++ }
            function flat() { flatCalls++ }
            function setBand(band, gain) { bandCalls = bandCalls.concat([[band, gain]]) }
        }
    }

    // Controlador da redução de ruído do microfone falso: registra os pedidos; nada chega ao PipeWire.
    Component {
        id: fakeNoiseSuppressionComponent
        QtObject {
            property bool busy: false
            property bool enabled: false
            property int threshold: 50
            property int minimumThreshold: 0
            property int maximumThreshold: 99
            property string status: ""
            property int enableCalls: 0
            property int disableCalls: 0
            property var thresholdCalls: []
            function enable() { enableCalls++ }
            function disable() { disableCalls++ }
            function setThreshold(percent) { thresholdCalls = thresholdCalls.concat([percent]) }
        }
    }

    // Controlador de perfis falso: registra os pedidos; nada chega à câmera, ao EQ nem ao arquivo.
    Component {
        id: fakeProfilesComponent
        QtObject {
            property bool busy: false
            property var profileNames: []
            property string status: ""
            property var report: []
            property bool loadFailed: false
            property bool automaticSwitching: false
            property string switchingStatus: "desligada"
            property string lastApplication: ""
            property var profileApplications: ({})
            property var saveCalls: []
            property var applyCalls: []
            property var removeCalls: []
            property var switchingCalls: []
            property var assignCalls: []
            property var unassignCalls: []
            function setAutomaticSwitching(enabled) { switchingCalls = switchingCalls.concat([enabled]) }
            function assignLastApplication(name) { assignCalls = assignCalls.concat([name]) }
            function unassignApplication(application) { unassignCalls = unassignCalls.concat([application]) }
            function exists(name) { return profileNames.indexOf(name) >= 0 }
            function saveCurrent(name, replace) { saveCalls = saveCalls.concat([[name, replace]]) }
            function apply(name) { applyCalls = applyCalls.concat([name]) }
            function remove(name) { removeCalls = removeCalls.concat([name]) }
        }
    }

    // Controlador da memória onboard do G703 falso: registra os pedidos; nada chega ao mouse.
    Component {
        id: fakeOnboardComponent
        QtObject {
            property bool busy: false
            property string status: ""
            property bool loaded: false
            property int reportRateHz: 0
            property var dpiStages: []
            property int defaultStage: 0
            property bool backupExists: false
            property int activeSector: 0
            property int readCalls: 0
            property int restoreCalls: 0
            property var writeCalls: []
            function readFromMouse() { readCalls++ }
            function writeToMouse(rate, stages, defaultStage) {
                writeCalls = writeCalls.concat([[rate, stages.slice(), defaultStage]])
            }
            function restoreBackup() { restoreCalls++ }
        }
    }

    // ViewModel de identidade HID++ falso (INC-015): registra os pedidos de leitura; nada chega a um dispositivo.
    Component {
        id: fakeIdentityComponent
        QtObject {
            property var identities: ({})
            property var readCalls: []
            function read(key) { readCalls = readCalls.concat([key]) }
        }
    }

    // ViewModel de estado HID++ falso (G915/G703/G733): registra as leituras; nada chega a um dispositivo.
    Component {
        id: fakeStatusComponent
        QtObject {
            property var status: ({})
            property bool reading: false
            property string error: ""
            property int readCalls: 0
            function read() { readCalls++ }
        }
    }

    // Controlador de desktop falso (INC-013): registra os pedidos; nada é gravado em disco.
    // `setAutostart` atualiza `autostartEnabled` como o controlador real faria.
    Component {
        id: fakeDesktopComponent
        QtObject {
            property bool trayAvailable: false
            property bool hideOnClose: false
            property bool startHidden: false
            property bool quitting: false
            property bool autostartEnabled: false
            property string status: ""
            property int showWindowCalls: 0
            property int quitCalls: 0
            property var autostartCalls: []
            property int refreshAutostartCalls: 0
            signal showWindowRequested()
            signal quitRequested()
            function showWindow() { showWindowCalls++ }
            function requestQuit() { quitCalls++ }
            function setAutostart(enabled) {
                autostartCalls = autostartCalls.concat([enabled])
                autostartEnabled = enabled
            }
            function refreshAutostart() { refreshAutostartCalls++ }
        }
    }

    // ViewModel de diagnóstico falso: registra as chamadas; nada é gravado nem lido do sistema.
    Component {
        id: fakeDiagnosticsComponent
        QtObject {
            property var backends: []
            property var entries: []
            property string status: ""
            property string lastReportPath: ""
            property int refreshCalls: 0
            property int exportCalls: 0
            function refresh() { refreshCalls++ }
            function exportReport() { exportCalls++ }
        }
    }

    // A janela precisa ser destruída antes do ViewModel falso (evita TypeError de binding no teardown).
    property var windows: []
    property var fakes: []

    function cleanup() {
        windows.forEach(w => w.destroy());
        fakes.forEach(f => f.destroy());
        windows = [];
        fakes = [];
    }

    // Carrega o Main.qml real do código-fonte (caminho relativo a este arquivo).
    function createMain(fake, inventoryFake, cameraFake, previewFake, editorFake, pointerFake, lightingFake,
                        headsetFake, equalizerFake, onboardFake, noiseFake, desktopFake, diagnosticsFake,
                        profilesFake, identityFake, keyboardStatusFake, pointerStatusFake, headsetStatusFake, lightingColorFake) {
        const component = Qt.createComponent(Qt.resolvedUrl("../../src/ui/qml/Main.qml"));
        compare(component.status, Component.Ready, component.errorString());
        const window = component.createObject(null, {batteryViewModel: fake,
                                                     deviceInventoryViewModel: inventoryFake || createFake([]),
                                                     cameraViewModel: cameraFake || createCameraFake("não detectada", [], []),
                                                     cameraPreviewViewModel: previewFake || createPreviewFake(),
                                                     cameraControlEditor: editorFake || createEditorFake(),
                                                     pointerViewModel: pointerFake || createPointerFake(),
                                                     keyboardLightingController: lightingFake || createLightingFake(),
                                                     headsetSettingsController: headsetFake || createHeadsetFake(),
                                                     headsetLightingController: lightingColorFake || createHeadsetLightingFake(),
                                                     hostEqualizerController: equalizerFake || createEqualizerFake(),
                                                     noiseSuppressionController: noiseFake || createNoiseSuppressionFake(),
                                                     g703OnboardController: onboardFake || createOnboardFake(),
                                                     desktopController: desktopFake || createDesktopFake(),
                                                     diagnosticsViewModel: diagnosticsFake || createDiagnosticsFake(),
                                                     profilesController: profilesFake || createProfilesFake(),
                                                     deviceIdentityViewModel: identityFake || createIdentityFake(),
                                                     keyboardStatusViewModel: keyboardStatusFake || createStatusFake(),
                                                     pointerStatusViewModel: pointerStatusFake || createStatusFake(),
                                                     headsetStatusViewModel: headsetStatusFake || createStatusFake()});
        verify(window !== null, "Main.qml deve instanciar");
        windows.push(window);
        return window;
    }

    function createFake(devices) {
        const fake = fakeViewModelComponent.createObject(null, {devices: devices});
        fakes.push(fake);
        return fake;
    }

    function createCameraFake(status, modes, controls) {
        const fake = fakeCameraComponent.createObject(null, {status: status, modes: modes, controls: controls});
        fakes.push(fake);
        return fake;
    }

    function createPreviewFake(props) {
        const fake = fakePreviewComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createEditorFake(props) {
        const fake = fakeEditorComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createPointerFake(props) {
        const fake = fakePointerComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createLightingFake(props) {
        const fake = fakeLightingComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createHeadsetFake(props) {
        const fake = fakeHeadsetComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createHeadsetLightingFake(props) {
        const fake = fakeHeadsetLightingComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createEqualizerFake(props) {
        const fake = fakeEqualizerComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createNoiseSuppressionFake(props) {
        const fake = fakeNoiseSuppressionComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createStatusFake(props) {
        const fake = fakeStatusComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function identityEntry(state, extra) {
        return Object.assign({state: state, name: "", type: "", firmwares: [], featureCount: 0, featureIds: [],
                              truncationNote: "", error: ""}, extra || {});
    }

    function createIdentityFake(entries) {
        const identities = {g915: identityEntry("idle"), g703: identityEntry("idle"), g733: identityEntry("idle")};
        const fake = fakeIdentityComponent.createObject(null, {identities: Object.assign(identities, entries || {})});
        fakes.push(fake);
        return fake;
    }

    function visibleByName(window, objectName) {
        return allItems(window).filter(i => i.objectName === objectName && i.visible && i.width > 0);
    }

    function createMainWithNoiseSuppression(noise) {
        return createMain(createFake([]), null, null, null, null, null, null, null, null, null, noise);
    }

    function createProfilesFake(props) {
        const fake = fakeProfilesComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createMainWithProfiles(profiles) {
        return createMain(createFake([]), null, null, null, null, null, null, null, null, null, null, null, null,
                          profiles);
    }

    function createOnboardFake(props) {
        const fake = fakeOnboardComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createMainWithEqualizer(equalizer) {
        return createMain(createFake([]), null, null, null, null, null, null, null, equalizer);
    }

    function eqSliders(window) {
        return allItems(window).filter(item => item.objectName === "eqBandSlider");
    }

    function createMainWithOnboard(onboard) {
        return createMain(createFake([]), null, null, null, null, null, null, null, null, onboard);
    }

    function createDesktopFake(props) {
        const fake = fakeDesktopComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createDiagnosticsFake(props) {
        const fake = fakeDiagnosticsComponent.createObject(null, props || {});
        fakes.push(fake);
        return fake;
    }

    function createMainWithHeadset(headset) {
        return createMain(createFake([]), null, null, null, null, null, null, headset);
    }

    function createMainWithDesktop(desktop) {
        return createMain(createFake([]), null, null, null, null, null, null, null, null, null, null, desktop);
    }

    function createMainWithDesktopAndDiagnostics(desktop, diagnostics) {
        return createMain(createFake([]), null, null, null, null, null, null, null, null, null, null, desktop, diagnostics);
    }

    // Rola a ScrollView até o item ficar visível (mouseClick não rola; a janela é limitada pela tela offscreen).
    function reveal(item) {
        for (let flick = item.parent; flick; flick = flick.parent) {
            if (flick.contentY !== undefined && flick.flickableDirection !== undefined) {
                flick.contentY = Math.max(0, item.mapToItem(flick.contentItem, 0, 0).y - 20);
                return;
            }
        }
    }

    // Navegação por cliques reais (INC-014): cartão da tela inicial -> aba lateral. A janela precisa estar
    // visível para o mouseClick acertar o item.
    function deviceCards(window) {
        return allItems(window).filter(item => item.objectName === "deviceCard");
    }

    function openDevice(window, name) {
        window.show();
        waitForRendering(window.contentItem);
        const card = deviceCards(window).find(c => c.deviceName === name);
        verify(card !== undefined, "cartão " + name);
        tryVerify(() => card.visible && card.width > 0 && card.height > 0);
        // Os cartões da parte 3 têm 450 px de altura: a segunda fileira precisa ser rolada para dentro da vista.
        reveal(card);
        waitForRendering(window.contentItem);
        mouseClick(card);
        tryVerify(() => window.currentPage !== 0);
        // A página recém-aberta só posiciona o trilho (ColumnLayout) no próximo quadro.
        waitForRendering(window.contentItem);
    }

    function openTab(window, key) {
        const visibleTab = () => allItems(window).find(item => item.objectName === "sideTab" && item.tabKey === key
                                                           && item.visible && item.width > 0);
        tryVerify(() => visibleTab() !== undefined);
        mouseClick(visibleTab());
        tryVerify(() => visibleTab().selected);
        waitForRendering(window.contentItem);
    }

    function openDeviceTab(window, name, key) {
        openDevice(window, name);
        openTab(window, key);
    }

    function clickButton(window, objectName) {
        window.show();
        const button = findItem(window, objectName);
        tryVerify(() => button.visible && button.width > 0);
        mouseClick(button);
        waitForRendering(window.contentItem);
    }

    function swatches(window) {
        return allItems(window).filter(item => item.objectName === "lightingSwatch");
    }

    function effectCombo(window) {
        return findItem(window, "lightingEffectCombo");
    }

    function pointerProfile(overrides) {
        return Object.assign({index: 0, name: "", active: false, enabled: true, reportRate: 1000,
                              supportedReportRates: [125, 250, 500, 1000],
                              stages: [{index: 0, dpiText: "7050 dpi", active: true, isDefault: true,
                                        disabled: false, listed: false}],
                              buttons: [{index: 5, text: "especial 0x40000007"}],
                              leds: [{index: 0, mode: "desligado", color: "#000000", brightness: 0}]},
                             overrides || {});
    }

    function integerControl(overrides) {
        return Object.assign({id: 1, name: "Brightness", summary: "128 (0–255, padrão 128)", kind: "integer",
                              minimum: 0, maximum: 255, step: 1, defaultValue: 128, value: 128, options: [],
                              editable: true}, overrides || {});
    }

    function booleanControl(overrides) {
        return Object.assign({id: 2, name: "White Balance, Automatic", summary: "ligado", kind: "boolean",
                              minimum: 0, maximum: 1, step: 1, defaultValue: 1, value: 1, options: [],
                              editable: true}, overrides || {});
    }

    function menuControl(overrides) {
        return Object.assign({id: 3, name: "Power Line Frequency", summary: "60 Hz", kind: "menu",
                              minimum: 0, maximum: 2, step: 1, defaultValue: 2, value: 2,
                              options: [{value: 0, label: "Disabled"}, {value: 1, label: "50 Hz"},
                                        {value: 2, label: "60 Hz"}],
                              editable: true}, overrides || {});
    }

    function findItem(window, objectName) {
        const found = allItems(window).filter(item => item.objectName === objectName);
        compare(found.length, 1, objectName);
        return found[0];
    }

    // Diálogos (Popup) não estão na árvore visual: procura entre os filhos não visuais do contentItem.
    function findDialog(window, objectName) {
        const found = [];
        const data = window.contentItem.data;
        for (let i = 0; i < data.length; ++i) {
            if (data[i].objectName === objectName)
                found.push(data[i]);
        }
        compare(found.length, 1, objectName);
        return found[0];
    }

    // Percorre a árvore visual (inclui o contentItem de Flickable/ScrollView) em largura.
    function allItems(window) {
        const items = [];
        const pending = [window.contentItem];
        if (window.header)
            pending.push(window.header);
        while (pending.length > 0) {
            const item = pending.shift();
            items.push(item);
            for (let i = 0; i < item.children.length; ++i)
                pending.push(item.children[i]);
            if (item.contentItem && item.contentItem !== item && !items.includes(item.contentItem)
                    && !pending.includes(item.contentItem))
                pending.push(item.contentItem);
        }
        return items;
    }

    function labelTexts(window, objectName = "batteryDeviceLabel") {
        return allItems(window).filter(item => item.objectName === objectName).map(item => item.text);
    }

    function test_showsOneLabelPerDevice() {
        const window = createMain(createFake([
            {name: "G915", summary: "44% · descarregando"},
            {name: "G703", summary: "não detectado"}]));
        tryVerify(() => JSON.stringify(labelTexts(window))
                  === JSON.stringify(["G915: 44% · descarregando", "G703: não detectado"]));
    }

    function test_labelsFollowDevicesChange() {
        const fake = createFake([
            {name: "G915", summary: "44% · descarregando"},
            {name: "G703", summary: "não detectado"}]);
        const window = createMain(fake);
        tryVerify(() => labelTexts(window).length === 2);
        fake.devices = [{name: "G703", summary: "80% · carregando"}];
        tryVerify(() => JSON.stringify(labelTexts(window))
                  === JSON.stringify(["G703: 80% · carregando"]));
    }

    function test_inventoryShowsOneLabelPerModel() {
        const window = createMain(createFake([]), createFake([
            {name: "G915", summary: "detectado via receptor Lightspeed"},
            {name: "G733", summary: "não detectado"}]));
        tryVerify(() => JSON.stringify(labelTexts(window, "inventoryDeviceLabel"))
                  === JSON.stringify(["G915: detectado via receptor Lightspeed", "G733: não detectado"]));
    }

    function test_inventoryLabelsFollowDevicesChange() {
        const inventory = createFake([{name: "G703", summary: "não detectado"}]);
        const window = createMain(createFake([]), inventory);
        tryVerify(() => labelTexts(window, "inventoryDeviceLabel").length === 1);
        inventory.devices = [{name: "G703", summary: "detectado via receptor Lightspeed e cabo USB"}];
        tryVerify(() => JSON.stringify(labelTexts(window, "inventoryDeviceLabel"))
                  === JSON.stringify(["G703: detectado via receptor Lightspeed e cabo USB"]));
    }

    function test_cameraShowsStatus() {
        const window = createMain(createFake([]), null, createCameraFake("não detectada", [], []));
        tryVerify(() => JSON.stringify(labelTexts(window, "cameraStatusLabel"))
                  === JSON.stringify(["não detectada"]));
        compare(labelTexts(window, "cameraModeLabel").length, 0);
        compare(labelTexts(window, "cameraControlLabel").length, 0);
    }

    function test_cameraShowsModesAndControls() {
        const window = createMain(createFake([]), null, createCameraFake("Brio 100",
            [{text: "MJPG: até 1920×1080 a 30 fps"}, {text: "YUYV: até 640×480 a 30 fps"}],
            [{name: "Brightness", summary: "128 (0–255, padrão 128)"},
             {name: "Contrast", summary: "32 (0–64, padrão 32)"},
             {name: "Saturation", summary: "64 (0–128, padrão 64)"}]));
        tryVerify(() => JSON.stringify(labelTexts(window, "cameraStatusLabel")) === JSON.stringify(["Brio 100"]));
        tryVerify(() => JSON.stringify(labelTexts(window, "cameraModeLabel"))
                  === JSON.stringify(["MJPG: até 1920×1080 a 30 fps", "YUYV: até 640×480 a 30 fps"]));
        tryVerify(() => JSON.stringify(labelTexts(window, "cameraControlLabel"))
                  === JSON.stringify(["Brightness: 128 (0–255, padrão 128)",
                                      "Contrast: 32 (0–64, padrão 32)",
                                      "Saturation: 64 (0–128, padrão 64)"]));
    }

    function test_cameraFollowsViewModelChange() {
        const camera = createCameraFake("não detectada", [], []);
        const window = createMain(createFake([]), null, camera);
        tryVerify(() => labelTexts(window, "cameraStatusLabel").length === 1);
        camera.status = "Brio 100";
        camera.modes = [{text: "MJPG: até 1920×1080 a 30 fps"}];
        camera.controls = [{name: "Zoom", summary: "100 (100–500, padrão 100)"}];
        tryVerify(() => JSON.stringify(labelTexts(window, "cameraStatusLabel")) === JSON.stringify(["Brio 100"]));
        tryVerify(() => JSON.stringify(labelTexts(window, "cameraModeLabel"))
                  === JSON.stringify(["MJPG: até 1920×1080 a 30 fps"]));
        tryVerify(() => JSON.stringify(labelTexts(window, "cameraControlLabel"))
                  === JSON.stringify(["Zoom: 100 (100–500, padrão 100)"]));
        camera.modes = [];
        camera.controls = [];
        tryVerify(() => labelTexts(window, "cameraModeLabel").length === 0
                  && labelTexts(window, "cameraControlLabel").length === 0);
    }

    function test_cameraShowsEachControlKindWithItsComponent() {
        const window = createMain(createFake([]), null, createCameraFake("Brio 100", [],
            [integerControl(), booleanControl(), menuControl()]));
        tryVerify(() => labelTexts(window, "cameraControlLabel").length === 3);
        const slider = findItem(window, "cameraControlSlider");
        compare(slider.from, 0);
        compare(slider.to, 255);
        compare(slider.stepSize, 1);
        compare(slider.value, 128);
        const toggle = findItem(window, "cameraControlSwitch");
        compare(toggle.checked, true);
        const combo = findItem(window, "cameraControlCombo");
        compare(combo.currentText, "60 Hz");
        compare(combo.currentValue, 2);
        compare(allItems(window).filter(item => item.objectName === "cameraControlDefault").length, 3);
    }

    function test_inactiveControlIsDisabled() {
        const window = createMain(createFake([]), null, createCameraFake("Brio 100", [],
            [integerControl({editable: false, summary: "128 (0–255, padrão 128) · inativo"})]));
        const slider = findItem(window, "cameraControlSlider");
        compare(slider.enabled, false);
        compare(findItem(window, "cameraControlDefault").enabled, false);
    }

    function test_noEditorCallIsMadeOnLoad() {
        const editor = createEditorFake();
        createMain(createFake([]), null, createCameraFake("Brio 100", [],
            [integerControl(), booleanControl(), menuControl()]), null, editor);
        wait(50);
        compare(editor.setCalls.length, 0);
        compare(editor.defaultCalls.length, 0);
    }

    function test_clickingDefaultCallsEditorRestoreDefault() {
        const editor = createEditorFake();
        const window = createMain(createFake([]), null, createCameraFake("Brio 100", [],
            [integerControl({id: 7})]), null, editor);
        window.show();
        openDeviceTab(window, "Brio 100", "image");
        const button = findItem(window, "cameraControlDefault");
        tryVerify(() => button.width > 0 && button.visible);
        reveal(button);
        mouseClick(button);
        tryCompare(editor, "defaultCalls", [7]);
        compare(editor.setCalls.length, 0);
    }

    function test_togglingSwitchCallsEditorSetValue() {
        const editor = createEditorFake();
        const window = createMain(createFake([]), null, createCameraFake("Brio 100", [],
            [booleanControl({id: 9})]), null, editor);
        window.show();
        openDeviceTab(window, "Brio 100", "image");
        const toggle = findItem(window, "cameraControlSwitch");
        tryVerify(() => toggle.width > 0 && toggle.visible);
        reveal(toggle);
        mouseClick(toggle);
        tryCompare(editor, "setCalls", [[9, 0]]);
    }

    function test_sliderWritesOnlyOnRelease() {
        const editor = createEditorFake();
        const window = createMain(createFake([]), null, createCameraFake("Brio 100", [],
            [integerControl({id: 5})]), null, editor);
        window.show();
        openDeviceTab(window, "Brio 100", "image");
        const slider = findItem(window, "cameraControlSlider");
        tryVerify(() => slider.width > 0 && slider.visible);
        reveal(slider);
        const y = slider.height / 2;
        mousePress(slider, slider.leftPadding + slider.availableWidth * 0.25, y);
        mouseMove(slider, slider.leftPadding + slider.availableWidth * 0.5, y);
        mouseMove(slider, slider.leftPadding + slider.availableWidth * 0.75, y);
        compare(editor.setCalls.length, 0);
        mouseRelease(slider, slider.leftPadding + slider.availableWidth * 0.75, y);
        tryCompare(editor.setCalls, "length", 1);
        compare(editor.setCalls[0][0], 5);
        verify(Math.abs(editor.setCalls[0][1] - 191) <= 8, "valor gravado: " + editor.setCalls[0][1]);
    }

    function test_comboActivationCallsEditorSetValue() {
        const editor = createEditorFake();
        const window = createMain(createFake([]), null, createCameraFake("Brio 100", [],
            [menuControl({id: 3})]), null, editor);
        const combo = findItem(window, "cameraControlCombo");
        combo.activated(1);
        compare(editor.setCalls.length, 1);
        compare(editor.setCalls[0][0], 3);
    }

    function test_editorStatusFollowsEditor() {
        const editor = createEditorFake();
        const window = createMain(createFake([]), null, null, null, editor);
        const label = findItem(window, "cameraControlEditorStatus");
        compare(label.text, "");
        editor.status = "Brightness: 150";
        tryCompare(label, "text", "Brightness: 150");
    }

    // Fora dos controles há dois botões na janela: o de pré-visualização e o de leitura do mouse (INC-007);
    // os demais são os "Padrão" (um por controle). Desde o INC-015 as abas Informações de G915, G703 e G733 (não a
    // da Brio 100) têm "Ler identidade" e o botão que expande os IDs das features.
    function test_cameraSectionButtons_ArePreviewToggleAndOneDefaultPerControl() {
        const window = createMain(createFake([]), null, createCameraFake("Brio 100",
            [{text: "MJPG: até 1920×1080 a 30 fps"}], [integerControl(), booleanControl()]));
        tryVerify(() => labelTexts(window, "cameraControlLabel").length === 2);
        const items = allItems(window);
        verify(items.some(item => /^(Label|QQuickLabel)/.test(item.toString())), "toString deve expor o tipo");
        // O fundo do Button (ButtonPanel) não é um botão: o padrão exige o nome do tipo seguido de "(".
        // Desde a parte 3 do INC-014 os botões do painel são GHubButton (o estilo reto do G HUB).
        const buttons = items.filter(item => /^(GHubButton|Button|QQuickButton)(_QMLTYPE_\d+)?\(/.test(item.toString()));
        compare(buttons.map(b => b.objectName).sort().join(","),
                "cameraControlDefault,cameraControlDefault,cameraPreviewToggle,diagnosticsExportButton,"
                + "diagnosticsRefreshButton,eqFlatButton,headsetLightingApplyButton,headsetLightsOffButton,"
                + "headsetLightsOnButton,headsetSidetoneApplyButton,"
                + "onboardAddStageButton,onboardReadButton,onboardRemoveStageButton,"
                + "onboardRestoreButton,onboardWriteButton,pointerReadButton,profileSaveButton,"
                + "readIdentityButton,readIdentityButton,readIdentityButton,"
                + "readStatusButton,readStatusButton,readStatusButton,"
                + "toggleFeatureIdsButton,toggleFeatureIdsButton,toggleFeatureIdsButton"); // amostras: delegates de tipo próprio, testadas à parte
    }

    function test_onboardSectionWarnsAndDoesNotTouchTheMouseOnLoad() {
        const onboard = createOnboardFake();
        const window = createMainWithOnboard(onboard);
        verify(findItem(window, "onboardNoticeLabel").text.indexOf("Feche o Piper") >= 0);
        compare(findItem(window, "onboardEditor").visible, false);
        compare(onboard.readCalls, 0);
        compare(onboard.restoreCalls, 0);
        compare(onboard.writeCalls.length, 0);
    }

    function test_onboardReadButtonCallsReadOnClick() {
        const onboard = createOnboardFake();
        const window = createMainWithOnboard(onboard);
        const button = findItem(window, "onboardReadButton");
        compare(button.text, "Ler do mouse");
        button.clicked();
        compare(onboard.readCalls, 1);
    }

    function test_onboardEditorAppearsOnceLoadedAndWriteForwardsTheEditedValues() {
        const onboard = createOnboardFake();
        const window = createMainWithOnboard(onboard);
        openDeviceTab(window, "G703", "onboard");
        onboard.reportRateHz = 1000;
        onboard.dpiStages = [7050, 1600];
        onboard.defaultStage = 1;
        onboard.loaded = true;
        tryCompare(findItem(window, "onboardEditor"), "visible", true);
        tryVerify(() => allItems(window).filter(item => item.objectName === "onboardDpiField").length === 2);
        compare(allItems(window).filter(item => item.objectName === "onboardDpiField")[0].value, 7050);
        findItem(window, "onboardWriteButton").clicked();
        compare(JSON.stringify(onboard.writeCalls), JSON.stringify([[1000, [7050, 1600], 1]]));
    }

    function test_onboardRestoreEnabledOnlyWithBackupAndButtonsDisabledWhileBusy() {
        const onboard = createOnboardFake({loaded: true, dpiStages: [800], reportRateHz: 500});
        const window = createMainWithOnboard(onboard);
        const restore = findItem(window, "onboardRestoreButton");
        compare(restore.enabled, false);
        onboard.backupExists = true;
        tryCompare(restore, "enabled", true);
        restore.clicked();
        compare(onboard.restoreCalls, 1);
        onboard.busy = true;
        tryCompare(restore, "enabled", false);
        compare(findItem(window, "onboardWriteButton").enabled, false);
        compare(findItem(window, "onboardReadButton").enabled, false);
    }

    function test_onboardStatusShowsControllerStatus() {
        const onboard = createOnboardFake();
        const window = createMainWithOnboard(onboard);
        onboard.status = "Falha: em uso por piper";
        tryCompare(findItem(window, "onboardStatusLabel"), "text", "Falha: em uso por piper");
    }

    function test_previewToggleShowsStartText_WhenStopped() {
        const window = createMain(createFake([]));
        compare(findItem(window, "cameraPreviewToggle").text, "Iniciar pré-visualização");
    }

    function test_previewToggleShowsStopText_WhenActive() {
        const window = createMain(createFake([]), null, null, createPreviewFake({active: true}));
        compare(findItem(window, "cameraPreviewToggle").text, "Parar pré-visualização");
    }

    function test_clickingToggleCallsViewModelOnce() {
        const preview = createPreviewFake();
        const window = createMain(createFake([]), null, null, preview);
        window.show();
        openDeviceTab(window, "Brio 100", "image");
        const button = findItem(window, "cameraPreviewToggle");
        tryVerify(() => button.width > 0 && button.visible);
        reveal(button);
        mouseClick(button);
        tryCompare(preview, "toggleCalls", 1);
    }

    function test_previewStatusFollowsViewModel() {
        const preview = createPreviewFake();
        const window = createMain(createFake([]), null, null, preview);
        const label = findItem(window, "cameraPreviewStatusLabel");
        compare(label.text, "Pré-visualização: parada");
        preview.status = "ao vivo · MJPG 1280×720 a 30 fps";
        tryCompare(label, "text", "Pré-visualização: ao vivo · MJPG 1280×720 a 30 fps");
    }

    function test_previewItemVisibleOnlyWhenActive() {
        const preview = createPreviewFake();
        const window = createMain(createFake([]), null, null, preview);
        openDeviceTab(window, "Brio 100", "image");
        const item = findItem(window, "cameraPreviewItem");
        compare(item.visible, false);
        preview.active = true;
        tryCompare(item, "visible", true);
        preview.active = false;
        tryCompare(item, "visible", false);
    }

    function test_previewItemKeepsModeAspectRatio() {
        const preview = createPreviewFake({active: true, frameWidth: 1280, frameHeight: 720});
        const window = createMain(createFake([]), null, null, preview);
        openDeviceTab(window, "Brio 100", "image");
        const item = findItem(window, "cameraPreviewItem");
        verify(item.width > 0);
        verify(Math.abs(item.height - item.width * 9 / 16) <= 1, item.width + "x" + item.height);
        // Modo 4:3 depois da mudança.
        preview.frameWidth = 640;
        preview.frameHeight = 480;
        tryVerify(() => Math.abs(item.height - item.width * 3 / 4) <= 1);
    }

    function test_previewItemUses16by9_WhenFrameWidthIsZero() {
        const preview = createPreviewFake({active: true, frameWidth: 0, frameHeight: 0});
        const window = createMain(createFake([]), null, null, preview);
        openDeviceTab(window, "Brio 100", "image");
        const item = findItem(window, "cameraPreviewItem");
        verify(item.width > 0);
        verify(Math.abs(item.height - item.width * 9 / 16) <= 1, item.width + "x" + item.height);
    }

    function test_frameReadyIsForwardedToItem() {
        const preview = createPreviewFake({active: true});
        const window = createMain(createFake([]), null, null, preview);
        const item = findItem(window, "cameraPreviewItem");
        compare(item.hasFrame, false);
        preview.frameReady(testImages.solid());
        compare(item.hasFrame, true);
    }

    function test_itemIsClearedWhenPreviewStops() {
        const preview = createPreviewFake({active: true});
        const window = createMain(createFake([]), null, null, preview);
        const item = findItem(window, "cameraPreviewItem");
        preview.frameReady(testImages.solid());
        compare(item.hasFrame, true);
        preview.active = false;
        preview.previewChanged();
        compare(item.hasFrame, false);
    }

    // R1: a janela nunca inicia nem para a pré-visualização por conta própria ao carregar.
    function test_windowStartsWithoutTogglingPreview() {
        const preview = createPreviewFake();
        createMain(createFake([]), null, null, preview);
        wait(50);
        compare(preview.toggleCalls, 0);
        compare(preview.stopCalls, 0);
    }

    function test_closingWindowStopsPreview() {
        const preview = createPreviewFake();
        const window = createMain(createFake([]), null, null, preview);
        window.show();
        window.close();
        compare(preview.stopCalls, 1);
    }

    // INC-013 R9: `--autostart` esconde na abertura (só faz sentido com bandeja).
    function test_windowStartsHiddenWithAutostart() {
        const desktop = createDesktopFake({trayAvailable: true, hideOnClose: true, startHidden: true});
        const window = createMainWithDesktop(desktop);
        compare(window.visible, false);
    }

    function test_windowStartsVisibleWithoutAutostart() {
        const desktop = createDesktopFake({trayAvailable: true, hideOnClose: true});
        const window = createMainWithDesktop(desktop);
        compare(window.visible, true);
    }

    // INC-013 R4/R5: com bandeja, fechar esconde (o fechamento é recusado) e encerra a pré-visualização.
    function test_closingWindowHidesWhenTrayAvailable() {
        const preview = createPreviewFake();
        const desktop = createDesktopFake({trayAvailable: true, hideOnClose: true});
        const window = createMain(createFake([]), null, null, preview, null, null, null, null, null, null, null, desktop);
        window.show();
        compare(window.close(), false);
        compare(window.visible, false);
        compare(preview.stopCalls, 1);
    }

    // INC-013 R4: durante um encerramento pedido, fechar deixa a janela fechar de verdade.
    function test_closingWindowQuitsWhenQuitting() {
        const desktop = createDesktopFake({trayAvailable: true, hideOnClose: true, quitting: true});
        const window = createMainWithDesktop(desktop);
        window.show();
        compare(window.close(), true);
        compare(window.visible, false);
    }

    // INC-013 R8: a alternância reflete o estado e ligar chama o controlador (que grava o .desktop).
    // O `Switch` fica no fim da rolagem: rolamos até o fim e clicamos de verdade (o `toggled` só dispara
    // com interação do usuário; `toggle()` programático não emite o sinal).
    function test_autostartSwitchOffByDefaultAndTogglingCallsSetAutostart() {
        const desktop = createDesktopFake();
        const window = createMainWithDesktop(desktop);
        window.show();
        clickButton(window, "settingsPageButton");
        const view = findItem(window, "settingsScrollView");
        tryVerify(() => view.visible && view.height > 0);
        view.contentItem.contentY = Math.max(0, view.contentItem.contentHeight - view.height);
        const toggle = findItem(window, "autostartSwitch");
        tryVerify(() => toggle.mapToItem(null, 0, 0).y >= 0
                  && toggle.mapToItem(null, 0, 0).y + toggle.height <= window.height);
        compare(toggle.checked, false);
        mouseClick(toggle);
        tryCompare(desktop, "autostartCalls", [true]);
        compare(toggle.checked, true);
    }

    function test_autostartSwitchReflectsEnabledState() {
        const desktop = createDesktopFake({autostartEnabled: true});
        const window = createMainWithDesktop(desktop);
        tryVerify(() => findItem(window, "autostartSwitch").checked === true);
        compare(desktop.autostartCalls.length, 0);
    }

    function test_autostartStatusFollowsController() {
        const desktop = createDesktopFake({status: "Inicialização automática desligada"});
        const window = createMainWithDesktop(desktop);
        const label = findItem(window, "autostartStatusLabel");
        compare(label.text, "Inicialização automática desligada");
        desktop.status = "não foi possível gravar /home/x/.config/autostart/ghub-linux.desktop";
        tryCompare(label, "text", "não foi possível gravar /home/x/.config/autostart/ghub-linux.desktop");
    }

    // R8: abrir a janela só lê o estado; nada é escrito sem ação explícita.
    function test_autostartWritesNothingOnLoad() {
        const desktop = createDesktopFake();
        createMainWithDesktop(desktop);
        wait(50);
        compare(desktop.autostartCalls.length, 0);
        compare(desktop.refreshAutostartCalls, 0);
    }

    // INC-013 R14: a lista de saúde e as últimas entradas vêm do ViewModel, sem ação do usuário.
    function test_diagnosticsListsBackendsAndEntries() {
        const diagnostics = createDiagnosticsFake({
            backends: [{name: "Bandeja do sistema", available: true, detail: "ícone disponível"},
                       {name: "ratbagd (G703)", available: false, detail: "serviço ausente"}],
            entries: [{timestamp: "2026-10-14T10:00:00.000", severity: "aviso", category: "ghub.camera",
                       message: "falha ao ler"}]});
        const window = createMainWithDesktopAndDiagnostics(null, diagnostics);
        tryVerify(() => JSON.stringify(labelTexts(window, "diagnosticsBackendLabel"))
                  === JSON.stringify(["Bandeja do sistema: disponível · ícone disponível",
                                      "ratbagd (G703): indisponível · serviço ausente"]));
        tryVerify(() => labelTexts(window, "diagnosticsEntryLabel").length === 1);
        verify(labelTexts(window, "diagnosticsEntryLabel")[0].indexOf("falha ao ler") >= 0);
        verify(labelTexts(window, "diagnosticsEntryLabel")[0].indexOf("aviso") >= 0);
    }

    // R14: "Atualizar" e "Exportar" só agem por clique; nada é lido nem gravado na abertura.
    function test_diagnosticsRefreshAndExportOnlyOnClick() {
        const diagnostics = createDiagnosticsFake();
        const window = createMainWithDesktopAndDiagnostics(null, diagnostics);
        compare(diagnostics.refreshCalls, 0);
        compare(diagnostics.exportCalls, 0);
        findItem(window, "diagnosticsRefreshButton").clicked();
        compare(diagnostics.refreshCalls, 1);
        findItem(window, "diagnosticsExportButton").clicked();
        compare(diagnostics.exportCalls, 1);
        compare(diagnostics.refreshCalls, 1);
    }

    function test_diagnosticsStatusFollowsViewModel() {
        const diagnostics = createDiagnosticsFake({status: "relatório gravado em /home/x/relatorio.txt"});
        const window = createMainWithDesktopAndDiagnostics(null, diagnostics);
        const label = findItem(window, "diagnosticsStatusLabel");
        compare(label.text, "relatório gravado em /home/x/relatorio.txt");
        diagnostics.status = "falha ao exportar o relatório: sem espaço";
        tryCompare(label, "text", "falha ao exportar o relatório: sem espaço");
    }

    function test_pointerSectionWarnsAboutRatbagdAndDoesNotReadOnLoad() {
        const pointer = createPointerFake();
        const window = createMain(createFake([]), null, null, null, null, pointer);
        const warning = findItem(window, "pointerNoticeLabel");
        verify(warning.text.indexOf("ratbagd") >= 0, warning.text);
        verify(warning.text.indexOf("G733") >= 0, warning.text);
        compare(findItem(window, "pointerStatusLabel").text, "nenhuma leitura feita");
        compare(pointer.readCalls, 0);
    }

    function test_pointerButtonCallsReadOnClick() {
        const pointer = createPointerFake();
        const window = createMain(createFake([]), null, null, null, null, pointer);
        const button = findItem(window, "pointerReadButton");
        verify(button.enabled);
        button.clicked();
        compare(pointer.readCalls, 1);
    }

    function test_pointerButtonDisabledWhileReading() {
        const pointer = createPointerFake({reading: true, state: "reading", statusText: "lendo a configuração do mouse…"});
        const window = createMain(createFake([]), null, null, null, null, pointer);
        const button = findItem(window, "pointerReadButton");
        compare(button.enabled, false);
        compare(findItem(window, "pointerStatusLabel").text, "lendo a configuração do mouse…");
        pointer.reading = false;
        tryVerify(() => button.enabled);
    }

    function test_pointerShowsStatusAndWarnings() {
        const pointer = createPointerFake({state: "unavailable",
                                           statusText: "leitura indisponível: em uso por openrgb",
                                           warnings: ["nenhum perfil ativo"]});
        const window = createMain(createFake([]), null, null, null, null, pointer);
        compare(findItem(window, "pointerStatusLabel").text, "leitura indisponível: em uso por openrgb");
        tryVerify(() => JSON.stringify(labelTexts(window, "pointerWarningLabel"))
                  === JSON.stringify(["nenhum perfil ativo"]));
        const warning = allItems(window).filter(item => item.objectName === "pointerWarningLabel")[0];
        verify(warning.font.bold === true);
    }

    function test_pointerShowsProfileStagesButtonsAndLeds() {
        const pointer = createPointerFake({state: "ready", statusText: "configuração lida",
                                           deviceName: "Logitech G703 LS", connectionText: "receptor Lightspeed",
                                           profiles: [pointerProfile({index: 0}),
                                                      pointerProfile({index: 1, active: true, enabled: false})]});
        const window = createMain(createFake([]), null, null, null, null, pointer);
        tryVerify(() => labelTexts(window, "pointerProfileHeader").length === 2);
        const headers = labelTexts(window, "pointerProfileHeader");
        verify(headers[0].indexOf("Perfil 0") >= 0 && headers[0].indexOf("1000 Hz") >= 0, headers[0]);
        verify(headers[1].indexOf("ativo segundo o ratbagd") >= 0 && headers[1].indexOf("desabilitado") >= 0, headers[1]);
        compare(labelTexts(window, "pointerStageLabel").length, 2);
        verify(labelTexts(window, "pointerStageLabel")[0].indexOf("7050 dpi") >= 0);
        verify(labelTexts(window, "pointerButtonLabel")[0].indexOf("especial 0x40000007") >= 0);
        const led = labelTexts(window, "pointerLedLabel")[0];
        verify(led.indexOf("desligado") >= 0 && led.indexOf("#000000") >= 0, led);
        verify(labelTexts(window, "pointerDeviceLabel")[0].indexOf("Logitech G703 LS") >= 0);
        verify(labelTexts(window, "pointerDeviceLabel")[0].indexOf("receptor Lightspeed") >= 0);
    }

    function test_lightingSectionWarnsAndDoesNothingOnLoad() {
        const lighting = createLightingFake();
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const notice = findItem(window, "lightingNoticeLabel").text;
        verify(notice.indexOf("G915") >= 0 && notice.indexOf("Nada é gravado") >= 0, notice);
        compare(lighting.startCalls, 0);
        compare(lighting.stopCalls, 0);
        compare(lighting.paintCalls.length, 0);
        compare(lighting.effectCalls.length, 0);
    }

    function test_lightingNoticeMentionsThatEffectsArePreviewsThatVanish() {
        const window = createMain(createFake([]), null, null, null, null, null, createLightingFake());
        const notice = findItem(window, "lightingNoticeLabel").text;
        verify(notice.indexOf("efeitos") >= 0 && notice.indexOf("prévia") >= 0, notice);
        // O SetEffect vai com persistência 01 (ADR-0013, R14): o aviso não pode prometer que nada é gravado nos efeitos.
        verify(notice.indexOf("Os efeitos também") < 0, notice);
    }

    function test_lightingEffectDropdownListsTheFourEffectsAndIsDisabledWithoutLiveOrWhileBusy() {
        const idle = createMain(createFake([]), null, null, null, null, null, createLightingFake());
        const combo = effectCombo(idle);
        compare(combo.count, 4);
        compare([0, 1, 2, 3].map(i => combo.textAt(i)).join(","), "Onda,Ciclo de cores,Respiração,Ondulação");
        compare(combo.enabled, false);
        const busy = createMain(createFake([]), null, null, null, null, null,
                                createLightingFake({live: true, busy: true}));
        compare(effectCombo(busy).count, 4);
        compare(effectCombo(busy).enabled, false);
    }

    function test_lightingEffectDropdownAppliesTheChosenKindWithTheChosenColor() {
        const lighting = createLightingFake({live: true});
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const combo = effectCombo(window);
        verify(combo.enabled);
        combo.activated(0);
        swatches(window)[2].clicked();
        combo.activated(2);
        combo.activated(3);
        combo.activated(1);
        compare(JSON.stringify(lighting.effectCalls),
                JSON.stringify([["wave", "#ffffff"], ["breathing", "#0000ff"], ["ripple", "#0000ff"],
                                ["cycle", "#0000ff"]]));
        compare(lighting.paintCalls.length, 1);
        compare(lighting.startCalls, 0);
    }

    function test_lightingSwitchStartsWhenOffAndShowsTheControllerState() {
        const lighting = createLightingFake();
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const toggle = findItem(window, "lightingLiveSwitch");
        compare(toggle.checked, false);
        toggle.clicked();
        compare(lighting.startCalls, 1);
        compare(lighting.stopCalls, 0);
        // O interruptor não muda sozinho: o estado exibido segue o controlador.
        compare(toggle.checked, false);
        lighting.live = true;
        tryCompare(toggle, "checked", true);
    }

    function test_lightingSwitchStopsWhenLive() {
        const lighting = createLightingFake({live: true});
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const toggle = findItem(window, "lightingLiveSwitch");
        compare(toggle.checked, true);
        toggle.clicked();
        compare(lighting.stopCalls, 1);
        compare(lighting.startCalls, 0);
    }

    function test_lightingSwitchDisabledWhileBusy() {
        const lighting = createLightingFake({busy: true});
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const toggle = findItem(window, "lightingLiveSwitch");
        compare(toggle.enabled, false);
        lighting.busy = false;
        tryVerify(() => toggle.enabled);
    }

    function test_lightingColorControlsDisabledWithoutLive() {
        const window = createMain(createFake([]), null, null, null, null, null, createLightingFake());
        compare(swatches(window).length, 8);
        swatches(window).forEach(s => compare(s.enabled, false));
        compare(findItem(window, "lightingCustomColorButton").enabled, false);
    }

    function test_lightingColorControlsDisabledWhileBusy() {
        const window = createMain(createFake([]), null, null, null, null, null,
                                  createLightingFake({live: true, busy: true}));
        swatches(window).forEach(s => compare(s.enabled, false));
        compare(findItem(window, "lightingCustomColorButton").enabled, false);
    }

    function test_lightingSwatchClickPaintsItsColor() {
        const lighting = createLightingFake({live: true});
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const all = swatches(window);
        compare(all.length, 8);
        all.forEach(s => verify(s.enabled));
        all[0].clicked();
        all[2].clicked();
        compare(lighting.paintCalls.length, 2);
        compare(lighting.paintCalls[0], "#ff0000");
        compare(lighting.paintCalls[1], "#0000ff");
        compare(lighting.startCalls, 0);
    }

    function test_lightingCustomColorButtonEnabledWithLive() {
        const lighting = createLightingFake({live: true});
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        verify(findItem(window, "lightingCustomColorButton").enabled);
        compare(lighting.paintCalls.length, 0);
    }

    function test_lightingColorsAreRoundSwatchesWithTheirNamesAndARingOnTheChosenOne() {
        const lighting = createLightingFake({live: true});
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const all = swatches(window);
        compare(all.map(s => s.Accessible.name).join(","),
                "Vermelho,Verde,Azul,Branco,Ciano,Magenta,Amarelo,Laranja");
        all.forEach(s => {
            compare(s.width, 20);
            compare(s.height, 20);
            compare(s.ToolTip.text, s.Accessible.name);
        });
        // O branco é a cor inicial dos efeitos; clicar em outra move o anel.
        compare(all.filter(s => s.selected).map(s => s.colorName), ["Branco"]);
        all[2].clicked();
        compare(all.filter(s => s.selected).map(s => s.colorName), ["Azul"]);
        const custom = findItem(window, "lightingCustomColorButton");
        compare(custom.plus, true);
        compare(custom.Accessible.name, "Outra cor…");
    }

    function test_lightingPanelHasColorsAndEffectsTabsThatSwapTheirContents() {
        const window = createMain(createFake([]), sampleInventory(), null, null, null, null,
                                  createLightingFake({live: true}));
        showMinimum(window);
        openDevice(window, "G915");
        const tabs = findItem(window, "lightingTabs");
        compare(tabs.model.join(","), "Cores,Efeitos");
        compare(tabs.currentIndex, 0);
        const colorsVisible = () => swatches(window)[0].visible;
        const effectsVisible = () => effectCombo(window).visible;
        tryVerify(() => colorsVisible());
        compare(effectsVisible(), false);
        const effectsTab = descendants(tabs).filter(i => i.objectName === "innerTab")[1];
        mouseClick(effectsTab);
        tryVerify(() => effectsVisible());
        compare(colorsVisible(), false);
        compare(findItem(window, "lightingEffectHeader").text, "Efeito");
    }

    function test_lightingStatusFollowsController() {
        const lighting = createLightingFake({status: "cor aplicada"});
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const label = findItem(window, "lightingStatusLabel");
        compare(label.text, "cor aplicada");
        lighting.status = "teclado ocupado: openrgb";
        tryCompare(label, "text", "teclado ocupado: openrgb");
    }

    function test_headsetSectionWarnsAndWritesNothingOnLoad() {
        const headset = createHeadsetFake();
        const window = createMainWithHeadset(headset);
        const notice = findItem(window, "headsetNoticeLabel");
        verify(notice.text.indexOf("não informa") >= 0, notice.text);
        // Teste físico de 2026-10-14: o sidetone fica gravado no headset; as luzes voltam ao padrão ao desligá-lo.
        verify(notice.text.indexOf("sidetone fica gravado") >= 0, notice.text);
        verify(notice.text.indexOf("luzes voltam ao padrão ao desligar o headset") >= 0, notice.text);
        verify(notice.text.indexOf("microfone") >= 0, notice.text);
        verify(notice.text.indexOf("ciclo") >= 0, notice.text);
        verify(notice.text.indexOf("HeadsetControl") < 0, notice.text);
        compare(headset.sidetoneCalls.length, 0);
        compare(headset.lightsCalls.length, 0);
    }

    function test_headsetSidetoneSliderCoversTheDeviceRangeAndWritesOnlyOnApply() {
        const headset = createHeadsetFake();
        const window = createMainWithHeadset(headset);
        const slider = findItem(window, "headsetSidetoneSlider");
        compare(slider.from, 0);
        compare(slider.to, 100);
        slider.value = 40;
        compare(headset.sidetoneCalls.length, 0);
        findItem(window, "headsetSidetoneApplyButton").clicked();
        compare(headset.sidetoneCalls, [40]);
    }

    function test_headsetLightsButtonsSendOnAndOff() {
        const headset = createHeadsetFake();
        const window = createMainWithHeadset(headset);
        findItem(window, "headsetLightsOnButton").clicked();
        findItem(window, "headsetLightsOffButton").clicked();
        compare(headset.lightsCalls, [true, false]);
        compare(headset.sidetoneCalls.length, 0);
    }

    function test_headsetControlsDisabledWhileBusy() {
        const headset = createHeadsetFake({busy: true});
        const window = createMainWithHeadset(headset);
        ["headsetSidetoneApplyButton", "headsetLightsOnButton", "headsetLightsOffButton"]
            .forEach(name => compare(findItem(window, name).enabled, false, name));
        headset.busy = false;
        tryVerify(() => findItem(window, "headsetSidetoneApplyButton").enabled);
    }

    function test_headsetShowsLastAppliedValuesAsSessionValues() {
        const headset = createHeadsetFake();
        const window = createMainWithHeadset(headset);
        const applied = findItem(window, "headsetAppliedLabel");
        verify(applied.text.indexOf("nenhum") >= 0, applied.text);
        headset.lastSidetone = 30;
        headset.lightsState = "off";
        tryVerify(() => applied.text.indexOf("30") >= 0 && applied.text.indexOf("desligadas") >= 0, 5000, applied.text);
        verify(applied.text.indexOf("nesta sessão") >= 0, applied.text);
    }

    function createMainWithLightingColor(fake) {
        return createMain(createFake([]), null, null, null, null, null, null, null, null, null, null, null, null,
                          null, null, null, null, null, fake);
    }

    function test_headsetColorSectionWarnsAndSendsNothingOnLoad() {
        const fake = createHeadsetLightingFake();
        const window = createMainWithLightingColor(fake);
        verify(findItem(window, "headsetLightingVolatileLabel").text.indexOf("volátil") >= 0);
        verify(findItem(window, "headsetLightingColorNoteLabel").text.indexOf("sem a correção") >= 0);
        compare(fake.applyCalls.length, 0);
    }

    function test_headsetColorApplyForwardsTheChosenValues() {
        const fake = createHeadsetLightingFake();
        const window = createMainWithLightingColor(fake);
        findItem(window, "headsetLightingZoneCombo").currentIndex = 1;
        findItem(window, "headsetLightingEffectCombo").currentIndex = 2;
        findItem(window, "headsetLightingRedField").value = 10;
        findItem(window, "headsetLightingGreenField").value = 20;
        findItem(window, "headsetLightingBlueField").value = 30;
        findItem(window, "headsetLightingPeriodField").value = 4500;
        findItem(window, "headsetLightingBrightnessSlider").value = 40;
        compare(fake.applyCalls.length, 0);
        findItem(window, "headsetLightingApplyButton").clicked();
        compare(fake.applyCalls, [[1, 2, 10, 20, 30, 4500, 40]]);
    }

    function test_headsetColorFieldsFollowTheEffect() {
        const fake = createHeadsetLightingFake();
        const window = createMainWithLightingColor(fake);
        openDeviceTab(window, "G733", "headset");
        const effect = findItem(window, "headsetLightingEffectCombo");
        const red = findItem(window, "headsetLightingRedField");
        const period = findItem(window, "headsetLightingPeriodField");
        effect.currentIndex = 0; // desligado
        verify(!red.visible);
        verify(!period.visible);
        effect.currentIndex = 1; // fixo: só cor
        tryVerify(() => red.visible);
        verify(!period.visible);
        effect.currentIndex = 2; // respiração: cor e tempo
        tryVerify(() => period.visible);
        verify(red.visible);
        effect.currentIndex = 3; // ciclo: só tempo
        tryVerify(() => !red.visible);
        verify(period.visible);
    }

    function test_headsetColorApplyIsDisabledWhileBusyAndShowsTheStatus() {
        const fake = createHeadsetLightingFake({busy: true, status: "Falha: headset desligado ou fora de alcance"});
        const window = createMainWithLightingColor(fake);
        verify(!findItem(window, "headsetLightingApplyButton").enabled);
        compare(findItem(window, "headsetLightingStatusLabel").text, "Falha: headset desligado ou fora de alcance");
        fake.busy = false;
        tryVerify(() => findItem(window, "headsetLightingApplyButton").enabled);
    }

    function test_headsetStatusFollowsController() {
        const headset = createHeadsetFake({status: "Sidetone aplicado: 40"});
        const window = createMainWithHeadset(headset);
        const label = findItem(window, "headsetStatusLabel");
        compare(label.text, "Sidetone aplicado: 40");
        headset.status = "Falha: em uso por solaar";
        tryCompare(label, "text", "Falha: em uso por solaar");
    }

    function test_eqSectionWarnsAndCallsNothingOnLoad() {
        const eq = createEqualizerFake();
        const window = createMainWithEqualizer(eq);
        const notice = findItem(window, "eqNoticeLabel");
        verify(notice.text.indexOf("Processado no PC pelo PipeWire, não no headset") >= 0, notice.text);
        verify(notice.text.indexOf("Vale só com o app aberto") >= 0, notice.text);
        verify(notice.text.indexOf("G733 + EQ") >= 0, notice.text);
        compare(eq.enableCalls + eq.disableCalls + eq.flatCalls, 0);
        compare(eq.bandCalls.length, 0);
    }

    function test_eqHasTenVerticalSlidersWithLabelsAndTheControllerRange() {
        const eq = createEqualizerFake();
        const window = createMainWithEqualizer(eq);
        const sliders = eqSliders(window);
        compare(sliders.length, 10);
        sliders.forEach(slider => {
            compare(slider.from, -12);
            compare(slider.to, 12);
            compare(slider.stepSize, 1);
            compare(slider.orientation, Qt.Vertical);
        });
        compare(labelTexts(window, "eqBandLabel"), eq.bandFrequencies);
    }

    function test_eqSlidersFollowControllerGainsWithoutSendingAnything() {
        const eq = createEqualizerFake();
        const window = createMainWithEqualizer(eq);
        eq.gains = [1, -2, 3, 0, 0, 0, 0, 0, 0, 12];
        tryCompare(eqSliders(window)[1], "value", -2);
        compare(eqSliders(window)[9].value, 12);
        compare(labelTexts(window, "eqBandValueLabel")[2], "+3");
        compare(labelTexts(window, "eqBandValueLabel")[1], "-2");
        compare(eq.bandCalls.length, 0);
    }

    function test_eqMovingASliderSendsThatBandOnly() {
        const eq = createEqualizerFake();
        const window = createMainWithEqualizer(eq);
        const slider = eqSliders(window)[4];
        slider.value = 7;
        compare(eq.bandCalls.length, 0); // mudar o valor sem o usuário mover não envia nada
        slider.moved();
        compare(eq.bandCalls, [[4, 7]]);
    }

    function test_eqSwitchAndResetButtonForwardToTheController() {
        const eq = createEqualizerFake();
        const window = createMainWithEqualizer(eq);
        const toggle = findItem(window, "eqEnableSwitch");
        compare(toggle.checked, false);
        toggle.clicked();
        compare(eq.enableCalls, 1);
        compare(eq.disableCalls, 0);
        eq.enabled = true;
        tryCompare(toggle, "checked", true);
        toggle.clicked();
        findItem(window, "eqFlatButton").clicked();
        compare(eq.enableCalls, 1);
        compare(eq.disableCalls, 1);
        compare(eq.flatCalls, 1);
    }

    function test_eqSwitchFollowsEnabledAndBusy() {
        const eq = createEqualizerFake();
        const window = createMainWithEqualizer(eq);
        const toggle = findItem(window, "eqEnableSwitch");
        compare(toggle.enabled, true);
        compare(findItem(window, "eqFlatButton").enabled, true);
        eq.enabled = true;
        tryCompare(toggle, "checked", true);
        eq.busy = true;
        tryCompare(toggle, "enabled", false);
        compare(findItem(window, "eqFlatButton").enabled, false);
    }

    // A troca da saída padrão de áudio é uma ação com efeito no sistema: o aviso curto fica visível no painel.
    function test_eqPanelKeepsAShortVisibleWarningAboutTheDefaultOutput() {
        const window = createMainWithEqualizer(createEqualizerFake());
        showMinimum(window);
        openDeviceTab(window, "G733", "equalizer");
        const warning = findItem(window, "eqOutputWarningLabel");
        verify(warning.text.indexOf("saída padrão") >= 0, warning.text);
        verify(warning.text.length < 80, warning.text);
        tryVerify(() => warning.visible && warning.width > 0);
    }

    function test_eqStatusFollowsController() {
        const eq = createEqualizerFake({status: "EQ ligado: saída padrão = G733 + EQ"});
        const window = createMainWithEqualizer(eq);
        const label = findItem(window, "eqStatusLabel");
        compare(label.text, "EQ ligado: saída padrão = G733 + EQ");
        eq.status = "EQ desligado: G733 removido";
        tryCompare(label, "text", "EQ desligado: G733 removido");
    }

    function test_noiseSectionWarnsAndCallsNothingOnLoad() {
        const noise = createNoiseSuppressionFake();
        const window = createMainWithNoiseSuppression(noise);
        const notice = findItem(window, "noiseNoticeLabel");
        verify(notice.text.indexOf("Processado no PC pelo RNNoise, não no headset") >= 0, notice.text);
        verify(notice.text.indexOf("não é o Blue VO!CE") >= 0, notice.text);
        verify(notice.text.indexOf("Vale só com o app aberto") >= 0, notice.text);
        verify(notice.text.indexOf("G733 + Redução de ruído") >= 0, notice.text);
        compare(noise.enableCalls + noise.disableCalls, 0);
        compare(noise.thresholdCalls.length, 0);
    }

    function test_noiseSliderUsesTheControllerRangeAndFollowsItWithoutSending() {
        const noise = createNoiseSuppressionFake();
        const window = createMainWithNoiseSuppression(noise);
        const slider = findItem(window, "noiseThresholdSlider");
        compare(slider.from, 0);
        compare(slider.to, 99);
        compare(slider.stepSize, 1);
        compare(slider.orientation, Qt.Horizontal);
        compare(slider.value, 50);
        noise.threshold = 80;
        tryCompare(slider, "value", 80);
        compare(findItem(window, "noiseThresholdLabel").text, "Sensibilidade: 80 %");
        compare(noise.thresholdCalls.length, 0);
    }

    function test_noiseMovingTheSliderSendsTheThreshold() {
        const noise = createNoiseSuppressionFake();
        const window = createMainWithNoiseSuppression(noise);
        const slider = findItem(window, "noiseThresholdSlider");
        slider.value = 33;
        compare(noise.thresholdCalls.length, 0); // mudar o valor sem o usuário mover não envia nada
        slider.moved();
        compare(noise.thresholdCalls, [33]);
    }

    function test_noiseSwitchForwardsToTheController() {
        const noise = createNoiseSuppressionFake();
        const window = createMainWithNoiseSuppression(noise);
        const toggle = findItem(window, "noiseEnableSwitch");
        compare(toggle.checked, false);
        toggle.clicked();
        compare(noise.enableCalls, 1);
        compare(noise.disableCalls, 0);
        noise.enabled = true;
        tryCompare(toggle, "checked", true);
        toggle.clicked();
        compare(noise.enableCalls, 1);
        compare(noise.disableCalls, 1);
    }

    function test_noiseSwitchFollowsEnabledAndBusy() {
        const noise = createNoiseSuppressionFake();
        const window = createMainWithNoiseSuppression(noise);
        const toggle = findItem(window, "noiseEnableSwitch");
        compare(toggle.enabled, true);
        noise.enabled = true;
        tryCompare(toggle, "checked", true);
        noise.busy = true;
        tryCompare(toggle, "enabled", false);
    }

    // A troca da entrada padrão de áudio é uma ação com efeito no sistema: o aviso curto fica visível no painel.
    function test_noisePanelKeepsAShortVisibleWarningAboutTheDefaultInput() {
        const window = createMainWithNoiseSuppression(createNoiseSuppressionFake());
        showMinimum(window);
        openDeviceTab(window, "G733", "microphone");
        const warning = findItem(window, "noiseInputWarningLabel");
        verify(warning.text.indexOf("entrada padrão") >= 0, warning.text);
        verify(warning.text.length < 80, warning.text);
        tryVerify(() => warning.visible && warning.width > 0);
    }

    function test_noiseStatusFollowsController() {
        const noise = createNoiseSuppressionFake({status: "Redução de ruído ligada: entrada padrão = G733 + Redução de ruído"});
        const window = createMainWithNoiseSuppression(noise);
        const label = findItem(window, "noiseStatusLabel");
        compare(label.text, "Redução de ruído ligada: entrada padrão = G733 + Redução de ruído");
        noise.status = "Redução de ruído desligada: G733 removido";
        tryCompare(label, "text", "Redução de ruído desligada: G733 removido");
    }

    function test_profilesSectionWarnsAndDoesNothingOnLoad() {
        const profiles = createProfilesFake({profileNames: ["A"]});
        const window = createMainWithProfiles(profiles);
        const notice = findItem(window, "profilesNoticeLabel");
        verify(notice.text.indexOf("Aplicar grava os controles da Brio 100") >= 0, notice.text);
        verify(notice.text.indexOf("não liga nem desliga o EQ") >= 0, notice.text);
        compare(findItem(window, "profilesLoadFailedLabel").visible, false);
        compare(profiles.saveCalls.length, 0);
        compare(profiles.applyCalls.length, 0);
        compare(profiles.removeCalls.length, 0);
    }

    function test_profilesLoadFailureShowsTheWarning() {
        const profiles = createProfilesFake({loadFailed: true});
        const window = createMainWithProfiles(profiles);
        clickButton(window, "profilesPageButton");
        const warning = findItem(window, "profilesLoadFailedLabel");
        compare(warning.visible, true);
        verify(warning.text.indexOf("Arquivo de perfis ilegível: nada será gravado até corrigi-lo.") >= 0, warning.text);
    }

    function test_profilesSaveWithNewNameCallsSaveCurrentWithoutReplace() {
        const profiles = createProfilesFake({profileNames: ["A"]});
        const window = createMainWithProfiles(profiles);
        findItem(window, "profileNameField").text = "Jogos";
        findItem(window, "profileSaveButton").clicked();
        compare(profiles.saveCalls, [["Jogos", false]]);
        compare(findDialog(window, "profileReplaceDialog").visible, false);
    }

    function test_profilesSaveWithExistingNameAsksBeforeReplacing() {
        const profiles = createProfilesFake({profileNames: ["A"]});
        const window = createMainWithProfiles(profiles);
        findItem(window, "profileNameField").text = "A";
        findItem(window, "profileSaveButton").clicked();
        compare(profiles.saveCalls.length, 0);
        const dialog = findDialog(window, "profileReplaceDialog");
        tryVerify(() => dialog.visible);
        dialog.accept();
        compare(profiles.saveCalls, [["A", true]]);
    }

    function test_profilesRejectingTheReplaceDialogSavesNothing() {
        const profiles = createProfilesFake({profileNames: ["A"]});
        const window = createMainWithProfiles(profiles);
        findItem(window, "profileNameField").text = "A";
        findItem(window, "profileSaveButton").clicked();
        const dialog = findDialog(window, "profileReplaceDialog");
        tryVerify(() => dialog.visible);
        dialog.reject();
        compare(profiles.saveCalls.length, 0);
    }

    function test_profilesListHasApplyAndRemovePerProfile() {
        const profiles = createProfilesFake({profileNames: ["A", "B"]});
        const window = createMainWithProfiles(profiles);
        const names = allItems(window).filter(item => item.objectName === "profileNameLabel").map(item => item.text);
        compare(names, ["A", "B"]);
        const applies = allItems(window).filter(item => item.objectName === "profileApplyButton");
        compare(applies.length, 2);
        applies[1].clicked();
        compare(profiles.applyCalls, ["B"]);
        compare(profiles.removeCalls.length, 0);
    }

    function test_profilesUpdateSavesCurrentStateIntoThatProfileAfterConfirmation() {
        const profiles = createProfilesFake({profileNames: ["A", "B"]});
        const window = createMainWithProfiles(profiles);
        const updates = allItems(window).filter(item => item.objectName === "profileUpdateButton");
        compare(updates.length, 2);
        updates[1].clicked();
        compare(profiles.saveCalls.length, 0);
        const dialog = findDialog(window, "profileUpdateDialog");
        tryVerify(() => dialog.visible);
        dialog.accept();
        compare(profiles.saveCalls, [["B", true]]);
    }

    function test_profilesRejectingTheUpdateDialogSavesNothing() {
        const profiles = createProfilesFake({profileNames: ["A"]});
        const window = createMainWithProfiles(profiles);
        allItems(window).filter(item => item.objectName === "profileUpdateButton")[0].clicked();
        const dialog = findDialog(window, "profileUpdateDialog");
        tryVerify(() => dialog.visible);
        dialog.reject();
        compare(profiles.saveCalls.length, 0);
    }

    function test_profilesRemoveAsksForConfirmation() {
        const profiles = createProfilesFake({profileNames: ["A", "B"]});
        const window = createMainWithProfiles(profiles);
        allItems(window).filter(item => item.objectName === "profileRemoveButton")[0].clicked();
        compare(profiles.removeCalls.length, 0);
        const dialog = findDialog(window, "profileRemoveDialog");
        tryVerify(() => dialog.visible);
        dialog.accept();
        compare(profiles.removeCalls, ["A"]);
    }

    function test_profilesRejectingTheRemoveDialogRemovesNothing() {
        const profiles = createProfilesFake({profileNames: ["A"]});
        const window = createMainWithProfiles(profiles);
        allItems(window).filter(item => item.objectName === "profileRemoveButton")[0].clicked();
        const dialog = findDialog(window, "profileRemoveDialog");
        tryVerify(() => dialog.visible);
        dialog.reject();
        compare(profiles.removeCalls.length, 0);
    }

    function test_profilesShowStatusAndReportLinesAndFollowChanges() {
        const profiles = createProfilesFake({status: "Perfil aplicado: A", report: ["Brightness: 128 aplicado"]});
        const window = createMainWithProfiles(profiles);
        compare(findItem(window, "profilesStatusLabel").text, "Perfil aplicado: A");
        compare(labelTexts(window, "profileReportLine"), ["Brightness: 128 aplicado"]);
        profiles.report = ["Gain: falhou — câmera em uso", "EQ: curva definida (vale com o EQ ligado)"];
        tryCompare(profiles, "report", ["Gain: falhou — câmera em uso", "EQ: curva definida (vale com o EQ ligado)"]);
        tryVerify(() => labelTexts(window, "profileReportLine").length === 2);
    }

    function test_profilesButtonsAreDisabledWhileBusy() {
        const profiles = createProfilesFake({profileNames: ["A"], busy: true});
        const window = createMainWithProfiles(profiles);
        ["profileSaveButton", "profileApplyButton", "profileRemoveButton"]
            .forEach(name => compare(findItem(window, name).enabled, false, name));
    }

    function test_profilesAutoSwitchIsOffByDefaultAndDoesNothingOnOpen() {
        const profiles = createProfilesFake({profileNames: ["A"]});
        const window = createMainWithProfiles(profiles);
        clickButton(window, "profilesPageButton");
        compare(findItem(window, "profilesAutoSwitch").checked, false);
        compare(profiles.switchingCalls.length, 0);
        compare(profiles.assignCalls.length, 0);
    }

    function test_profilesAutoSwitchCallsSetAutomaticSwitching() {
        const profiles = createProfilesFake({profileNames: ["A"]});
        const window = createMainWithProfiles(profiles);
        clickButton(window, "profilesPageButton");
        const toggle = findItem(window, "profilesAutoSwitch");
        toggle.click();
        compare(profiles.switchingCalls, [true]);
        // O estado exibido segue o controlador (a gravação pode falhar).
        profiles.automaticSwitching = true;
        tryCompare(toggle, "checked", true);
        toggle.click();
        compare(profiles.switchingCalls, [true, false]);
    }

    function test_profilesShowSwitchingStatusAndLastApplication() {
        const profiles = createProfilesFake({automaticSwitching: true, switchingStatus: "indisponível: KWin ausente",
                                             lastApplication: "org.mozilla.firefox"});
        const window = createMainWithProfiles(profiles);
        clickButton(window, "profilesPageButton");
        compare(findItem(window, "profilesSwitchingStatus").text, "Troca automática: indisponível: KWin ausente");
        compare(findItem(window, "profilesLastApplication").text, "Último aplicativo em foco: org.mozilla.firefox");
        profiles.switchingStatus = "ativa";
        profiles.lastApplication = "konsole";
        tryCompare(findItem(window, "profilesSwitchingStatus"), "text", "Troca automática: ativa");
        tryCompare(findItem(window, "profilesLastApplication"), "text", "Último aplicativo em foco: konsole");
    }

    function test_profilesLastApplicationShowsNoneWhenEmpty() {
        const window = createMainWithProfiles(createProfilesFake({}));
        clickButton(window, "profilesPageButton");
        compare(findItem(window, "profilesLastApplication").text, "Último aplicativo em foco: nenhum");
    }

    function test_profilesAssignButtonCallsAssignLastApplicationWithTheProfileName() {
        const profiles = createProfilesFake({profileNames: ["A", "B"], lastApplication: "konsole"});
        const window = createMainWithProfiles(profiles);
        clickButton(window, "profilesPageButton");
        const buttons = allItems(window).filter(item => item.objectName === "profileAssignButton");
        compare(buttons.length, 2);
        buttons[1].clicked();
        compare(profiles.assignCalls, ["B"]);
    }

    function test_profilesAssignButtonIsDisabledWithoutLastApplicationOrWhileBusy() {
        const profiles = createProfilesFake({profileNames: ["A"]});
        const window = createMainWithProfiles(profiles);
        clickButton(window, "profilesPageButton");
        compare(findItem(window, "profileAssignButton").enabled, false);
        profiles.lastApplication = "konsole";
        tryCompare(findItem(window, "profileAssignButton"), "enabled", true);
        profiles.busy = true;
        tryCompare(findItem(window, "profileAssignButton"), "enabled", false);
    }

    function test_profilesListAssociatedApplicationsWithRemoveButtons() {
        const profiles = createProfilesFake({profileNames: ["A", "B"],
                                             profileApplications: {"A": ["firefox", "kate"], "B": []}});
        const window = createMainWithProfiles(profiles);
        clickButton(window, "profilesPageButton");
        const labels = allItems(window).filter(item => item.objectName === "profileApplicationLabel").map(item => item.text);
        compare(labels, ["firefox", "kate"]);
        const removes = allItems(window).filter(item => item.objectName === "profileUnassignButton");
        compare(removes.length, 2);
        removes[1].clicked();
        compare(profiles.unassignCalls, ["kate"]);
        compare(profiles.removeCalls.length, 0);
    }

    function test_profilesAutoSwitchWarnsAboutTheKWinScript() {
        const window = createMainWithProfiles(createProfilesFake({}));
        clickButton(window, "profilesPageButton");
        const notice = findItem(window, "profilesAutoSwitchNotice");
        verify(notice.text.indexOf("pequeno script no KWin") >= 0);
        verify(notice.text.indexOf("só o nome do aplicativo") >= 0);
    }

    function test_profilesAutoSwitchControlsAreDisabledWhileBusy() {
        const profiles = createProfilesFake({profileNames: ["A"], busy: true,
                                             profileApplications: {"A": ["firefox"]}});
        const window = createMainWithProfiles(profiles);
        clickButton(window, "profilesPageButton");
        ["profilesAutoSwitch", "profileUnassignButton"]
            .forEach(name => compare(findItem(window, name).enabled, false, name));
    }

    // ---- INC-014: interface no estilo do G HUB (cartões, páginas e abas) ----

    function visibleTabKeys(window) {
        return allItems(window).filter(item => item.objectName === "sideTab" && item.visible && item.width > 0)
                               .map(item => item.tabKey);
    }

    function descendants(root) {
        const items = [];
        const pending = [root];
        while (pending.length > 0) {
            const item = pending.shift();
            items.push(item);
            for (let i = 0; i < item.children.length; ++i)
                pending.push(item.children[i]);
        }
        return items;
    }

    function sampleInventory() {
        return createFake([{name: "G915", summary: "detectado via receptor Lightspeed"},
                           {name: "G703", summary: "detectado via cabo USB"},
                           {name: "G733", summary: "não detectado"},
                           {name: "Brio 100", summary: "detectado via cabo USB"}]);
    }

    function test_homeShowsFourDeviceCardsInCatalogOrder() {
        const window = createMain(createFake([]), sampleInventory());
        compare(deviceCards(window).map(c => c.deviceName), ["G915", "G703", "G733", "Brio 100"]);
        compare(findItem(window, "pageStack").currentIndex, 0);
        compare(findItem(window, "backButton").visible, false);
    }

    function test_windowHasStartingAndMinimumSize() {
        const window = createMain(createFake([]));
        // INC-014 parte 3 (R25): o mínimo é o do G HUB (1024×768), então a janela inicial não pode ser menor que ele.
        compare(window.width, 1100);
        compare(window.height, 800);
        compare(window.minimumWidth, 1024);
        compare(window.minimumHeight, 768);
    }

    function test_cardShowsConnectionAndBattery() {
        const window = createMain(createFake([{name: "G915", summary: "44% · descarregando", percent: 44}]),
                                  sampleInventory());
        const card = deviceCards(window)[0];
        verify(card.connectionText.indexOf("receptor Lightspeed") >= 0, card.connectionText);
        compare(card.batteryText, "44% · descarregando");
        const bars = descendants(card).filter(item => item.objectName === "deviceCardBattery");
        compare(bars.length, 1);
        compare(bars[0].visible, true);
        compare(bars[0].value, 44);
        compare(card.opacity, 1);
    }

    function test_cardBatteryBarIsHidden_WhenPercentIsMinusOne() {
        const window = createMain(createFake([{name: "G915", summary: "estado desconhecido", percent: -1}]),
                                  sampleInventory());
        const bar = descendants(deviceCards(window)[0]).find(item => item.objectName === "deviceCardBattery");
        compare(bar.visible, false);
        // Brio 100 não tem item de bateria: sem texto nem barra.
        const brio = deviceCards(window)[3];
        compare(brio.batteryText, "");
        compare(descendants(brio).find(item => item.objectName === "deviceCardBattery").visible, false);
    }

    function test_absentDeviceCardIsDimmedButClickable() {
        const window = createMain(createFake([]), createFake([{name: "G915", summary: "detectado via cabo USB"}]));
        const cards = deviceCards(window);
        compare(cards[0].opacity, 1);
        compare(cards[1].connectionText, "não detectado");
        verify(cards[1].opacity < 1);
        verify(cards[3].opacity < 1);
        openDevice(window, "G703");
        compare(window.currentPage, 2);
    }

    function test_notDetectedSummaryDimsCard() {
        const window = createMain(createFake([]), sampleInventory());
        verify(deviceCards(window)[2].opacity < 1); // G733: "não detectado"
        compare(deviceCards(window)[3].opacity, 1);
    }

    function test_clickingCardOpensItsPageWithItsTabs() {
        const expected = {"G915": ["lighting", "info"], "G703": ["onboard", "ratbag", "info"],
                          "G733": ["headset", "equalizer", "microphone", "info"],
                          "Brio 100": ["image", "modes", "info"]};
        const pages = {"G915": 1, "G703": 2, "G733": 3, "Brio 100": 4};
        Object.keys(expected).forEach(name => {
            const window = createMain(createFake([]), sampleInventory());
            openDevice(window, name);
            compare(findItem(window, "pageStack").currentIndex, pages[name], name);
            compare(visibleTabKeys(window), expected[name], name);
            compare(findItem(window, "backButton").visible, true);
        });
    }

    function test_clickingTabShowsItsSectionOnly() {
        const window = createMain(createFake([]), sampleInventory());
        openDevice(window, "G733");
        const sidetone = findItem(window, "headsetSidetoneApplyButton");
        tryVerify(() => sidetone.visible);
        compare(findItem(window, "eqEnableSwitch").visible, false);
        openTab(window, "equalizer");
        tryVerify(() => findItem(window, "eqEnableSwitch").visible);
        compare(sidetone.visible, false);
        openTab(window, "microphone");
        tryVerify(() => findItem(window, "noiseEnableSwitch").visible);
        compare(findItem(window, "eqEnableSwitch").visible, false);
    }

    function test_g703TabsSeparateOnboardAndRatbag() {
        const window = createMain(createFake([]), sampleInventory());
        openDevice(window, "G703");
        tryVerify(() => findItem(window, "onboardReadButton").visible);
        compare(findItem(window, "pointerReadButton").visible, false);
        openTab(window, "ratbag");
        tryVerify(() => findItem(window, "pointerReadButton").visible);
        compare(findItem(window, "onboardReadButton").visible, false);
    }

    function test_backButtonReturnsHome() {
        const window = createMain(createFake([]), sampleInventory());
        openDevice(window, "G915");
        clickButton(window, "backButton");
        tryCompare(window, "currentPage", 0);
        compare(findItem(window, "backButton").visible, false);
    }

    function test_profilesAndSettingsButtonsOpenTheirPages() {
        const window = createMain(createFake([]), sampleInventory());
        clickButton(window, "profilesPageButton");
        compare(findItem(window, "pageStack").currentIndex, 5);
        tryVerify(() => findItem(window, "profileSaveButton").visible);
        clickButton(window, "settingsPageButton");
        compare(findItem(window, "pageStack").currentIndex, 6);
        tryVerify(() => findItem(window, "autostartSwitch").visible);
        tryVerify(() => findItem(window, "diagnosticsRefreshButton").visible);
        compare(findItem(window, "profileSaveButton").visible, false);
    }

    function test_infoTabShowsOnlyThatDevicesLabels() {
        const window = createMain(createFake([{name: "G915", summary: "44% · descarregando", percent: 44},
                                              {name: "G703", summary: "não detectado", percent: -1}]),
                                  sampleInventory());
        compare(labelTexts(window, "inventoryDeviceLabel").length, 4);
        openDeviceTab(window, "G703", "info");
        const visibleTexts = name => allItems(window).filter(i => i.objectName === name && i.visible)
                                                     .map(i => i.text);
        tryCompare(window, "currentPage", 2);
        tryVerify(() => visibleTexts("inventoryDeviceLabel").length === 1);
        compare(visibleTexts("inventoryDeviceLabel"), ["G703: detectado via cabo USB"]);
        compare(visibleTexts("batteryDeviceLabel"), ["G703: não detectado"]);
        compare(visibleTexts("batteryStateText"), ["não detectado"]);
    }

    // R7: o stream nunca fica ligado numa página invisível.
    function test_leavingBrioPageStopsPreview() {
        const preview = createPreviewFake();
        const window = createMain(createFake([]), sampleInventory(), null, preview);
        openDevice(window, "Brio 100");
        compare(preview.stopCalls, 0);
        clickButton(window, "backButton");
        tryCompare(preview, "stopCalls", 1);
    }

    function test_goingFromBrioToProfilesStopsPreview() {
        const preview = createPreviewFake();
        const window = createMain(createFake([]), sampleInventory(), null, preview);
        openDevice(window, "Brio 100");
        // A página do dispositivo não tem a aba Perfis (item 1 do pedido de estilo); a função de navegação continua
        // a parar a pré-visualização ao sair da Brio 100 para qualquer página.
        window.openPage(window.profilesPage);
        tryCompare(preview, "stopCalls", 1);
        compare(preview.toggleCalls, 0);
    }

    function test_navigatingElsewhereDoesNotStopPreview() {
        const preview = createPreviewFake();
        const window = createMain(createFake([]), sampleInventory(), null, preview);
        openDevice(window, "G915");
        clickButton(window, "backButton");
        clickButton(window, "settingsPageButton");
        compare(preview.stopCalls, 0);
    }

    // R8: aviso antes do botão de leitura do ratbagd.
    function test_ratbagTabWarnsAboutBlocking() {
        const window = createMain(createFake([]), sampleInventory());
        const warning = findItem(window, "ratbagBlockWarningLabel");
        verify(warning.text.indexOf("ratbagd") >= 0, warning.text);
        verify(warning.text.indexOf("20 minutos") >= 0, warning.text);
        const button = findItem(window, "pointerReadButton");
        verify(warning.parent === button.parent, "mesma coluna");
        const siblings = Array.from(warning.parent.children);
        verify(siblings.indexOf(warning) < siblings.indexOf(button), "aviso antes do botão");
    }

    // R9: o texto antigo era falso desde o INC-006 parte 2.
    function test_noLabelClaimsNothingIsWritten() {
        const window = createMain(createFake([]), sampleInventory());
        const offenders = allItems(window).filter(i => typeof i.text === "string"
                                                       && i.text.indexOf("Nenhuma configuração é gravada") >= 0);
        compare(offenders.length, 0);
    }

    // ---- INC-014 parte 2: tokens de design do G HUB (singleton Theme) ----

    function stateOverlayOf(item) {
        const found = descendants(item).filter(i => i.objectName === "stateOverlay");
        compare(found.length, 1, "stateOverlay");
        return found[0];
    }

    function test_themeExposesGHubTokens() {
        verify(Qt.colorEqual(Theme.primary, "#1196ff"), String(Theme.primary));
        // INC-014 parte 3 (R18): o fundo da janela é preto puro; o #101113 antigo vinha do CSS do rev/.
        verify(Qt.colorEqual(Theme.background, "#000000"), String(Theme.background));
        verify(Qt.colorEqual(Theme.surface, "#212225"), String(Theme.surface));
        verify(Qt.colorEqual(Theme.error, "#f54155"), String(Theme.error));
        verify(Qt.colorEqual(Theme.foreground, "#ffffff"), String(Theme.foreground));
        verify(Qt.colorEqual(Theme.outline, "#1affffff"), String(Theme.outline));
        verify(Qt.colorEqual(Theme.outlineHigh, "#4dffffff"), String(Theme.outlineHigh));
        verify(Qt.colorEqual(Theme.foregroundMedium, "#99ffffff"), String(Theme.foregroundMedium));
        verify(Qt.colorEqual(Theme.foregroundDisabled, "#4dffffff"), String(Theme.foregroundDisabled));
        compare(Theme.radius, 8);
        compare(Theme.hoverOpacity, 0.10);
        compare(Theme.pressOpacity, 0.15);
        compare(Theme.selectedOpacity, 0.15);
        verify(Theme.shortDuration >= 150 && Theme.shortDuration <= 250, String(Theme.shortDuration));
        verify(Theme.mediumDuration >= 150 && Theme.mediumDuration <= 250, String(Theme.mediumDuration));
        compare(Theme.easingCurve.length, 6);
    }

    // INC-014 parte 3 (R18): tokens medidos nas capturas próprias do G HUB (REFERENCIA-ESTILO-GHUB.md §1).
    function test_themeExposesLayoutTokens() {
        verify(Qt.colorEqual(Theme.divider, "#2b303b"), String(Theme.divider));
        verify(Qt.colorEqual(Theme.buttonSurface, "#393939"), String(Theme.buttonSurface));
        verify(Qt.colorEqual(Theme.sliderFill, "#afb1b4"), String(Theme.sliderFill));
        verify(Qt.colorEqual(Theme.sliderTrack, "#393939"), String(Theme.sliderTrack));
        verify(Qt.colorEqual(Theme.cardHover, "#2e2f31"), String(Theme.cardHover));
        verify(Qt.colorEqual(Theme.tabForeground, "#c8cdd7"), String(Theme.tabForeground));
        compare(Theme.cardRadius, 12);
    }

    function test_windowUsesThemeColors() {
        const window = createMain(createFake([]));
        verify(Qt.colorEqual(window.Material.accent, Theme.primary), String(window.Material.accent));
        verify(Qt.colorEqual(window.Material.background, Theme.background), String(window.Material.background));
        verify(Qt.colorEqual(window.Material.primary, Theme.surface), String(window.Material.primary));
        verify(Qt.colorEqual(window.Material.foreground, Theme.foreground), String(window.Material.foreground));
    }

    function test_profilesLoadFailedLabelUsesErrorColor() {
        const window = createMainWithProfiles(createProfilesFake({loadFailed: true}));
        const warning = findItem(window, "profilesLoadFailedLabel");
        verify(Qt.colorEqual(warning.color, Theme.error), String(warning.color));
    }

    // ---- INC-014 parte 3: layout do G HUB (R19 barra do topo) ----

    function test_topBarIs88pxBlackWithDividerAtTheBottomAndOwnBrand() {
        const window = createMain(createFake([]), sampleInventory());
        compare(window.header.height, 88);
        const bar = findItem(window, "topBar");
        compare(bar.height, 88);
        verify(Qt.colorEqual(bar.color, Theme.background), String(bar.color));
        const divider = findItem(window, "topBarDivider");
        compare(divider.height, 1);
        verify(Qt.colorEqual(divider.color, Theme.divider), String(divider.color));
        compare(divider.y + divider.height, bar.height);
        compare(divider.width, bar.width);
        compare(findItem(window, "brandLabel").text, "Hubless");
    }

    function test_topBarTabsMarkTheActivePageWithPrimaryAndA24x2Underline() {
        const window = createMain(createFake([]), sampleInventory());
        const devices = findItem(window, "devicesTabButton");
        const profiles = findItem(window, "profilesPageButton");
        compare(devices.text, "Dispositivos");
        compare(profiles.text, "Perfis");
        const underline = tab => descendants(tab).find(i => i.objectName === "tabUnderline");
        verify(Qt.colorEqual(devices.contentItem.color, Theme.primary), String(devices.contentItem.color));
        verify(Qt.colorEqual(profiles.contentItem.color, Theme.tabForeground), String(profiles.contentItem.color));
        compare(underline(devices).visible, true);
        compare(underline(devices).width, 24);
        compare(underline(devices).height, 2);
        verify(Qt.colorEqual(underline(devices).color, Theme.primary));
        compare(underline(profiles).visible, false);
        clickButton(window, "profilesPageButton");
        tryVerify(() => Qt.colorEqual(profiles.contentItem.color, Theme.primary));
        verify(Qt.colorEqual(devices.contentItem.color, Theme.tabForeground));
        compare(underline(profiles).visible, true);
        compare(underline(devices).visible, false);
        clickButton(window, "devicesTabButton");
        tryCompare(window, "currentPage", 0);
    }

    function test_topBarGearOpensSettingsAndIsA20pxThemeIcon() {
        const window = createMain(createFake([]), sampleInventory());
        const gear = findItem(window, "settingsPageButton");
        compare(gear.icon.name, "applications-system");
        compare(gear.icon.width, 20);
        compare(gear.icon.height, 20);
        compare(gear.text, "");
        compare(gear.Accessible.name, "Configurações");
        clickButton(window, "settingsPageButton");
        tryCompare(window, "currentPage", 6);
    }

    // ---- INC-014 parte 3 (R20): tela inicial com cartões de 296×450 ----

    // Mostra a janela no tamanho mínimo do G HUB (R25) e espera a primeira renderização.
    function showMinimum(window) {
        window.width = 1024;
        window.height = 768;
        window.show();
        waitForRendering(window.contentItem);
    }

    function test_cardsHaveG_HubSizeAndShape() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        const cards = deviceCards(window);
        compare(cards.map(c => c.width), [616, 296, 296, 296]);
        cards.forEach(c => {
            compare(c.height, 450, c.deviceName);
            compare(c.radius, Theme.cardRadius, c.deviceName);
            verify(Qt.colorEqual(c.color, Theme.surface), String(c.color));
        });
        const content = descendants(cards[1]).find(i => i.objectName === "cardContent");
        compare(content.x, 25);
        compare(content.y, 25);
        compare(content.width, 296 - 50);
        compare(content.height, 450 - 50);
    }

    function test_cardsFormACenteredGridWith24pxGaps() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        const cards = deviceCards(window);
        tryVerify(() => cards[0].visible && cards[0].width > 0);
        compare(cards[1].y, cards[0].y);
        compare(cards[1].x - (cards[0].x + cards[0].width), 24);
        compare(cards[2].y - (cards[0].y + cards[0].height), 24);
        compare(cards[3].x - (cards[2].x + cards[2].width), 24);
        // A grade ocupa 616 + 24 + 296 px e fica centrada na janela.
        const left = cards[0].mapToItem(null, 0, 0).x;
        const right = cards[1].mapToItem(null, cards[1].width, 0).x;
        compare(right - left, 616 + 24 + 296);
        verify(Math.abs((left + right) / 2 - window.width / 2) <= 1, left + " " + right);
    }

    function test_cardContentIsTitleBatteryRowAndCenteredDrawing() {
        const window = createMain(createFake([{name: "G915", summary: "63% · descarregando", percent: 63}]),
                                  sampleInventory());
        showMinimum(window);
        const [wide, common] = deviceCards(window);
        const title = descendants(wide).find(i => i.objectName === "cardTitle");
        compare(title.text, "G915");
        compare(title.font.pixelSize, 24);
        compare(title.font.bold, true);
        const row = descendants(wide).find(i => i.objectName === "cardBatteryRow");
        compare(row.visible, true);
        compare(descendants(row).find(i => i.objectName === "cardBatteryText").text, "63% · descarregando");
        compare(descendants(row).find(i => i.objectName === "deviceCardBattery").value, 63);
        // Sem bateria (Brio 100), a linha inteira some.
        const brio = deviceCards(window)[3];
        compare(descendants(brio).find(i => i.objectName === "cardBatteryRow").visible, false);
        // Área de imagem: 246×200 no cartão comum, 566×177 no largo; o desenho fica no centro dela.
        const area = card => descendants(card).find(i => i.objectName === "cardImageArea");
        compare(area(common).width, 246);
        compare(area(common).height, 200);
        compare(area(wide).width, 616 - 50);
        compare(area(wide).height, 177);
        [wide, common].forEach(card => {
            const drawing = descendants(area(card)).find(i => i.objectName === "deviceDrawing");
            verify(drawing !== undefined, "desenho do dispositivo");
            verify(Math.abs(drawing.x + drawing.width / 2 - area(card).width / 2) <= 1, "centro x");
            verify(Math.abs(drawing.y + drawing.height / 2 - area(card).height / 2) <= 1, "centro y");
            verify(drawing.width > 0 && drawing.height > 0);
        });
    }

    function test_cardHoverShowsWhiteOutlineAndHoverFill() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        const card = deviceCards(window)[1];
        tryVerify(() => card.visible && card.width > 0 && card.height > 0);
        compare(card.border.width, 0);
        verify(Qt.colorEqual(card.color, Theme.surface), String(card.color));
        mouseMove(card, 20, 20);
        tryVerify(() => Qt.colorEqual(card.color, Theme.cardHover));
        compare(card.border.width, 1);
        verify(Qt.colorEqual(card.border.color, Theme.foreground), String(card.border.color));
        mouseMove(card, -20, -20);
        tryVerify(() => Qt.colorEqual(card.color, Theme.surface));
        compare(card.border.width, 0);
    }

    // ---- INC-014 parte 3 (R21): cabeçalho da página e trilho de ícones ----

    function railButtons(window) {
        return allItems(window).filter(item => item.objectName === "sideTab" && item.visible && item.width > 0)
                              .sort((a, b) => a.mapToItem(null, 0, 0).y - b.mapToItem(null, 0, 0).y);
    }

    function test_deviceHeaderShowsPlainArrowAndTitle36pxBold() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        compare(findItem(window, "deviceTitle").visible, false);
        openDevice(window, "G703");
        const title = findItem(window, "deviceTitle");
        compare(title.visible, true);
        compare(title.text, "G703");
        compare(title.font.pixelSize, 36);
        compare(title.font.bold, true);
        const back = findItem(window, "backButton");
        compare(back.background, null);
        compare(back.contentItem.text, "←");
        compare(findItem(window, "brandLabel").visible, false);
    }

    function test_railHas48pxIconButtonsAt52pxStepWithTooltipsAndGearAtTheBottom() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        openDevice(window, "G733");
        const buttons = railButtons(window);
        compare(buttons.map(b => b.tabKey), ["headset", "equalizer", "microphone", "info"]);
        compare(buttons.map(b => b.ToolTip.text), ["Headset", "Equalizador", "Microfone", "Informações"]);
        buttons.forEach(b => {
            compare(b.width, 48, b.tabKey);
            compare(b.height, 48, b.tabKey);
            compare(b.icon.width, 24, b.tabKey);
            compare(b.icon.height, 24, b.tabKey);
            verify(b.icon.name !== "", "ícone de " + b.tabKey);
            compare(b.mapToItem(null, 0, 0).x, 13, b.tabKey);
        });
        const ys = buttons.map(b => b.mapToItem(null, 0, 0).y);
        compare(ys[1] - ys[0], 52);
        compare(ys[2] - ys[1], 52);
        // A engrenagem (Informações) fica no rodapé do trilho, afastada das demais.
        verify(ys[3] - ys[2] > 52, "engrenagem no rodapé: " + ys);
        const rail = visibleByName(window, "tabRail")[0];
        const bottom = buttons[3].mapToItem(rail, 0, 48).y;
        verify(bottom <= rail.height && bottom >= rail.height - 8, bottom + " de " + rail.height);
        compare(new Set(buttons.map(b => b.icon.name)).size, 4);
    }

    function test_railSelectedButtonIsPrimaryWithWhiteIconAndOthersAreDim() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        openDevice(window, "G915");
        const buttons = railButtons(window);
        const selected = buttons.find(t => t.selected);
        const other = buttons.find(t => !t.selected);
        compare(selected.tabKey, "lighting");
        verify(selected !== undefined && other !== undefined);
        verify(Qt.colorEqual(selected.icon.color, Theme.foreground), String(selected.icon.color));
        verify(Qt.colorEqual(other.icon.color, Theme.foregroundMedium), String(other.icon.color));
        const fill = descendants(selected).find(i => i.objectName === "selectedFill");
        verify(fill !== undefined, "selectedFill");
        compare(fill.radius, Theme.cardRadius);
        verify(Qt.colorEqual(fill.color, Theme.primary), String(fill.color));
        compare(fill.opacity, 1);
        compare(descendants(other).find(i => i.objectName === "selectedFill").opacity, 0);
        // Hover e clique seguem os overlays de estado da parte 2 (R15).
        const selectedOverlay = stateOverlayOf(selected);
        const overlay = stateOverlayOf(other);
        compare(overlay.opacity, 0);
        mouseMove(other, 20, 10);
        tryCompare(overlay, "opacity", Theme.hoverOpacity);
        mousePress(other, 20, 10);
        tryCompare(overlay, "opacity", Theme.pressOpacity);
        mouseRelease(other, 20, 10);
        // Soltar o clique abriu a aba Informações; o botão selecionado agora é outro.
        tryVerify(() => other.selected);
        mouseMove(selected, -100, 10);
        tryCompare(overlay, "opacity", 0);
        compare(selectedOverlay.opacity, 0);
    }

    // ---- INC-014 parte 3 (R22/R25): painel lateral de 310 px e área principal ----

    function test_devicePanelIs310pxWideWithTabTitleAndFillsTheHeight() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        openDevice(window, "G733");
        const panels = visibleByName(window, "devicePanel");
        compare(panels.length, 1);
        const panel = panels[0];
        compare(panel.width, 310);
        compare(panel.radius, Theme.cardRadius);
        verify(Qt.colorEqual(panel.color, Theme.surface), String(panel.color));
        const origin = panel.mapToItem(null, 0, 0);
        compare(origin.x, 75);
        compare(origin.y, 96);
        compare(origin.y + panel.height, window.height - 5);
        const title = descendants(panel).find(i => i.objectName === "panelTitle");
        compare(title.text, "Headset");
        compare(title.font.pixelSize, 24);
        openTab(window, "equalizer");
        compare(title.text, "Equalizador");
    }

    function test_mainAreaCentersTheDrawingAndPutsTheBatteryAtTheTopRight() {
        const window = createMain(createFake([{name: "G733", summary: "93% · descarregando", percent: 93}]),
                                  sampleInventory());
        showMinimum(window);
        openDevice(window, "G733");
        const area = visibleByName(window, "mainArea")[0];
        const origin = area.mapToItem(null, 0, 0);
        compare(origin.x, 400);
        compare(area.width, window.width - 400);
        const drawing = descendants(area).find(i => i.objectName === "deviceDrawing");
        compare(drawing.kind, "headset");
        verify(Math.abs(drawing.mapToItem(area, drawing.width / 2, 0).x - area.width / 2) <= 1, "centro x");
        verify(Math.abs(drawing.mapToItem(area, 0, drawing.height / 2).y - area.height / 2) <= 1, "centro y");
        // Como no G HUB: bloco alinhado à esquerda que começa 282 px antes da borda direita da área (item 8).
        const block = descendants(area).find(i => i.objectName === "mainBatteryBlock");
        compare(block.visible, true);
        compare(block.mapToItem(area, 0, 0).x, area.width - 282);
        verify(block.mapToItem(area, block.width, 0).x <= area.width, "dentro da área");
        verify(block.mapToItem(area, 0, 0).y <= 40, "no topo");
        const part = name => descendants(block).find(i => i.objectName === name);
        compare(part("batteryHeader").text, "Nível de bateria");
        compare(part("batteryPercentText").text, "93%");
        compare(part("batteryPercentText").font.pixelSize, 18);
        compare(part("batteryStateText").text, "descarregando");
        compare(part("batteryIcon").value, 93);
        verify(part("batteryIcon").mapToItem(block, 0, 0).x >= part("batteryPercentText").width, "ícone à direita do número");
    }

    function test_mainAreaHasNoBatteryBlockForTheBrio100() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        openDevice(window, "Brio 100");
        const area = visibleByName(window, "mainArea")[0];
        compare(descendants(area).find(i => i.objectName === "deviceDrawing").kind, "camera");
        compare(descendants(area).find(i => i.objectName === "mainBatteryBlock").visible, false);
    }

    // Nenhum item visível do painel passa da borda direita do painel (o conteúdo longo rola na vertical).
    function allVisibleDescendants(root) {
        const items = [];
        const pending = [root];
        while (pending.length > 0) {
            const item = pending.shift();
            if (!item.visible)
                continue;
            items.push(item);
            for (let i = 0; i < item.children.length; ++i)
                pending.push(item.children[i]);
            if (item.contentItem && item.contentItem !== item && !pending.includes(item.contentItem))
                pending.push(item.contentItem);
        }
        return items;
    }

    function richCamera() {
        return createCameraFake("Brio 100", [{text: "MJPG: até 1920×1080 a 30 fps"}],
                                [integerControl({id: 1}), booleanControl({id: 2}), menuControl({id: 3})]);
    }

    function test_noTabOverflowsThePanelAt1024x768() {
        const onboard = createOnboardFake({loaded: true, dpiStages: [400, 800, 1600, 3200, 6400], defaultStage: 1,
                                           reportRateHz: 1000, backupExists: true, status: "lido"});
        const pointer = createPointerFake({statusText: "lido", deviceName: "G703", connectionText: "USB",
                                           profiles: [pointerProfile()], warnings: ["aviso longo do ratbagd sobre bloqueio"]});
        const tabsByDevice = {"G915": ["lighting", "info"], "G703": ["onboard", "ratbag", "info"],
                              "G733": ["headset", "equalizer", "microphone", "info"], "Brio 100": ["image", "modes", "info"]};
        const window = createMain(createFake([{name: "G915", summary: "44% · descarregando", percent: 44}]),
                                  sampleInventory(), richCamera(), null, null, pointer, null, null, null, onboard);
        showMinimum(window);
        for (const name of Object.keys(tabsByDevice)) {
            openDevice(window, name);
            for (const key of tabsByDevice[name]) {
                openTab(window, key);
                if (key === "info") {
                    // A página Informações não tem painel: as três colunas cabem na largura da janela.
                    const page = visibleByName(window, "infoPage")[0];
                    const outside = allVisibleDescendants(page).filter(item => item.objectName !== "" && item.width > 0
                        && item.height > 0 && item.mapToItem(null, item.width, 0).x > window.width + 1)
                        .map(item => item.objectName + " " + item.mapToItem(null, item.width, 0).x);
                    compare(outside, [], name + "/info");
                    compare(visibleByName(window, "devicePanel").length, 0, name + "/info sem painel");
                    continue;
                }
                const panel = visibleByName(window, "devicePanel")[0];
                // Só itens com objectName (controles e rótulos do app); os internos do estilo Material (sombra,
                // ondulação) passam da borda de propósito.
                const bad = allVisibleDescendants(panel).filter(item => item.objectName !== "" && item.width > 0
                    && item.height > 0 && item.mapToItem(panel, item.width, 0).x > panel.width + 1)
                    .map(item => item.objectName + " " + item.mapToItem(panel, item.width, 0).x);
                compare(bad, [], name + "/" + key);
            }
            clickButton(window, "backButton");
            tryCompare(window, "currentPage", 0);
        }
    }

    // ---- INC-014 parte 3 (R23): controles no estilo do G HUB ----

    function test_sectionTitlesAreUppercase13pxWithSpacingAndDimWhite() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        openDeviceTab(window, "G733", "headset");
        const titles = visibleByName(window, "sectionTitle");
        verify(titles.length > 0, "há cabeçalhos de seção");
        titles.forEach(t => {
            compare(t.font.pixelSize, 13);
            compare(t.font.letterSpacing, 0.5);
            compare(t.font.capitalization, Font.AllUppercase);
            verify(Qt.colorEqual(t.color, Theme.foregroundMedium), String(t.color));
        });
        // Na página Informações os cabeçalhos usam #afb1b4 (referência §5).
        openTab(window, "info");
        const infoTitles = visibleByName(window, "sectionTitle");
        verify(infoTitles.length > 0, "há cabeçalhos na página Informações");
        infoTitles.forEach(t => {
            compare(t.font.pixelSize, 13);
            compare(t.font.capitalization, Font.AllUppercase);
            verify(Qt.colorEqual(t.color, Theme.sliderFill), String(t.color));
        });
    }

    function test_horizontalSliderHas2pxTrackAndWhite12pxThumb() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        openDeviceTab(window, "G733", "headset");
        const slider = findItem(window, "headsetSidetoneSlider");
        const track = descendants(slider).find(i => i.objectName === "sliderTrack");
        const fill = descendants(slider).find(i => i.objectName === "sliderFill");
        compare(track.height, 2);
        verify(Qt.colorEqual(track.color, Theme.sliderTrack), String(track.color));
        compare(fill.height, 2);
        verify(Qt.colorEqual(fill.color, Theme.sliderFill), String(fill.color));
        compare(slider.handle.width, 12);
        compare(slider.handle.height, 12);
        compare(slider.handle.radius, 6);
        verify(Qt.colorEqual(slider.handle.color, Theme.foreground), String(slider.handle.color));
        // O trecho cheio acompanha o valor: no meio do curso, ocupa a metade do trilho.
        slider.value = 50;
        verify(Math.abs(fill.width - track.width / 2) <= 1, fill.width + " de " + track.width);
    }

    function test_verticalSliderHas2pxWideTrack() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        openDeviceTab(window, "G733", "equalizer");
        const slider = eqSliders(window)[0];
        const track = descendants(slider).find(i => i.objectName === "sliderTrack");
        compare(track.width, 2);
        compare(slider.handle.width, 12);
        compare(slider.handle.height, 12);
    }

    function test_checkBoxIs14pxWith2pxBorderAndFillsWhenChecked() {
        const desktop = createDesktopFake({autostartEnabled: true});
        const window = createMainWithDesktop(desktop);
        showMinimum(window);
        clickButton(window, "settingsPageButton");
        const box = findItem(window, "autostartSwitch");
        tryVerify(() => box.visible && box.width > 0);
        compare(box.indicator.width, 14);
        compare(box.indicator.height, 14);
        compare(box.indicator.border.width, 2);
        compare(box.indicator.radius, 0);
        verify(Qt.colorEqual(box.indicator.border.color, Theme.sliderFill), String(box.indicator.border.color));
        compare(box.checked, true);
        verify(Qt.colorEqual(box.indicator.color, Theme.sliderFill), String(box.indicator.color));
        desktop.autostartEnabled = false;
        tryVerify(() => !box.checked);
        compare(box.indicator.color.a, 0);
    }

    function test_buttonsAreFlat32pxSurfaceWithUppercaseBoldText() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        const check = (name) => {
            const button = findItem(window, name);
            compare(button.height, 32, name);
            compare(button.background.radius, 0, name);
            verify(Qt.colorEqual(button.background.color, button.enabled ? Theme.buttonSurface : Theme.buttonDisabledSurface),
                   name + " " + button.background.color);
            compare(button.contentItem.font.bold, true, name);
            compare(button.contentItem.font.capitalization, Font.AllUppercase, name);
            verify(Qt.colorEqual(button.contentItem.color, button.enabled ? Theme.foreground : Theme.buttonDisabledText),
                   name + " " + button.contentItem.color);
        };
        openDeviceTab(window, "G703", "onboard");
        check("onboardReadButton");
        compare(findItem(window, "onboardReadButton").width, 278); // largura toda do conteúdo do painel
        clickButton(window, "backButton");
        openDeviceTab(window, "G733", "headset");
        check("headsetLightsOnButton");
        compare(findItem(window, "headsetLightsOnButton").width, 278);
        clickButton(window, "backButton");
        clickButton(window, "profilesPageButton");
        check("profileSaveButton");
        clickButton(window, "settingsPageButton");
        check("diagnosticsRefreshButton");
    }

    // ---- Estilo mais fiel ao G HUB (itens 1 a 12 do pedido de 2026-10-15) ----

    // Item 1: na página do dispositivo o cabeçalho tem só a seta e o título.
    function test_deviceHeaderHasOnlyTheArrowAndTheTitle() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        ["devicesTabButton", "profilesPageButton", "settingsPageButton", "topBarDivider"]
            .forEach(name => compare(findItem(window, name).visible, true, name + " na tela inicial"));
        openDevice(window, "G915");
        ["devicesTabButton", "profilesPageButton", "settingsPageButton", "topBarDivider"]
            .forEach(name => compare(findItem(window, name).visible, false, name + " na página do dispositivo"));
        const back = findItem(window, "backButton");
        const arrow = back.mapToItem(null, back.width / 2, back.height / 2);
        verify(Math.abs(arrow.x - 35.5) <= 3, "seta em x≈25..46: " + arrow.x);
        verify(Math.abs(arrow.y - 56) <= 3, "seta em y≈56: " + arrow.y);
        compare(findItem(window, "deviceTitle").mapToItem(null, 0, 0).x, 81);
        // Nas páginas Perfis e Configurações as abas e a engrenagem continuam.
        clickButton(window, "backButton");
        clickButton(window, "profilesPageButton");
        compare(findItem(window, "devicesTabButton").visible, true);
        compare(findItem(window, "settingsPageButton").visible, true);
        compare(findItem(window, "topBarDivider").visible, true);
    }

    // Item 2: os parágrafos longos viram o texto de um "?" ao lado do cabeçalho; nada é apagado.
    function test_longNoticesLiveInHelpIcons() {
        const window = createMain(createFake([]), sampleInventory());
        ["lightingNoticeLabel", "onboardNoticeLabel", "pointerNoticeLabel", "headsetNoticeLabel", "eqNoticeLabel",
         "noiseNoticeLabel"].forEach(name => {
            const icon = findItem(window, name);
            verify(icon.tip !== undefined, name + " é um ícone de ajuda");
            compare(icon.width, 20, name);
            verify(icon.text.length > 60, name + ": " + icon.text);
        });
    }

    function test_noPanelShowsALongParagraphOutsideTheHelpIcons() {
        const onboard = createOnboardFake({loaded: true, dpiStages: [400, 800], defaultStage: 1, reportRateHz: 1000,
                                           backupExists: true, status: "lido"});
        const window = createMain(createFake([]), sampleInventory(), richCamera(), null, null, null, null, null, null,
                                  onboard);
        showMinimum(window);
        const tabsByDevice = {"G915": ["lighting"], "G703": ["onboard", "ratbag"],
                              "G733": ["headset", "equalizer", "microphone"], "Brio 100": ["image", "modes"]};
        for (const name of Object.keys(tabsByDevice)) {
            openDevice(window, name);
            for (const key of tabsByDevice[name]) {
                openTab(window, key);
                const panel = visibleByName(window, "devicePanel")[0];
                const longTexts = allVisibleDescendants(panel).filter(item => typeof item.text === "string"
                    && item.tip === undefined && !/Status|Applied/.test(item.objectName)
                    && item.text.length > 90).map(item => item.objectName + ": " + item.text.substring(0, 40));
                compare(longTexts, [], name + "/" + key);
            }
            clickButton(window, "backButton");
            tryCompare(window, "currentPage", 0);
        }
    }

    // Exceção de segurança do item 2: ações que gravam no dispositivo ou trocam a saída/entrada de áudio mantêm uma
    // linha curta visível.
    function test_onboardPanelKeepsAShortVisibleWriteWarning() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        openDevice(window, "G703");
        const warning = findItem(window, "onboardWriteWarningLabel");
        compare(warning.text, "Grava no perfil ativo do mouse.");
        tryVerify(() => warning.visible && warning.width > 0);
    }

    function test_ratbagWarningIsOneShortVisibleLineThatStillNamesTheBlocking() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        openDeviceTab(window, "G703", "ratbag");
        const warning = findItem(window, "ratbagBlockWarningLabel");
        tryVerify(() => warning.visible && warning.width > 0);
        verify(warning.text.length < 110, warning.text);
        const notice = findItem(window, "pointerNoticeLabel");
        // O texto longo (com a menção à memória do mouse) vai inteiro para o balão.
        verify(notice.text.indexOf("memória do mouse") >= 0, notice.text);
        verify(notice.text.indexOf("ratbagd") >= 0, notice.text);
    }

    // Item 4/5: iluminação do G915 com abas CORES/EFEITOS e "+" para outra cor.
    function test_lightingCustomColorPlusOpensTheSameColorDialogOnlyWhenEnabled() {
        const lighting = createLightingFake({live: true});
        const window = createMain(createFake([]), sampleInventory(), null, null, null, null, lighting);
        showMinimum(window);
        openDevice(window, "G915");
        const plus = findItem(window, "lightingCustomColorButton");
        tryVerify(() => plus.visible && plus.width > 0);
        compare(plus.enabled, true);
        compare(lighting.paintCalls.length, 0);
    }

    // Item 7: controles do G703 próprios; as ações são as mesmas de antes.
    function test_onboardEditorUsesOwnComboRadiosAndSpinBoxesAndForwardsTheEditedValues() {
        const onboard = createOnboardFake({loaded: true, dpiStages: [7050, 1600], defaultStage: 1, reportRateHz: 500,
                                           backupExists: true});
        const window = createMainWithOnboard(onboard);
        showMinimum(window);
        openDevice(window, "G703");
        const rate = findItem(window, "onboardRateBox");
        tryVerify(() => rate.visible && rate.width > 0);
        verify(rate.popup !== undefined && rate.placeholderText !== undefined, "GHubComboBox");
        compare(rate.currentText, "500");
        rate.activated(3);
        const radios = allItems(window).filter(i => i.objectName === "onboardDefaultRadio");
        compare(radios.map(r => r.checked), [false, true]);
        verify(radios[0].indicator.objectName === "radioRing", "GHubRadioButton");
        radios[0].clicked();
        const fields = allItems(window).filter(i => i.objectName === "onboardDpiField");
        compare(fields.length, 2);
        const plus = descendants(fields[0]).find(i => i.objectName === "spinPlus");
        verify(plus !== undefined, "GHubSpinBox");
        reveal(fields[0]);
        waitForRendering(window.contentItem);
        mouseClick(plus);
        tryCompare(fields[0], "value", 7100);
        findItem(window, "onboardWriteButton").clicked();
        compare(JSON.stringify(onboard.writeCalls), JSON.stringify([[1000, [7100, 1600], 0]]));
    }

    // Item 6/7: sidetone com o valor à direita e os extremos; Brio com controles próprios.
    function test_headsetSidetoneSliderShowsItsValueAndExtremes() {
        const window = createMain(createFake([]), sampleInventory());
        const slider = findItem(window, "headsetSidetoneSlider");
        compare(slider.showValue, true);
        compare(slider.showExtremes, true);
        slider.value = 40;
        compare(descendants(slider).find(i => i.objectName === "sliderValue").text, "40");
    }

    function test_brioControlsUseOwnSliderSwitchAndDropdown() {
        const window = createMain(createFake([]), null, richCamera());
        const slider = findItem(window, "cameraControlSlider");
        compare(slider.showValue, true);
        compare(slider.showExtremes, true);
        const toggle = findItem(window, "cameraControlSwitch");
        compare(toggle.width, 54);
        compare(toggle.height, 24);
        verify(toggle.toggleRequested !== undefined, "GHubSwitch");
        const combo = findItem(window, "cameraControlCombo");
        verify(combo.placeholderText !== undefined, "GHubComboBox");
    }

    // Item 9: as 10 bandas ficam na área principal; o painel guarda o título, o interruptor, o "?" e o estado.
    function test_equalizerBandsLiveInTheMainAreaReplacingTheDrawing() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        openDeviceTab(window, "G733", "equalizer");
        const area = visibleByName(window, "mainArea")[0];
        compare(descendants(area).filter(i => i.objectName === "eqBandSlider").length, 10);
        compare(descendants(area).find(i => i.objectName === "deviceDrawing").visible, false);
        const panel = visibleByName(window, "devicePanel")[0];
        compare(descendants(panel).filter(i => i.objectName === "eqBandSlider").length, 0);
        verify(descendants(panel).some(i => i.objectName === "eqEnableSwitch"), "interruptor no painel");
        verify(descendants(panel).some(i => i.objectName === "eqStatusLabel"), "estado no painel");
        // Nas outras abas o desenho do headset volta.
        openTab(window, "headset");
        compare(descendants(visibleByName(window, "mainArea")[0]).find(i => i.objectName === "deviceDrawing").visible, true);
    }

    // Item 10: Informações em 3 colunas, sem painel; o trilho continua.
    function test_infoPageHasThreeColumnsWithoutThePanel() {
        const window = createMain(createFake([{name: "G733", summary: "93% · descarregando", percent: 93}]),
                                  sampleInventory());
        showMinimum(window);
        openDeviceTab(window, "G733", "info");
        const page = visibleByName(window, "infoPage")[0];
        verify(page !== undefined, "página Informações visível");
        compare(visibleByName(window, "devicePanel").length, 0);
        compare(visibleByName(window, "mainArea").length, 0);
        compare(railButtons(window).length, 4);
        const column = name => descendants(page).find(i => i.objectName === name);
        const left = column("infoLeftColumn");
        const center = column("infoCenterColumn");
        const right = column("infoRightColumn");
        [left, center, right].forEach(c => compare(c.width, 250, c.objectName));
        const xs = [left, center, right].map(c => c.mapToItem(null, 0, 0).x);
        compare(xs[1] - xs[0], 320);
        compare(xs[2] - xs[1], 318);
        verify(xs[0] >= 61, "a primeira coluna começa depois do trilho: " + xs[0]);
        // Esquerda: bateria e conexão. Centro: desenho. Direita: identidade, alinhada à esquerda da coluna.
        verify(descendants(left).some(i => i.objectName === "batteryHeader"), "bateria na esquerda");
        verify(descendants(left).some(i => i.objectName === "inventoryDeviceLabel"), "conexão na esquerda");
        compare(descendants(center).find(i => i.objectName === "deviceDrawing").kind, "headset");
        const read = descendants(right).find(i => i.objectName === "readIdentityButton");
        compare(read.mapToItem(null, 0, 0).x, xs[2]);
        compare(read.width, 250);
        compare(descendants(right).find(i => i.objectName === "sectionTitle").text, "Identidade (HID++)");
    }

    // ---- Estado HID++ na página Informações (G915, G703, G733) ----

    function createMainWithStatus(keyboard, pointer, headset) {
        return createMain(createFake([]), sampleInventory(), null, null, null, null, null, null, null, null, null, null,
                          null, null, null, keyboard, pointer, headset);
    }

    function statusButton(window) {
        return visibleByName(window, "readStatusButton")[0];
    }

    function test_statusSectionReadsOnlyOnClickAndNothingOnOpen() {
        const fakes3 = {"G915": createStatusFake(), "G703": createStatusFake(), "G733": createStatusFake()};
        const window = createMainWithStatus(fakes3["G915"], fakes3["G703"], fakes3["G733"]);
        for (const name of Object.keys(fakes3)) {
            openDeviceTab(window, name, "info");
            tryVerify(() => statusButton(window) !== undefined);
            compare(fakes3[name].readCalls, 0, name + " nada lido ao abrir");
            compare(statusButton(window).text, "Ler estado");
            mouseClick(statusButton(window));
            compare(fakes3[name].readCalls, 1, name);
            const header = visibleByName(window, "sectionTitle").find(t => t.text === "Estado (HID++)");
            verify(header !== undefined, name + " cabeçalho");
            clickButton(window, "backButton");
        }
    }

    function test_statusHelpIconExplainsOnDemandReadingAndUninterpretedBytes() {
        const window = createMainWithStatus(createStatusFake(), createStatusFake(), createStatusFake());
        openDeviceTab(window, "G915", "info");
        const help = visibleByName(window, "statusHelpIcon")[0];
        verify(help !== undefined);
        verify(help.text.indexOf("só quando você clica") >= 0, help.text);
        verify(help.text.indexOf("não interpretado") >= 0, help.text);
    }

    function test_statusButtonIsDisabledWhileReading() {
        const status = createStatusFake({reading: true});
        const window = createMainWithStatus(status, createStatusFake(), createStatusFake());
        openDeviceTab(window, "G915", "info");
        tryVerify(() => statusButton(window) !== undefined);
        compare(statusButton(window).enabled, false);
        status.reading = false;
        tryCompare(statusButton(window), "enabled", true);
    }

    function statusRows(window) {
        const labels = visibleByName(window, "statusRowLabel").map(i => i.text);
        const values = visibleByName(window, "statusRowValue").map(i => i.text);
        return labels.map((l, i) => l + "=" + values[i]);
    }

    function test_statusShowsTheValuesOfTheDevice() {
        const keyboard = createStatusFake({status: {brightness: "100 de 100", brightnessUnexplained: "05 03 (não interpretado)",
                                                    reportRate: "1000 Hz (1 ms)", supportedReportRates: "125, 250, 500, 1000"}});
        const pointer = createStatusFake({status: {dpiRange: "100 a 25600", dpi: "7050 (padrão 800)",
                                                   reportRate: "1000 Hz (1 ms)", supportedReportRates: "125, 250, 500, 1000"}});
        const headset = createStatusFake({status: {equalizer: "10 bandas", equalizerUnexplained: "",
                                                   uninterpreted: "00 00 00 00 (não interpretado)"}});
        const window = createMainWithStatus(keyboard, pointer, headset);
        openDeviceTab(window, "G915", "info");
        tryVerify(() => statusRows(window).length === 4);
        compare(statusRows(window), ["Brilho=100 de 100", "Report rate=1000 Hz (1 ms)",
                                     "Taxas suportadas=125, 250, 500, 1000", "Não interpretado=05 03 (não interpretado)"]);
        const value = visibleByName(window, "statusRowValue")[0];
        compare(value.font.bold, true);
        verify(Qt.colorEqual(value.color, Theme.foreground));
        verify(Qt.colorEqual(visibleByName(window, "statusRowLabel")[0].color, "#888888"));
        clickButton(window, "backButton");
        openDeviceTab(window, "G703", "info");
        tryVerify(() => statusRows(window).length === 4);
        compare(statusRows(window), ["DPI=7050 (padrão 800)", "Faixa de DPI=100 a 25600", "Report rate=1000 Hz (1 ms)",
                                     "Taxas suportadas=125, 250, 500, 1000"]);
        clickButton(window, "backButton");
        openDeviceTab(window, "G733", "info");
        tryVerify(() => statusRows(window).length === 2);
        compare(statusRows(window), ["Equalizador=10 bandas", "Sidetone (0x8300)=00 00 00 00 (não interpretado)"]);
    }

    function test_statusShowsTheErrorOnlyWhenThereIsOne() {
        const keyboard = createStatusFake();
        const window = createMainWithStatus(keyboard, createStatusFake(), createStatusFake());
        openDeviceTab(window, "G915", "info");
        tryVerify(() => statusButton(window) !== undefined);
        compare(visibleByName(window, "statusErrorLabel").length, 0);
        keyboard.error = "teclado ocupado";
        tryVerify(() => visibleByName(window, "statusErrorLabel").length === 1);
        compare(visibleByName(window, "statusErrorLabel")[0].text, "teclado ocupado");
    }

    function test_brioInfoHasNoStatusSection() {
        const window = createMainWithStatus(createStatusFake(), createStatusFake(), createStatusFake());
        openDeviceTab(window, "Brio 100", "info");
        tryVerify(() => visibleByName(window, "inventoryDeviceLabel").length === 1);
        compare(visibleByName(window, "readStatusButton").length, 0);
        compare(visibleByName(window, "statusHelpIcon").length, 0);
    }

    // G703: cabeçalho de seção para a taxa de relatório e estágios em maiúsculas secundárias.
    function test_onboardReportRateHasASectionHeaderAndStagesAreUppercaseSecondary() {
        const onboard = createOnboardFake({loaded: true, dpiStages: [400, 800], defaultStage: 1, reportRateHz: 1000});
        const window = createMainWithOnboard(onboard);
        showMinimum(window);
        openDevice(window, "G703");
        const rate = findItem(window, "onboardRateBox");
        tryVerify(() => rate.visible && rate.width > 0);
        const header = visibleByName(window, "sectionTitle").find(t => t.text === "Report rate (por segundo)");
        verify(header !== undefined, "cabeçalho REPORT RATE");
        compare(header.font.capitalization, Font.AllUppercase);
        verify(rate.mapToItem(null, 0, 0).y > header.mapToItem(null, 0, 0).y, "dropdown abaixo do cabeçalho");
        const stages = visibleByName(window, "onboardStageLabel");
        compare(stages.map(l => l.text), ["Estágio 1", "Estágio 2"]);
        stages.forEach(l => {
            compare(l.font.capitalization, Font.AllUppercase);
            verify(l.font.pixelSize >= 12 && l.font.pixelSize <= 13);
            verify(Qt.colorEqual(l.color, Theme.foregroundMedium));
        });
    }

    // Regressão: a página Informações ocupa a largura toda e não pode engolir os cliques do trilho.
    function test_railStillWorksFromTheInfoPage() {
        const window = createMain(createFake([]), sampleInventory());
        showMinimum(window);
        openDeviceTab(window, "G703", "info");
        openTab(window, "ratbag");
        tryVerify(() => findItem(window, "pointerReadButton").visible);
    }

    // O aplicativo instalado pode rodar sem o plugin de tema do KDE: o texto não pode herdar a paleta do sistema.
    function test_textColorsComeFromTheThemeNotFromTheSystemPalette() {
        const window = createMain(createFake([{name: "G915", summary: "63% · descarregando", percent: 63}]),
                                  sampleInventory());
        showMinimum(window);
        openDevice(window, "G915");
        const same = (item, color) => verify(Qt.colorEqual(item.color, color), item.objectName + " " + item.color);
        same(visibleByName(window, "panelTitle")[0], Theme.foreground);
        same(visibleByName(window, "batteryPercentText")[0], Theme.foreground);
        same(visibleByName(window, "switchRowLabel")[0], Theme.foreground);
        openTab(window, "info");
        same(visibleByName(window, "inventoryDeviceLabel")[0], Theme.foreground);
        same(visibleByName(window, "batteryPercentText")[0], Theme.foreground);
    }

    // Item 12: a página Perfis usa os controles próprios.
    function test_profilesPageUsesOwnTextFieldAndSwitch() {
        const window = createMainWithProfiles(createProfilesFake({}));
        const field = findItem(window, "profileNameField");
        const underline = descendants(field).find(i => i.objectName === "fieldUnderline");
        compare(underline.height, 1);
        const toggle = findItem(window, "profilesAutoSwitch");
        compare(toggle.width, 54);
        verify(toggle.toggleRequested !== undefined, "GHubSwitch");
    }

    // ---- INC-014 parte 3: renderização fora da tela (1024×768) para conferir o layout contra a referência ----

    // Lê uma variável de ambiente (QML não tem getenv): /proc/self/environ, liberado pelo ctest com
    // QML_XHR_ALLOW_FILE_READ=1. Sem a variável (ou sem a leitura), devolve "".
    function environmentVariable(name) {
        try {
            const request = new XMLHttpRequest();
            request.open("GET", "file:///proc/self/environ", false);
            request.send();
            const entry = request.responseText.split("\u0000").find(e => e.startsWith(name + "="));
            return entry === undefined ? "" : entry.substring(name.length + 1);
        } catch (e) {
            return "";
        }
    }

    function renderSampleWindow() {
        const batteries = createFake([{name: "G915", summary: "63% · descarregando", percent: 63},
                                      {name: "G703", summary: "88% · carregando", percent: 88},
                                      {name: "G733", summary: "41% · descarregando", percent: 41}]);
        const inventory = createFake([{name: "G915", summary: "detectado via receptor Lightspeed"},
                                      {name: "G703", summary: "detectado via receptor Lightspeed e cabo USB"},
                                      {name: "G733", summary: "detectado via receptor USB"},
                                      {name: "Brio 100", summary: "detectado via cabo USB"}]);
        const camera = createCameraFake("Brio 100 detectada", [{text: "MJPG: até 1920×1080 a 30 fps"},
                                                               {text: "YUYV: até 640×480 a 30 fps"}],
                                        [integerControl({id: 1}), booleanControl({id: 2}), menuControl({id: 3})]);
        const pointer = createPointerFake({statusText: "lido", deviceName: "G703", connectionText: "receptor",
                                           profiles: [pointerProfile()]});
        const onboard = createOnboardFake({loaded: true, dpiStages: [400, 800, 1600, 3200], defaultStage: 1,
                                           reportRateHz: 1000, backupExists: true, status: "memória lida"});
        const equalizer = createEqualizerFake({gains: [4, 3, 1, 0, -1, 0, 2, 3, 2, 1], status: "EQ desligado"});
        const profiles = createProfilesFake({profileNames: ["Jogos", "Escritório"],
                                             profileApplications: {"Jogos": ["steam"]}, status: "2 perfis"});
        const diagnostics = createDiagnosticsFake({backends: [{name: "HeadsetControl", available: true, detail: "4.1.0"},
                                                              {name: "ratbagd", available: false, detail: "inativo"}],
                                                   status: "pronto"});
        const lighting = createLightingFake({live: true, status: "modo ao vivo ligado"});
        const keyboardStatus = createStatusFake({status: {brightness: "100 de 100",
            brightnessUnexplained: "05 03 (não interpretado)", reportRate: "1000 Hz (1 ms)",
            supportedReportRates: "125, 250, 500, 1000"}});
        const pointerStatus = createStatusFake({status: {dpiRange: "100 a 25600", dpi: "7050 (padrão 800)",
            reportRate: "1000 Hz (1 ms)", supportedReportRates: "125, 250, 500, 1000"}});
        const headsetStatus = createStatusFake({status: {equalizer: "10 bandas", equalizerUnexplained: "",
            uninterpreted: "00 00 00 00 (não interpretado)"}});
        return createMain(batteries, inventory, camera, null, null, pointer, lighting, null, equalizer, onboard, null,
                          null, diagnostics, profiles, null, keyboardStatus, pointerStatus, headsetStatus);
    }

    // Sem GHUB_UI_RENDER_DIR só confere que a captura funciona; com ela, grava um PNG por tela.
    function test_rendersEachScreenAtMinimumSize() {
        const directory = environmentVariable("GHUB_UI_RENDER_DIR");
        const window = renderSampleWindow();
        showMinimum(window);
        const root = window.contentItem.parent;
        const capture = name => {
            waitForRendering(root);
            const image = grabImage(root);
            compare(image.width, 1024, name);
            compare(image.height, 768, name);
            // A barra do topo é preta (Theme.background): o canto superior esquerdo da captura confirma que
            // a captura enxerga o cabeçalho, não só o conteúdo.
            verify(Qt.colorEqual(image.pixel(2, 2), Theme.background), name + " " + image.pixel(2, 2));
            // save() lança exceção se não conseguir gravar (pasta inexistente, por exemplo).
            if (directory !== "")
                image.save(directory + "/" + name + ".png");
        };
        capture("01-inicio");
        openDevice(window, "G915");
        capture("02-g915-iluminacao");
        mouseClick(descendants(findItem(window, "lightingTabs")).filter(i => i.objectName === "innerTab")[1]);
        waitForRendering(window.contentItem);
        capture("13-g915-efeitos");
        openTab(window, "info");
        capture("03-g915-informacoes");
        clickButton(window, "backButton");
        openDevice(window, "G703");
        capture("04-g703-sensibilidade");
        openTab(window, "info");
        capture("14-g703-informacoes");
        openTab(window, "ratbag");
        capture("05-g703-leitura");
        clickButton(window, "backButton");
        openDevice(window, "G733");
        capture("06-g733-headset");
        openTab(window, "equalizer");
        capture("07-g733-equalizador");
        openTab(window, "microphone");
        capture("08-g733-microfone");
        openTab(window, "info");
        capture("15-g733-informacoes");
        clickButton(window, "backButton");
        openDevice(window, "Brio 100");
        capture("09-brio-imagem");
        openTab(window, "modes");
        capture("10-brio-modos");
        clickButton(window, "backButton");
        clickButton(window, "profilesPageButton");
        capture("11-perfis");
        clickButton(window, "settingsPageButton");
        capture("12-configuracoes");
    }

    // INC-015: identidade HID++ na aba Informações de G915, G703 e G733; nada é lido ao navegar.
    function test_identityButtonCallsReadWithTheDeviceKeyOnly_OnClick() {
        const identity = createIdentityFake();
        const window = createMain(createFake([]), sampleInventory(), null, null, null, null, null, null, null, null,
                                  null, null, null, null, identity);
        const expected = {"G915": "g915", "G703": "g703", "G733": "g733"};
        for (const name of Object.keys(expected)) {
            openDeviceTab(window, name, "info");
            tryVerify(() => visibleByName(window, "readIdentityButton").length === 1);
            compare(identity.readCalls.length, Object.keys(expected).indexOf(name), "nada é lido ao navegar");
            const button = visibleByName(window, "readIdentityButton")[0];
            verify(button.enabled);
            mouseClick(button);
            compare(identity.readCalls[identity.readCalls.length - 1], expected[name]);
            clickButton(window, "backButton");
        }
        compare(identity.readCalls, ["g915", "g703", "g733"]);
    }

    function test_identitySectionIsAbsentFromTheBrioInfoTab() {
        const identity = createIdentityFake();
        const window = createMain(createFake([]), sampleInventory(), null, null, null, null, null, null, null, null,
                                  null, null, null, null, identity);
        openDeviceTab(window, "Brio 100", "info");
        tryVerify(() => visibleByName(window, "inventoryDeviceLabel").length === 1);
        compare(visibleByName(window, "readIdentityButton").length, 0);
        compare(identity.readCalls.length, 0);
    }

    function test_identityButtonIsDisabledWhileReading() {
        const identity = createIdentityFake({g915: identityEntry("reading")});
        const window = createMain(createFake([]), sampleInventory(), null, null, null, null, null, null, null, null,
                                  null, null, null, null, identity);
        openDeviceTab(window, "G915", "info");
        tryVerify(() => visibleByName(window, "readIdentityButton").length === 1);
        compare(visibleByName(window, "readIdentityButton")[0].enabled, false);
        identity.identities = Object.assign({}, identity.identities, {g915: identityEntry("ok")});
        tryCompare(visibleByName(window, "readIdentityButton")[0], "enabled", true);
    }

    function test_identityShowsTheResultOfTheDeviceOnly() {
        const identity = createIdentityFake({
            g915: identityEntry("ok", {name: "G915 X LIGHTSPEED", type: "teclado",
                                       firmwares: ["MPK 09.04 b0042 — aplicação (ativo)", "BLK 01.00 b0001 — bootloader"],
                                       featureCount: 3, featureIds: ["0x0001", "0x0003", "0x1b04"]}),
            g703: identityEntry("ok", {name: "G703 LIGHTSPEED", type: "mouse", featureCount: 1, featureIds: ["0x0001"]})});
        const window = createMain(createFake([]), sampleInventory(), null, null, null, null, null, null, null, null,
                                  null, null, null, null, identity);
        openDeviceTab(window, "G915", "info");
        const texts = name => visibleByName(window, name).map(i => i.text);
        tryVerify(() => texts("identityNameLabel").length === 1);
        compare(texts("identityNameLabel"), ["Nome: G915 X LIGHTSPEED"]);
        compare(texts("identityTypeLabel"), ["Tipo: teclado"]);
        compare(texts("identityFirmwareLabel"),
                ["Firmware: MPK 09.04 b0042 — aplicação (ativo)\nBLK 01.00 b0001 — bootloader"]);
        compare(texts("identityFeatureCountLabel"), ["Features: 3"]);
        compare(visibleByName(window, "identityErrorLabel").length, 0);
        compare(visibleByName(window, "identityTruncationLabel").length, 0);
    }

    function test_identityShowsNotInformedWhenTheDeviceHasNoFirmware() {
        const identity = createIdentityFake({g733: identityEntry("ok", {name: "não informado", type: "não informado"})});
        const window = createMain(createFake([]), sampleInventory(), null, null, null, null, null, null, null, null,
                                  null, null, null, null, identity);
        openDeviceTab(window, "G733", "info");
        tryVerify(() => visibleByName(window, "identityFirmwareLabel").length === 1);
        compare(visibleByName(window, "identityFirmwareLabel")[0].text, "Firmware: não informado");
        compare(visibleByName(window, "toggleFeatureIdsButton").length, 0, "sem features, sem lista para expandir");
    }

    function test_identityShowsTheErrorAndNoResult() {
        const identity = createIdentityFake({g733: identityEntry("error", {error: "headset desligado ou fora de alcance"})});
        const window = createMain(createFake([]), sampleInventory(), null, null, null, null, null, null, null, null,
                                  null, null, null, null, identity);
        openDeviceTab(window, "G733", "info");
        tryVerify(() => visibleByName(window, "identityErrorLabel").length === 1);
        compare(visibleByName(window, "identityErrorLabel")[0].text, "headset desligado ou fora de alcance");
        compare(visibleByName(window, "identityNameLabel").length, 0);
        compare(visibleByName(window, "identityFirmwareLabel").length, 0);
    }

    function test_identityFeatureIdsExpandOnDemand() {
        const identity = createIdentityFake({
            g915: identityEntry("ok", {name: "G915", type: "teclado", featureCount: 3,
                                       featureIds: ["0x0001", "0x0003", "0x1b04"]})});
        const window = createMain(createFake([]), sampleInventory(), null, null, null, null, null, null, null, null,
                                  null, null, null, null, identity);
        openDeviceTab(window, "G915", "info");
        tryVerify(() => visibleByName(window, "toggleFeatureIdsButton").length === 1);
        compare(visibleByName(window, "identityFeatureIdsLabel").length, 0, "recolhida por padrão");
        const toggle = visibleByName(window, "toggleFeatureIdsButton")[0];
        reveal(toggle);
        mouseClick(toggle);
        tryVerify(() => visibleByName(window, "identityFeatureIdsLabel").length === 1);
        compare(visibleByName(window, "identityFeatureIdsLabel")[0].text, "0x0001, 0x0003, 0x1b04");
        mouseClick(visibleByName(window, "toggleFeatureIdsButton")[0]);
        tryVerify(() => visibleByName(window, "identityFeatureIdsLabel").length === 0);
    }

    function test_identityShowsTheTruncationNote() {
        const identity = createIdentityFake({
            g703: identityEntry("ok", {name: "G703", type: "mouse", featureCount: 64,
                                       truncationNote: "o dispositivo tem mais features do que as 64 lidas"})});
        const window = createMain(createFake([]), sampleInventory(), null, null, null, null, null, null, null, null,
                                  null, null, null, null, identity);
        openDeviceTab(window, "G703", "info");
        tryVerify(() => visibleByName(window, "identityTruncationLabel").length === 1);
        compare(visibleByName(window, "identityTruncationLabel")[0].text,
                "o dispositivo tem mais features do que as 64 lidas");
    }
}
