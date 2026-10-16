import QtQuick
import QtQuick.Controls
import QtTest
import "../../src/ui/qml"

// Controles próprios no estilo do G HUB (REFERENCIA-ESTILO-GHUB.md §4.3), testados isolados do Main.qml:
// cada teste cria o componente pelo código-fonte (assim a falha mostra o erro real quando o tipo não existe).
TestCase {
    id: testCase
    name: "GHubControls"
    width: 600
    height: 600
    when: windowShown
    visible: true

    property var created: []

    function cleanup() {
        created.forEach(o => { o.visible = false; o.destroy(); });
        created = [];
    }

    // Cria `type { props }` como filho do TestCase. `props` é um texto QML de propriedades (pode ser vazio).
    function make(type, props) {
        const code = "import QtQuick\nimport QtQuick.Controls\nimport \"../../src/ui/qml\"\n"
                   + type + " {" + (props || "") + "}";
        const object = Qt.createQmlObject(code, testCase, Qt.resolvedUrl("tst_components.qml"));
        verify(object !== null, type);
        created.push(object);
        waitForRendering(testCase);
        return object;
    }

    function descendants(root) {
        const items = [];
        const pending = [root];
        while (pending.length > 0) {
            const item = pending.shift();
            items.push(item);
            for (let i = 0; i < item.children.length; ++i)
                pending.push(item.children[i]);
            if (item.contentItem && item.contentItem !== item && !pending.includes(item.contentItem))
                pending.push(item.contentItem);
        }
        return items;
    }

    function named(root, objectName) {
        const found = descendants(root).filter(i => i.objectName === objectName);
        verify(found.length >= 1, objectName + " não encontrado");
        return found[0];
    }

    // ---- GHubSwitch (item 6) ----

    function test_switch_isAPill48x24WithOffTrackAndKnobOnTheLeft() {
        const control = make("GHubSwitch");
        compare(control.width, 48);
        compare(control.height, 24);
        const track = named(control, "switchTrack");
        const knob = named(control, "switchKnob");
        compare(track.radius, 12);
        verify(Qt.colorEqual(track.color, "#a6a6a7"), String(track.color));
        verify(Qt.colorEqual(knob.color, "#ffffff"), String(knob.color));
        verify(knob.x + knob.width / 2 < control.width / 2, "bolinha à esquerda");
    }

    function test_switch_onShowsPrimaryTrackAndKnobOnTheRight() {
        const control = make("GHubSwitch", "checked: true");
        const track = named(control, "switchTrack");
        const knob = named(control, "switchKnob");
        verify(Qt.colorEqual(track.color, "#1196ff"), String(track.color));
        tryVerify(() => knob.x + knob.width / 2 > control.width / 2, 5000, "bolinha à direita");
        verify(Qt.colorEqual(knob.color, "#ffffff"), String(knob.color));
    }

    // O estado exibido vem de quem usa (ViewModel): o clique só pede a mudança, não altera `checked` sozinho.
    function test_switch_clickRequestsTheOppositeStateWithoutChangingItself() {
        const control = make("GHubSwitch");
        const requests = [];
        control.toggleRequested.connect(wanted => requests.push(wanted));
        mouseClick(control);
        compare(requests, [true]);
        compare(control.checked, false);
        control.checked = true;
        mouseClick(control);
        compare(requests, [true, false]);
        compare(control.checked, true);
    }

    function test_switch_directClickedSignalAlsoRequests() {
        const control = make("GHubSwitch", "checked: true");
        const requests = [];
        control.toggleRequested.connect(wanted => requests.push(wanted));
        control.clicked();
        compare(requests, [false]);
    }

    function test_switch_disabledDoesNotRequest() {
        const control = make("GHubSwitch", "enabled: false");
        const requests = [];
        control.toggleRequested.connect(wanted => requests.push(wanted));
        mouseClick(control);
        compare(requests.length, 0);
        verify(control.opacity < 1, "esmaecido");
    }

    // ---- GHubComboBox (itens 4 e 7) ----

    function comboItems(combo) {
        return [0, 1, 2].map(i => combo.popup.contentItem.itemAtIndex(i));
    }

    function test_combo_showsUppercaseWhiteValueWithChevronAndNoBox() {
        const combo = make("GHubComboBox", "model: [\"Onda\", \"Ciclo\", \"Respiração\"]; currentIndex: 1");
        const value = named(combo, "comboValue");
        compare(value.text, "Ciclo");
        compare(value.font.capitalization, Font.AllUppercase);
        verify(Qt.colorEqual(value.color, "#ffffff"), String(value.color));
        verify(value.font.pixelSize >= 13 && value.font.pixelSize <= 14, String(value.font.pixelSize));
        verify(named(combo, "comboChevron").visible);
        verify(combo.background === null || combo.background.color.a === 0, "sem caixa");
        compare(combo.background === null ? 0 : combo.background.border.width, 0);
    }

    function test_combo_showsPlaceholderWhenNothingIsChosen() {
        const combo = make("GHubComboBox", "model: [\"Onda\"]; currentIndex: -1; placeholderText: \"Escolher\"");
        compare(named(combo, "comboValue").text, "Escolher");
    }

    function test_combo_openListIsAFlat240pxBoxWith24pxItemsAndHoverHighlight() {
        const combo = make("GHubComboBox", "model: [\"Onda\", \"Ciclo\", \"Respiração\"]; x: 20; y: 20");
        mouseClick(combo);
        tryVerify(() => combo.popup.visible);
        tryVerify(() => comboItems(combo).every(i => i !== null && i !== undefined));
        const popup = combo.popup;
        compare(popup.width, 240);
        compare(popup.background.radius, 4);
        verify(Qt.colorEqual(popup.background.color, "#393939"), String(popup.background.color));
        const items = comboItems(combo);
        compare(items[1].y - items[0].y, 24);
        compare(items[2].y - items[1].y, 24);
        mouseMove(items[1], 10, 10);
        // CSS computado §5.3: itens brancos a 0,4 de opacidade; o sob o cursor vai a 1,0.
        tryCompare(items[1], "opacity", 1);
        compare(items[2].opacity, 0.4);
    }

    function test_combo_choosingAnItemEmitsItsIndexAndClosesTheList() {
        const combo = make("GHubComboBox", "model: [\"Onda\", \"Ciclo\", \"Respiração\"]; x: 20; y: 20");
        const chosen = [];
        combo.activated.connect(index => chosen.push(index));
        mouseClick(combo);
        tryVerify(() => combo.popup.visible);
        tryVerify(() => comboItems(combo).every(i => i !== null && i !== undefined));
        mouseClick(comboItems(combo)[2]);
        tryCompare(combo.popup, "visible", false);
        compare(chosen, [2]);
        compare(combo.currentIndex, 2);
    }

    // ---- GHubSpinBox (item 7) ----

    function test_spin_showsBoldCenteredValueWith1pxUnderlineAndNoBox() {
        const spin = make("GHubSpinBox", "from: 100; to: 400; stepSize: 50; value: 200");
        const text = spin.contentItem;
        compare(text.text, "200");
        compare(text.font.bold, true);
        compare(text.font.pixelSize, 14);
        compare(text.horizontalAlignment, Text.AlignHCenter);
        const underline = named(spin, "spinUnderline");
        compare(underline.height, 1);
        verify(Qt.colorEqual(underline.color, "#afb1b4"), String(underline.color));
        compare(named(spin, "spinMinus").width, 24);
        compare(named(spin, "spinPlus").width, 24);
        verify(spin.background === null || spin.background.color.a === 0, "sem caixa");
    }

    function test_spin_plusAndMinusStepAndRespectTheLimits() {
        const spin = make("GHubSpinBox", "from: 100; to: 200; stepSize: 50; value: 150");
        const changes = [];
        spin.valueModified.connect(() => changes.push(spin.value));
        mouseClick(named(spin, "spinPlus"));
        compare(spin.value, 200);
        mouseClick(named(spin, "spinPlus"));
        compare(spin.value, 200);
        mouseClick(named(spin, "spinMinus"));
        mouseClick(named(spin, "spinMinus"));
        compare(spin.value, 100);
        mouseClick(named(spin, "spinMinus"));
        compare(spin.value, 100);
        compare(changes, [200, 150, 100]);
    }

    // ---- GHubRadioButton (item 7) ----

    function test_radio_isACircleWithThinBorderAndFillsWhenChecked() {
        const radio = make("GHubRadioButton", "text: \"padrão\"");
        const ring = named(radio, "radioRing");
        verify(ring.width >= 12 && ring.width <= 16, String(ring.width));
        compare(ring.radius, ring.width / 2);
        compare(ring.border.width, 1);
        verify(Qt.colorEqual(ring.border.color, "#afb1b4"), String(ring.border.color));
        compare(named(radio, "radioDot").visible, false);
        mouseClick(radio);
        compare(radio.checked, true);
        compare(named(radio, "radioDot").visible, true);
    }

    function test_radio_exclusiveGroupKeepsOneChecked() {
        const host = make("Item", "Column { GHubRadioButton { objectName: \"a\"; checked: true } GHubRadioButton { objectName: \"b\" } }");
        const a = named(host, "a");
        const b = named(host, "b");
        mouseClick(b);
        compare(b.checked, true);
        compare(a.checked, false);
    }

    // ---- GHubTextField (item 7) ----

    function test_textField_hasOnlyA1pxUnderlineAndDimPlaceholder() {
        const field = make("GHubTextField", "placeholderText: \"Nome do perfil\"; width: 200");
        const underline = named(field, "fieldUnderline");
        compare(underline.height, 1);
        verify(Qt.colorEqual(underline.color, "#afb1b4"), String(underline.color));
        compare(underline.y + underline.height, field.height);
        compare(underline.width, field.width);
        verify(Qt.colorEqual(field.placeholderTextColor, Qt.rgba(1, 1, 1, 0.6)), String(field.placeholderTextColor));
        field.forceActiveFocus();
        keyClick("A");
        compare(field.text, "A");
    }

    // ---- ColorSwatch (item 3) ----

    function test_swatch_is20pxCircleOfItsColorWithWhiteRingWhenSelected() {
        const swatch = make("ColorSwatch", "swatchColor: \"#ff0000\"; colorName: \"Vermelho\"");
        compare(swatch.width, 20);
        compare(swatch.height, 20);
        const disc = named(swatch, "swatchDisc");
        compare(disc.radius, 10);
        verify(Qt.colorEqual(disc.color, "#ff0000"), String(disc.color));
        compare(named(swatch, "swatchRing").visible, false);
        swatch.selected = true;
        const ring = named(swatch, "swatchRing");
        compare(ring.visible, true);
        compare(ring.border.width, 2);
        verify(Qt.colorEqual(ring.border.color, "#ffffff"), String(ring.border.color));
        compare(swatch.Accessible.name, "Vermelho");
        compare(swatch.ToolTip.text, "Vermelho");
    }

    function test_swatch_clickEmitsClickedOnlyWhenEnabled() {
        const swatch = make("ColorSwatch", "swatchColor: \"#00ff00\"; colorName: \"Verde\"");
        let clicks = 0;
        swatch.clicked.connect(() => clicks++);
        mouseClick(swatch);
        compare(clicks, 1);
        swatch.enabled = false;
        mouseClick(swatch);
        compare(clicks, 1);
        verify(swatch.opacity < 1);
    }

    function test_swatch_plusVariantIsA20pxRoundButtonWithAPlusSign() {
        const plus = make("ColorSwatch", "plus: true; colorName: \"Outra cor…\"");
        compare(plus.width, 20);
        compare(named(plus, "swatchPlus").text, "+");
        compare(plus.Accessible.name, "Outra cor…");
    }

    // ---- InnerTabs (item 5) ----

    function test_innerTabs_areUppercase14pxWithUnderlineOnTheActiveOne() {
        const tabs = make("InnerTabs", "model: [\"Cores\", \"Efeitos\"]");
        const buttons = descendants(tabs).filter(i => i.objectName === "innerTab");
        compare(buttons.length, 2);
        compare(buttons.map(b => b.text), ["Cores", "Efeitos"]);
        const labels = buttons.map(b => named(b, "innerTabLabel"));
        labels.forEach(l => {
            compare(l.font.pixelSize, 14);
            compare(l.font.capitalization, Font.AllUppercase);
        });
        verify(Qt.colorEqual(labels[0].color, "#ffffff"), String(labels[0].color));
        verify(Qt.colorEqual(labels[1].color, "#ffffff"), String(labels[1].color));
        compare(buttons[1].opacity, 0.6);
        compare(buttons[1].x - (buttons[0].x + buttons[0].width), 16);
        const underline = named(buttons[0], "innerTabUnderline");
        compare(underline.visible, true);
        compare(underline.height, 2);
        compare(underline.width, labels[0].contentWidth);
        verify(Qt.colorEqual(underline.color, "#ffffff"), String(underline.color));
        compare(underline.y - (labels[0].y + labels[0].contentHeight), 8);
        compare(named(buttons[1], "innerTabUnderline").visible, false);
    }

    function test_innerTabs_clickingATabSelectsItAndEmitsItsIndex() {
        const tabs = make("InnerTabs", "model: [\"Cores\", \"Efeitos\"]");
        const chosen = [];
        tabs.tabSelected.connect(index => chosen.push(index));
        const buttons = descendants(tabs).filter(i => i.objectName === "innerTab");
        mouseClick(buttons[1]);
        compare(tabs.currentIndex, 1);
        compare(chosen, [1]);
        compare(named(buttons[1], "innerTabUnderline").visible, true);
        compare(named(buttons[0], "innerTabUnderline").visible, false);
    }

    // ---- HelpIcon (item 2) ----

    function test_help_isA20pxCircleWithQuestionMarkAndShowsTheTextInAWhiteTooltip() {
        const help = make("HelpIcon", "text: \"Texto longo explicativo\"; x: 100; y: 100");
        compare(help.width, 20);
        compare(help.height, 20);
        const disc = named(help, "helpDisc");
        compare(disc.radius, 10);
        verify(Qt.colorEqual(disc.border.color, "#a6a6a8") || Qt.colorEqual(disc.color, "#a6a6a8"),
               String(disc.color) + " " + String(disc.border.color));
        compare(named(help, "helpMark").text, "?");
        compare(help.tip.visible, false);
        mouseMove(help, 10, 10);
        tryVerify(() => help.tip.visible);
        compare(help.tip.text, "Texto longo explicativo");
        verify(Qt.colorEqual(help.tip.background.color, "#ffffff"), String(help.tip.background.color));
        verify(Qt.colorEqual(help.tip.contentItem.color, "#000000"), String(help.tip.contentItem.color));
        compare(help.tip.contentItem.font.pixelSize, 12);
        compare(help.tip.background.radius, 5);
        mouseMove(help, 300, 300);
        tryVerify(() => !help.tip.visible);
    }

    function test_help_clickAlsoShowsTheTextAndAccessibleNameIsTheText() {
        const help = make("HelpIcon", "text: \"Explicação\"; x: 100; y: 100");
        mouseClick(help);
        tryVerify(() => help.tip.visible);
        compare(help.Accessible.description, "Explicação");
    }

    // ---- SectionHeader: título de seção MAIÚSCULO com "?" opcional ----

    function test_sectionHeader_showsTheHelpIconOnlyWhenThereIsHelpText() {
        const plain = make("SectionHeader", "text: \"Efeito\"; width: 278");
        const title = named(plain, "sectionTitle");
        compare(title.text, "Efeito");
        compare(title.font.capitalization, Font.AllUppercase);
        compare(named(plain, "helpIcon").visible, false);
        const withHelp = make("SectionHeader", "text: \"Efeito\"; help: \"Explicação\"; width: 278");
        compare(named(withHelp, "helpIcon").visible, true);
        compare(named(withHelp, "helpIcon").text, "Explicação");
    }

    // ---- GHubSlider: valor em negrito à direita e extremos abaixo (item 7) ----

    function test_slider_showsBoldValueOnTheRightAtTrackHeightAndExtremesBelow() {
        const slider = make("GHubSlider", "from: 0; to: 100; value: 40; width: 250; showValue: true; showExtremes: true");
        const track = named(slider, "sliderTrack");
        const value = named(slider, "sliderValue");
        compare(value.text, "40");
        compare(value.font.bold, true);
        compare(value.font.pixelSize, 14);
        verify(value.x >= track.x + track.width, "valor à direita do trilho");
        verify(Math.abs((value.y + value.height / 2) - (track.y + track.height / 2)) <= 1, "na altura do trilho");
        compare(value.x + value.width, slider.width);
        const low = named(slider, "sliderMinLabel");
        const high = named(slider, "sliderMaxLabel");
        compare(low.text, "0");
        compare(high.text, "100");
        compare(low.font.pixelSize, 11);
        verify(Qt.colorEqual(low.color, "#888888"), String(low.color));
        verify(low.y > track.y + track.height, "extremos abaixo do trilho");
        compare(Math.round(low.y + low.height / 2 - (track.y + track.height / 2)), 18);
        slider.value = 77;
        compare(value.text, "77");
    }

    function test_slider_withoutTheOptionsKeepsTheCompactLayout() {
        const slider = make("GHubSlider", "from: 0; to: 100; width: 250");
        compare(slider.height, 20);
        compare(descendants(slider).filter(i => i.objectName === "sliderValue" && i.visible).length, 0);
        compare(descendants(slider).filter(i => i.objectName === "sliderMinLabel" && i.visible).length, 0);
    }

    // ---- BatteryBlock (item 8) ----

    function test_batteryBlock_stacksHeaderPercentWithIconAndState() {
        const block = make("BatteryBlock", "summary: \"63% · descarregando\"; percent: 63");
        const header = named(block, "batteryHeader");
        compare(header.text, "Nível de bateria");
        compare(header.font.capitalization, Font.AllUppercase);
        compare(header.font.pixelSize, 13);
        const value = named(block, "batteryPercentText");
        compare(value.text, "63%");
        compare(value.font.bold, true);
        compare(value.font.pixelSize, 18);
        const icon = named(block, "batteryIcon");
        compare(icon.value, 63);
        compare(icon.width, 20);
        compare(icon.height, 10);
        verify(icon.mapToItem(block, 0, 0).x >= value.mapToItem(block, value.width, 0).x, "ícone à direita do número");
        const state = named(block, "batteryStateText");
        compare(state.text, "descarregando");
        compare(state.font.capitalization, Font.AllUppercase);
        compare(state.font.pixelSize, 12);
        compare(state.font.bold, true);
        verify(state.mapToItem(block, 0, 0).y > value.mapToItem(block, 0, value.height).y - 1, "estado abaixo");
        // As linhas ficam alinhadas à esquerda do bloco.
        compare(header.mapToItem(block, 0, 0).x, 0);
        compare(value.mapToItem(block, 0, 0).x, 0);
        compare(state.mapToItem(block, 0, 0).x, 0);
    }

    function test_batteryBlock_withoutPercentShowsTheWholeSummaryAsState() {
        const block = make("BatteryBlock", "summary: \"estado desconhecido\"; percent: -1");
        compare(named(block, "batteryPercentText").visible, false);
        compare(named(block, "batteryStateText").text, "estado desconhecido");
        compare(named(block, "batteryIcon").visible, false);
    }

    function test_batteryBlock_isHiddenWithoutSummary() {
        const block = make("BatteryBlock", "summary: \"\"; percent: -1");
        compare(block.visible, false);
    }

    // ---- EqualizerView (item 9): as 10 bandas na área principal ----

    function eqProps() {
        return "width: 622; height: 650; minimumGain: -12; maximumGain: 12; "
             + "bandFrequencies: [\"32\", \"64\", \"125\", \"250\", \"500\", \"1k\", \"2k\", \"4k\", \"8k\", \"16k\"]; "
             + "gains: [4, 3, 1, 0, -1, 0, 2, 3, 2, 1]";
    }

    function test_equalizerView_hasTenVerticalRailsAt35pxStepAnd232pxHigh() {
        const view = make("EqualizerView", eqProps());
        const sliders = descendants(view).filter(i => i.objectName === "eqBandSlider");
        compare(sliders.length, 10);
        sliders.forEach((s, i) => {
            compare(s.orientation, Qt.Vertical);
            compare(s.height, 232);
            compare(s.from, -12);
            compare(s.to, 12);
            compare(s.stepSize, 1);
            compare(s.handle.width, 12);
        });
        const xs = sliders.map(s => s.mapToItem(view, 0, 0).x);
        for (let i = 1; i < 10; ++i)
            compare(xs[i] - xs[i - 1], 35);
        compare(sliders[0].value, 4);
        compare(sliders[4].value, -1);
        // O bloco de bandas fica centrado na área.
        const left = xs[0];
        const right = xs[9] + sliders[9].width;
        verify(Math.abs((left + right) / 2 - view.width / 2) <= 1, left + " " + right);
    }

    function test_equalizerView_showsFrequencyHeaderScaleAndIntegerValues() {
        const view = make("EqualizerView", eqProps());
        const freq = descendants(view).filter(i => i.objectName === "eqBandLabel");
        compare(freq.map(l => l.text), ["32", "64", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]);
        freq.forEach(l => {
            compare(l.font.pixelSize, 12);
            compare(l.font.capitalization, Font.AllUppercase);
            verify(Qt.colorEqual(l.color, "#aaaaaa"), String(l.color));
        });
        compare(named(view, "eqUnitLabel").text, "Hz");
        const values = descendants(view).filter(i => i.objectName === "eqBandValueLabel");
        compare(values.map(l => l.text), ["+4", "+3", "+1", "0", "-1", "0", "+2", "+3", "+2", "+1"]);
        verify(Qt.colorEqual(values[0].color, "#afb1b4"), String(values[0].color));
        const scale = descendants(view).filter(i => i.objectName === "eqScaleLabel").map(l => l.text);
        verify(scale.includes("+12dB") && scale.includes("0") && scale.includes("−12dB"), scale.join(","));
        const first = descendants(view).find(i => i.objectName === "eqBandLabel");
        const slider = descendants(view).find(i => i.objectName === "eqBandSlider");
        verify(first.mapToItem(view, 0, 0).y < slider.mapToItem(view, 0, 0).y, "frequências acima das bandas");
        verify(values[0].mapToItem(view, 0, 0).y > slider.mapToItem(view, 0, slider.height).y - 1, "valores abaixo");
    }

    function test_equalizerView_movingARailEmitsThatBandOnlyAndResetButtonEmitsReset() {
        const view = make("EqualizerView", eqProps());
        const moved = [];
        let resets = 0;
        view.bandMoved.connect((band, gain) => moved.push([band, gain]));
        view.resetRequested.connect(() => resets++);
        const sliders = descendants(view).filter(i => i.objectName === "eqBandSlider");
        sliders[4].value = 7;
        compare(moved.length, 0);
        sliders[4].moved();
        compare(moved, [[4, 7]]);
        const reset = named(view, "eqFlatButton");
        compare(reset.text, "Redefinir");
        compare(reset.width, 200);
        compare(reset.height, 32);
        verify(Math.abs(reset.mapToItem(view, reset.width / 2, 0).x - view.width / 2) <= 1);
        compare(view.height - (reset.mapToItem(view, 0, reset.height).y), 17);
        reset.clicked();
        compare(resets, 1);
        view.busy = true;
        compare(reset.enabled, false);
    }

    function test_equalizerView_railsFollowGainsWithoutEmitting() {
        const view = make("EqualizerView", eqProps());
        const moved = [];
        view.bandMoved.connect((band, gain) => moved.push([band, gain]));
        view.gains = [0, -2, 0, 0, 0, 0, 0, 0, 0, 12];
        const sliders = descendants(view).filter(i => i.objectName === "eqBandSlider");
        tryCompare(sliders[1], "value", -2);
        compare(sliders[9].value, 12);
        compare(moved.length, 0);
    }

    // ---- SwitchRow: rótulo à esquerda, "?" opcional e interruptor à direita (item 6) ----

    function test_switchRow_putsTheLabelOnTheLeftAndTheSwitchOnTheRight() {
        const row = make("SwitchRow", "text: \"Modo ao vivo\"; help: \"Explicação longa\"; width: 278; checked: true; "
                                      + "switchObjectName: \"theSwitch\"; helpObjectName: \"theHelp\"");
        const label = named(row, "switchRowLabel");
        const toggle = named(row, "theSwitch");
        const help = named(row, "theHelp");
        compare(label.text, "Modo ao vivo");
        compare(label.font.bold, true);
        compare(toggle.width, 48);
        compare(toggle.checked, true);
        compare(toggle.mapToItem(row, toggle.width, 0).x, row.width);
        verify(label.mapToItem(row, 0, 0).x < help.mapToItem(row, 0, 0).x);
        verify(help.mapToItem(row, 0, 0).x < toggle.mapToItem(row, 0, 0).x);
        compare(help.text, "Explicação longa");
        row.switchEnabled = false;
        compare(toggle.enabled, false);
        compare(help.enabled, true);
    }

    function test_switchRow_forwardsTheRequestedStateWithoutChangingItsOwn() {
        const row = make("SwitchRow", "text: \"EQ\"; width: 278; switchObjectName: \"theSwitch\"");
        const requests = [];
        row.toggleRequested.connect(wanted => requests.push(wanted));
        const toggle = named(row, "theSwitch");
        mouseClick(toggle);
        compare(requests, [true]);
        compare(row.checked, false);
        compare(descendants(row).find(i => i.objectName === "helpIcon").visible, false);
    }
}
