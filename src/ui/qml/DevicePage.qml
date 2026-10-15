import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Página de um dispositivo (INC-014 parte 3, R21/R22): trilho de ícones à esquerda (48×48, passo de 52 px, com a
// aba "info" como engrenagem no rodapé), painel lateral de 310 px com o título e a página da aba, e à direita a
// área principal com o desenho do dispositivo e a bateria no canto superior direito. Só apresentação.
Item {
    id: page

    property string deviceName: ""
    // "keyboard", "mouse", "headset" ou "camera": escolhe o desenho da área principal.
    property string deviceKind: "keyboard"
    // [{key, text, icon}]; a aba de chave "info" vai para a engrenagem do rodapé do trilho.
    property var tabs: []
    property string currentTab: ""
    // Resumo e nível da bateria do dispositivo; texto vazio = dispositivo sem bateria.
    property string batteryText: ""
    property int batteryPercent: -1
    signal tabSelected(string key)
    // As páginas das abas, na ordem de `tabs`, vão para a pilha do painel.
    default property alias pages: tabStack.data

    readonly property var currentTabEntry: tabs.find(t => t.key === currentTab) ?? null

    // Trilho: abas em coluna (passo de 52 px = 48 + 4) e, no fim, a engrenagem "info" colada à base.
    ColumnLayout {
        id: rail
        objectName: "tabRail"
        x: 13
        y: 28
        width: 48
        height: page.height - 28 - 5
        spacing: 4

        Repeater {
            model: page.tabs.filter(t => t.key !== "info")

            delegate: SideTab {
                required property var modelData
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                tabKey: modelData.key
                text: modelData.text
                icon.name: modelData.icon
                selected: page.currentTab === modelData.key
                onClicked: page.tabSelected(modelData.key)
            }
        }

        Item { Layout.fillHeight: true }

        Repeater {
            model: page.tabs.filter(t => t.key === "info")

            delegate: SideTab {
                required property var modelData
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                tabKey: modelData.key
                text: modelData.text
                icon.name: modelData.icon
                selected: page.currentTab === modelData.key
                onClicked: page.tabSelected(modelData.key)
            }
        }
    }

    PanelCard {
        objectName: "devicePanel"
        x: 75
        y: 8
        width: 310
        height: page.height - 8 - 5
        title: page.currentTabEntry !== null ? page.currentTabEntry.text : page.deviceName

        StackLayout {
            id: tabStack
            anchors.fill: parent
            currentIndex: Math.max(0, page.tabs.findIndex(t => t.key === page.currentTab))
        }
    }

    Item {
        objectName: "mainArea"
        x: 400
        width: page.width - 400
        height: page.height

        DeviceDrawing {
            kind: page.deviceKind
            anchors.centerIn: parent
            boxWidth: 450
            boxHeight: 300
        }

        Column {
            objectName: "mainBatteryBlock"
            visible: page.batteryText !== ""
            anchors.top: parent.top
            anchors.topMargin: 16
            anchors.right: parent.right
            anchors.rightMargin: 40
            spacing: 6

            Label {
                anchors.right: parent.right
                text: qsTr("Nível de bateria")
                font.pixelSize: 13
                font.letterSpacing: 0.5
                font.capitalization: Font.AllUppercase
                color: Theme.foregroundMedium
            }

            Row {
                anchors.right: parent.right
                spacing: 8

                Label {
                    objectName: "mainBatteryText"
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.batteryText
                    font.pixelSize: 18
                    font.bold: true
                }

                BatteryIcon {
                    objectName: "mainBatteryIcon"
                    anchors.verticalCenter: parent.verticalCenter
                    value: page.batteryPercent
                }
            }
        }
    }
}
