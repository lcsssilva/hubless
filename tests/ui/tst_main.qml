import QtQuick
import QtTest

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
                        headsetFake) {
        const component = Qt.createComponent(Qt.resolvedUrl("../../src/ui/qml/Main.qml"));
        compare(component.status, Component.Ready, component.errorString());
        const window = component.createObject(null, {batteryViewModel: fake,
                                                     deviceInventoryViewModel: inventoryFake || createFake([]),
                                                     cameraViewModel: cameraFake || createCameraFake("não detectada", [], []),
                                                     cameraPreviewViewModel: previewFake || createPreviewFake(),
                                                     cameraControlEditor: editorFake || createEditorFake(),
                                                     pointerViewModel: pointerFake || createPointerFake(),
                                                     keyboardLightingController: lightingFake || createLightingFake(),
                                                     headsetSettingsController: headsetFake || createHeadsetFake()});
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

    function createMainWithHeadset(headset) {
        return createMain(createFake([]), null, null, null, null, null, null, headset);
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

    // Percorre a árvore visual (inclui o contentItem de Flickable/ScrollView) em largura.
    function allItems(window) {
        const items = [];
        const pending = [window.contentItem];
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
        const button = findItem(window, "cameraControlDefault");
        tryVerify(() => button.width > 0 && button.visible);
        mouseClick(button);
        tryCompare(editor, "defaultCalls", [7]);
        compare(editor.setCalls.length, 0);
    }

    function test_togglingSwitchCallsEditorSetValue() {
        const editor = createEditorFake();
        const window = createMain(createFake([]), null, createCameraFake("Brio 100", [],
            [booleanControl({id: 9})]), null, editor);
        window.show();
        const toggle = findItem(window, "cameraControlSwitch");
        tryVerify(() => toggle.width > 0 && toggle.visible);
        mouseClick(toggle);
        tryCompare(editor, "setCalls", [[9, 0]]);
    }

    function test_sliderWritesOnlyOnRelease() {
        const editor = createEditorFake();
        const window = createMain(createFake([]), null, createCameraFake("Brio 100", [],
            [integerControl({id: 5})]), null, editor);
        window.show();
        const slider = findItem(window, "cameraControlSlider");
        tryVerify(() => slider.width > 0 && slider.visible);
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
                "cameraControlDefault,cameraControlDefault,cameraPreviewToggle,headsetLightsOffButton,"
                + "headsetLightsOnButton,headsetSidetoneApplyButton,lightingCustomColorButton,"
                + "lightingLiveButton,pointerReadButton"); // amostras: delegates de tipo próprio, testadas à parte
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
        const button = findItem(window, "cameraPreviewToggle");
        tryVerify(() => button.width > 0 && button.visible);
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
}
