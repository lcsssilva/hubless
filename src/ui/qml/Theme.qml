pragma Singleton
import QtQuick

// Tokens de design observados no G HUB, tema dark (rev/lghub-analysis/ui-ux-specification.md §3.2).
// Só valores (cores, raio, opacidades, tempos); fontes, ícones e imagens da Logitech não são usados.
// Atenção: o CSS usa #RRGGBBAA, o QML usa #AARRGGBB.
QtObject {
    // Cores. "foreground" corresponde ao onSurface do G HUB: QML trata propriedade "on<Maiúscula>" como
    // manipulador de sinal, então o nome onSurface não pode ser declarado.
    readonly property color primary: "#1196ff"
    readonly property color background: "#101113"
    readonly property color surface: "#212225"
    readonly property color error: "#f54155"
    readonly property color foreground: "#ffffff"
    readonly property color foregroundMedium: "#99ffffff"
    readonly property color foregroundDisabled: "#4dffffff"
    readonly property color outline: "#1affffff"
    readonly property color outlineHigh: "#4dffffff"

    // Forma
    readonly property int radius: 8

    // Overlays de estado
    readonly property real hoverOpacity: 0.10
    readonly property real pressOpacity: 0.15
    readonly property real selectedOpacity: 0.15

    // Movimento: desaceleração Material, cubic-bezier(0, 0, .2, 1), para Easing.BezierSpline
    readonly property int shortDuration: 150
    readonly property int mediumDuration: 250
    readonly property var easingCurve: [0.0, 0.0, 0.2, 1.0, 1.0, 1.0]
}
