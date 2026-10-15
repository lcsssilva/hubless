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
                        profilesFake) {
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
                                                     hostEqualizerController: equalizerFake || createEqualizerFake(),
                                                     noiseSuppressionController: noiseFake || createNoiseSuppressionFake(),
                                                     g703OnboardController: onboardFake || createOnboardFake(),
                                                     desktopController: desktopFake || createDesktopFake(),
                                                     diagnosticsViewModel: diagnosticsFake || createDiagnosticsFake(),
                                                     profilesController: profilesFake || createProfilesFake()});
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
        mouseClick(card);
        tryVerify(() => window.currentPage !== 0);
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

    function effectButtons(window) {
        return allItems(window).filter(item => item.objectName === "lightingEffectButton");
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
    // os demais são os "Padrão" (um por controle).
    function test_cameraSectionButtons_ArePreviewToggleAndOneDefaultPerControl() {
        const window = createMain(createFake([]), null, createCameraFake("Brio 100",
            [{text: "MJPG: até 1920×1080 a 30 fps"}], [integerControl(), booleanControl()]));
        tryVerify(() => labelTexts(window, "cameraControlLabel").length === 2);
        const items = allItems(window);
        verify(items.some(item => /^(Label|QQuickLabel)/.test(item.toString())), "toString deve expor o tipo");
        // O fundo do Button (ButtonPanel) não é um botão: o padrão exige o nome do tipo seguido de "(".
        const buttons = items.filter(item => /^(Button|QQuickButton)(_QMLTYPE_\d+)?\(/.test(item.toString()));
        compare(buttons.map(b => b.objectName).sort().join(","),
                "cameraControlDefault,cameraControlDefault,cameraPreviewToggle,diagnosticsExportButton,"
                + "diagnosticsRefreshButton,eqDisableButton,eqEnableButton,"
                + "eqFlatButton,headsetLightsOffButton,"
                + "headsetLightsOnButton,headsetSidetoneApplyButton,lightingCustomColorButton,"
                + "lightingLiveButton,noiseDisableButton,noiseEnableButton,onboardAddStageButton,onboardReadButton,onboardRemoveStageButton,"
                + "onboardRestoreButton,onboardWriteButton,pointerReadButton,profileSaveButton"); // amostras: delegates de tipo próprio, testadas à parte
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

    function test_lightingEffectButtonsExistAndAreDisabledWithoutLiveOrWhileBusy() {
        const idle = createMain(createFake([]), null, null, null, null, null, createLightingFake());
        compare(effectButtons(idle).map(b => b.text).join(","), "Onda,Ciclo de cores,Respiração,Ondulação");
        effectButtons(idle).forEach(b => compare(b.enabled, false));
        const busy = createMain(createFake([]), null, null, null, null, null,
                                createLightingFake({live: true, busy: true}));
        compare(effectButtons(busy).length, 4);
        effectButtons(busy).forEach(b => compare(b.enabled, false));
    }

    function test_lightingEffectButtonsApplyTheirKindWithTheChosenColor() {
        const lighting = createLightingFake({live: true});
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const buttons = effectButtons(window);
        buttons.forEach(b => verify(b.enabled));
        buttons[0].clicked();
        swatches(window)[2].clicked();
        buttons[2].clicked();
        buttons[3].clicked();
        buttons[1].clicked();
        compare(JSON.stringify(lighting.effectCalls),
                JSON.stringify([["wave", "#ffffff"], ["breathing", "#0000ff"], ["ripple", "#0000ff"],
                                ["cycle", "#0000ff"]]));
        compare(lighting.paintCalls.length, 1);
        compare(lighting.startCalls, 0);
    }

    function test_lightingButtonStartsWhenNotLive() {
        const lighting = createLightingFake();
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const button = findItem(window, "lightingLiveButton");
        compare(button.text, "Ligar modo ao vivo");
        button.clicked();
        compare(lighting.startCalls, 1);
        compare(lighting.stopCalls, 0);
    }

    function test_lightingButtonStopsWhenLive() {
        const lighting = createLightingFake({live: true});
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const button = findItem(window, "lightingLiveButton");
        compare(button.text, "Desligar modo ao vivo");
        button.clicked();
        compare(lighting.stopCalls, 1);
        compare(lighting.startCalls, 0);
    }

    function test_lightingButtonDisabledWhileBusy() {
        const lighting = createLightingFake({busy: true});
        const window = createMain(createFake([]), null, null, null, null, null, lighting);
        const button = findItem(window, "lightingLiveButton");
        compare(button.enabled, false);
        lighting.busy = false;
        tryVerify(() => button.enabled);
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
        verify(notice.text.indexOf("respiração") >= 0, notice.text);
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

    function test_eqButtonsForwardToTheController() {
        const eq = createEqualizerFake();
        const window = createMainWithEqualizer(eq);
        findItem(window, "eqEnableButton").clicked();
        eq.enabled = true;
        tryVerify(() => findItem(window, "eqDisableButton").enabled);
        findItem(window, "eqDisableButton").clicked();
        findItem(window, "eqFlatButton").clicked();
        compare(eq.enableCalls, 1);
        compare(eq.disableCalls, 1);
        compare(eq.flatCalls, 1);
    }

    function test_eqButtonsFollowEnabledAndBusy() {
        const eq = createEqualizerFake();
        const window = createMainWithEqualizer(eq);
        compare(findItem(window, "eqEnableButton").enabled, true);
        compare(findItem(window, "eqDisableButton").enabled, false);
        eq.enabled = true;
        tryCompare(findItem(window, "eqEnableButton"), "enabled", false);
        compare(findItem(window, "eqDisableButton").enabled, true);
        eq.busy = true;
        tryCompare(findItem(window, "eqDisableButton"), "enabled", false);
        compare(findItem(window, "eqFlatButton").enabled, false);
        eq.enabled = false;
        compare(findItem(window, "eqEnableButton").enabled, false); // ocupado
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

    function test_noiseButtonsForwardToTheController() {
        const noise = createNoiseSuppressionFake();
        const window = createMainWithNoiseSuppression(noise);
        findItem(window, "noiseEnableButton").clicked();
        noise.enabled = true;
        tryVerify(() => findItem(window, "noiseDisableButton").enabled);
        findItem(window, "noiseDisableButton").clicked();
        compare(noise.enableCalls, 1);
        compare(noise.disableCalls, 1);
    }

    function test_noiseButtonsFollowEnabledAndBusy() {
        const noise = createNoiseSuppressionFake();
        const window = createMainWithNoiseSuppression(noise);
        compare(findItem(window, "noiseEnableButton").enabled, true);
        compare(findItem(window, "noiseDisableButton").enabled, false);
        noise.enabled = true;
        tryCompare(findItem(window, "noiseEnableButton"), "enabled", false);
        compare(findItem(window, "noiseDisableButton").enabled, true);
        noise.busy = true;
        tryCompare(findItem(window, "noiseDisableButton"), "enabled", false);
        noise.enabled = false;
        compare(findItem(window, "noiseEnableButton").enabled, false); // ocupado
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
        compare(window.width, 1100);
        compare(window.height, 720);
        compare(window.minimumWidth, 900);
        compare(window.minimumHeight, 600);
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
        compare(findItem(window, "eqEnableButton").visible, false);
        openTab(window, "equalizer");
        tryVerify(() => findItem(window, "eqEnableButton").visible);
        compare(sidetone.visible, false);
        openTab(window, "microphone");
        tryVerify(() => findItem(window, "noiseEnableButton").visible);
        compare(findItem(window, "eqEnableButton").visible, false);
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
        clickButton(window, "profilesPageButton");
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
        verify(Qt.colorEqual(Theme.background, "#101113"), String(Theme.background));
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

    function test_windowUsesThemeColors() {
        const window = createMain(createFake([]));
        verify(Qt.colorEqual(window.Material.accent, Theme.primary), String(window.Material.accent));
        verify(Qt.colorEqual(window.Material.background, Theme.background), String(window.Material.background));
        verify(Qt.colorEqual(window.Material.primary, Theme.surface), String(window.Material.primary));
        verify(Qt.colorEqual(window.Material.foreground, Theme.foreground), String(window.Material.foreground));
    }

    function test_deviceCardUsesThemeShape() {
        const window = createMain(createFake([]), sampleInventory());
        const card = deviceCards(window)[0];
        compare(card.radius, Theme.radius);
        verify(Qt.colorEqual(card.color, Theme.surface), String(card.color));
        verify(Qt.colorEqual(card.border.color, Theme.outline), String(card.border.color));
    }

    function test_deviceCardOverlayFollowsHoverAndPress() {
        const window = createMain(createFake([]), sampleInventory());
        window.show();
        waitForRendering(window.contentItem);
        const card = deviceCards(window)[0];
        tryVerify(() => card.visible && card.width > 0 && card.height > 0);
        const overlay = stateOverlayOf(card);
        compare(overlay.radius, Theme.radius);
        compare(overlay.opacity, 0);
        mouseMove(card, 20, 20);
        tryCompare(overlay, "opacity", Theme.hoverOpacity);
        verify(Qt.colorEqual(card.border.color, Theme.primary), String(card.border.color));
        mousePress(card, 20, 20);
        tryCompare(overlay, "opacity", Theme.pressOpacity);
        mouseRelease(card, 20, 20);
        mouseMove(card, -20, -20);
        tryCompare(overlay, "opacity", 0);
        verify(Qt.colorEqual(card.border.color, Theme.outline), String(card.border.color));
    }

    function test_sideTabUsesThemeColorsAndOverlay() {
        const window = createMain(createFake([]), sampleInventory());
        openDevice(window, "G915");
        const tabs = allItems(window).filter(item => item.objectName === "sideTab" && item.visible && item.width > 0);
        const selected = tabs.find(t => t.selected);
        const other = tabs.find(t => !t.selected);
        verify(selected !== undefined && other !== undefined);
        verify(Qt.colorEqual(selected.contentItem.color, Theme.primary), String(selected.contentItem.color));
        verify(Qt.colorEqual(other.contentItem.color, Theme.foreground), String(other.contentItem.color));
        const fill = descendants(selected).find(i => i.objectName === "selectedFill");
        verify(fill !== undefined, "selectedFill");
        compare(fill.radius, Theme.radius);
        verify(Qt.colorEqual(fill.color, Theme.primary), String(fill.color));
        compare(fill.opacity, Theme.selectedOpacity);
        const otherFill = descendants(other).find(i => i.objectName === "selectedFill");
        compare(otherFill.opacity, 0);
        const selectedOverlay = stateOverlayOf(selected);
        const overlay = stateOverlayOf(other);
        compare(overlay.opacity, 0);
        mouseMove(other, 20, 10);
        tryCompare(overlay, "opacity", Theme.hoverOpacity);
        mousePress(other, 20, 10);
        tryCompare(overlay, "opacity", Theme.pressOpacity);
        mouseRelease(other, 20, 10);
        mouseMove(selected, -100, 10);
        tryCompare(overlay, "opacity", 0);
        compare(selectedOverlay.opacity, 0);
    }

    function test_profilesLoadFailedLabelUsesErrorColor() {
        const window = createMainWithProfiles(createProfilesFake({loadFailed: true}));
        const warning = findItem(window, "profilesLoadFailedLabel");
        verify(Qt.colorEqual(warning.color, Theme.error), String(warning.color));
    }
}
