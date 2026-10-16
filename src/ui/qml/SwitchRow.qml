import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Linha de painel com o rótulo à esquerda (negrito), o "?" opcional e o interruptor à direita. O estado (`checked`)
// vem de quem usa — o ViewModel — e o clique só emite `toggleRequested(wanted)` (ver GHubSwitch).
Item {
    id: row

    property string text: ""
    property string help: ""
    property bool checked: false
    // Habilita só o interruptor; o "?" continua respondendo ao mouse.
    property bool switchEnabled: true
    property string helpObjectName: "helpIcon"
    property string switchObjectName: "switchControl"

    signal toggleRequested(bool wanted)

    Layout.fillWidth: true
    implicitHeight: Math.max(24, label.height)

    Label {
        id: label
        objectName: "switchRowLabel"
        anchors.verticalCenter: parent.verticalCenter
        width: Math.min(implicitWidth, row.width - 54 - 16 - (row.help !== "" ? 28 : 0))
        text: row.text
        wrapMode: Text.WordWrap
        font.pixelSize: 14
        font.bold: true
        color: Theme.foreground
    }

    HelpIcon {
        objectName: row.helpObjectName
        visible: row.help !== ""
        x: label.width + 8
        anchors.verticalCenter: parent.verticalCenter
        text: row.help
    }

    GHubSwitch {
        objectName: row.switchObjectName
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        checked: row.checked
        enabled: row.switchEnabled
        Accessible.name: row.text
        onToggleRequested: wanted => row.toggleRequested(wanted)
    }
}
