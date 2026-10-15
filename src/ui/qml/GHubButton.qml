import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Botão reto do painel no estilo do G HUB (INC-014 parte 3, R23): 32 px de altura, fundo `buttonSurface`, sem raio,
// texto MAIÚSCULO em negrito. Desabilitado usa o fundo e o texto esmaecidos. `flat` tira o fundo.
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

    contentItem: Label {
        text: control.text
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 14
        font.bold: true
        font.letterSpacing: 0.5
        font.capitalization: Font.AllUppercase
        color: control.enabled ? Theme.foreground : Theme.buttonDisabledText
    }

    background: Rectangle {
        radius: 0
        color: !control.enabled ? Theme.buttonDisabledSurface : (control.flat ? "transparent" : Theme.buttonSurface)

        // Camada de estado: realce de hover e de clique.
        Rectangle {
            objectName: "stateOverlay"
            anchors.fill: parent
            color: Theme.foreground
            opacity: !control.enabled ? 0 : (control.down ? Theme.pressOpacity : (control.hovered ? Theme.hoverOpacity : 0))

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.shortDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: Theme.easingCurve
                }
            }
        }
    }
}
