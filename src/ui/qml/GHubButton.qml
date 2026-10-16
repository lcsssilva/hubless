import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Botão do painel no estilo do G HUB (ESTILO-GHUB-CSS-COMPUTADO.md §5.3): 32 px de altura, fundo `buttonSurface`,
// raio 4 (reto com `square`, como o da área principal), texto MAIÚSCULO em negrito; hover deixa o texto cinza,
// clique reduz a opacidade a 0,7. Desabilitado usa o fundo e o texto esmaecidos. `flat` tira o fundo.
Button {
    id: control

    // Num painel, o botão ocupa a largura da coluna (278 px no painel de 310); em Flow vale a largura natural.
    Layout.fillWidth: true
    implicitWidth: Math.max(80, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: 32
    leftPadding: 16
    rightPadding: 16
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    // Botão da área principal: reto.
    property bool square: false
    opacity: down ? 0.7 : 1

    contentItem: Label {
        text: control.text
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 14
        font.bold: true
        font.letterSpacing: 0.5
        font.capitalization: Font.AllUppercase
        color: !control.enabled ? Theme.buttonDisabledText : (control.hovered ? Theme.sectionHeader : Theme.foreground)
    }

    background: Rectangle {
        radius: control.square ? 0 : Theme.radiusSmall
        color: !control.enabled ? Theme.buttonDisabledSurface : (control.flat ? "transparent" : Theme.buttonSurface)

        // Foco de teclado: contorno de 3 px para dentro.
        Rectangle {
            visible: control.visualFocus
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 3
            border.color: Theme.focusRing
        }
    }
}
