import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Diálogo modal no estilo do G HUB (INC-014 parte 5): centrado, fundo `surface`, raio de cartão, título de 24 px,
// mensagem em texto secundário e botões retos; o primário é azul e o secundário (opcional) é cinza. Só apresenta e
// emite `accepted`/`rejected`; quem usa decide o que fazer.
Popup {
    id: dialog
    objectName: "ghubDialog"

    property string title: ""
    property string message: ""
    property string primaryText: qsTr("Entendi")
    // Vazio = sem botão secundário.
    property string secondaryText: ""
    readonly property alias primaryButton: primary
    readonly property alias secondaryButton: secondary

    signal accepted()
    signal rejected()

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    width: 420
    padding: 25

    Overlay.modal: Rectangle { color: "#99000000" }

    background: Rectangle {
        color: Theme.surface
        radius: Theme.cardRadius
        border.width: 1
        border.color: Theme.outline
    }

    contentItem: ColumnLayout {
        spacing: 16

        Label {
            objectName: "dialogTitle"
            Layout.fillWidth: true
            text: dialog.title
            visible: text !== ""
            wrapMode: Text.WordWrap
            font.pixelSize: 24
            font.bold: true
            color: Theme.foreground
        }

        Label {
            objectName: "dialogMessage"
            Layout.fillWidth: true
            text: dialog.message
            visible: text !== ""
            wrapMode: Text.WordWrap
            font.pixelSize: 14
            color: Theme.foregroundMedium
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: 8
            spacing: 8

            GHubButton {
                id: secondary
                objectName: "dialogSecondaryButton"
                Layout.fillWidth: false
                visible: dialog.secondaryText !== ""
                text: dialog.secondaryText
                onClicked: {
                    dialog.close()
                    dialog.rejected()
                }
            }

            GHubButton {
                id: primary
                objectName: "dialogPrimaryButton"
                Layout.fillWidth: false
                text: dialog.primaryText
                onClicked: {
                    dialog.close()
                    dialog.accepted()
                }
                background: Rectangle {
                    color: Theme.primary

                    Rectangle {
                        anchors.fill: parent
                        color: Theme.foreground
                        opacity: primary.down ? Theme.pressOpacity : (primary.hovered ? Theme.hoverOpacity : 0)
                    }
                }
            }
        }
    }
}
