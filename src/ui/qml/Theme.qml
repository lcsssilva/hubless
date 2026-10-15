pragma Singleton
import QtQuick

// Tokens de design observados no G HUB, tema dark (rev/lghub-analysis/ui-ux-specification.md §3.2).
// Só valores (cores, raio, opacidades, tempos); fontes, ícones e imagens da Logitech não são usados.
// Atenção: o CSS usa #RRGGBBAA, o QML usa #AARRGGBB.
QtObject {
    // Cores. "foreground" corresponde ao onSurface do G HUB: QML trata propriedade "on<Maiúscula>" como
    // manipulador de sinal, então o nome onSurface não pode ser declarado.
    readonly property color primary: "#1196ff"
    // Preto puro, como a janela do G HUB 2026.6 (REFERENCIA-ESTILO-GHUB.md §1); o #101113 vinha do CSS do rev/.
    readonly property color background: "#000000"
    readonly property color surface: "#212225"
    readonly property color error: "#f54155"
    readonly property color foreground: "#ffffff"
    readonly property color foregroundMedium: "#99ffffff"
    readonly property color foregroundDisabled: "#4dffffff"
    readonly property color outline: "#1affffff"
    readonly property color outlineHigh: "#4dffffff"

    // Tokens de layout medidos nas capturas próprias (INC-014 parte 3, R18).
    readonly property color divider: "#2b303b"          // linha de 1 px sob a barra do topo
    readonly property color buttonSurface: "#393939"    // botões retos do painel
    readonly property color sliderFill: "#afb1b4"       // trecho cheio do trilho do slider
    readonly property color sliderTrack: "#393939"      // trecho vazio do trilho do slider
    readonly property color cardHover: "#2e2f31"       // fundo do cartão com o mouse em cima
    readonly property color buttonDisabledSurface: "#222222"
    readonly property color buttonDisabledText: "#999999"
    readonly property color tabForeground: "#c8cdd7"    // abas inativas da barra do topo

    // Desenhos próprios dos dispositivos (DeviceDrawing): corpo, contorno e detalhes, sem imagens de terceiros.
    readonly property color deviceBody: "#34363a"
    readonly property color deviceEdge: "#55585e"
    readonly property color deviceDetail: "#46494e"

    // Forma
    readonly property int radius: 8
    readonly property int cardRadius: 12                // cartões e painéis

    // Overlays de estado
    readonly property real hoverOpacity: 0.10
    readonly property real pressOpacity: 0.15
    readonly property real selectedOpacity: 0.15

    // Movimento: desaceleração Material, cubic-bezier(0, 0, .2, 1), para Easing.BezierSpline
    readonly property int shortDuration: 150
    readonly property int mediumDuration: 250
    readonly property var easingCurve: [0.0, 0.0, 0.2, 1.0, 1.0, 1.0]
}
