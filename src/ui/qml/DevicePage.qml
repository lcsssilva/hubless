import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Página de um dispositivo (INC-014 parte 3, R21/R22): trilho de ícones à esquerda (48×48, passo de 52 px, com a
// aba "info" como engrenagem no rodapé), painel lateral de 310 px com o título e a página da aba, e à direita a
// área principal com o desenho do dispositivo e o bloco de bateria. A aba "info" não usa painel nem área principal:
// mostra a página de três colunas (`infoContent`) ao lado do trilho. Uma aba pode trocar o desenho da área
// principal por uma vista própria (`mainViewTab` + `mainView`), como o equalizador. Só apresentação.
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
    // Chave da aba cuja vista própria (`mainView`) substitui o desenho na área principal; vazia = nenhuma.
    property string mainViewTab: ""
    signal tabSelected(string key)
    // As páginas das abas, na ordem de `tabs`, vão para a pilha do painel.
    default property alias pages: tabStack.data
    // Vista própria da área principal (um item que preenche a área) e página Informações (um item que preenche a página).
    property alias mainView: mainViewHost.data
    property alias infoContent: infoHost.data

    readonly property var currentTabEntry: tabs.find(t => t.key === currentTab) ?? null
    readonly property bool infoMode: currentTab === "info"

    // Trilho: abas em coluna (passo de 52 px = 48 + 4) e, no fim, a engrenagem "info" colada à base.
    ColumnLayout {
        id: rail
        objectName: "tabRail"
        // Acima da página Informações, que ocupa a largura toda e senão engoliria os cliques do trilho.
        z: 1
        // Acima da página Informações, que ocupa a largura toda e senão engoliria os cliques do trilho.
        x: 14
        y: 35
        width: 48
        height: page.height - 35 - 20
        spacing: 4

        Repeater {
            model: page.tabs.filter(t => t.key !== "info")

            delegate: SideTab {
                required property var modelData
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                tabKey: modelData.key
                text: modelData.text
                icon.source: Qt.resolvedUrl("icons/" + modelData.icon + ".svg")
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
                icon.source: Qt.resolvedUrl("icons/" + modelData.icon + ".svg")
                selected: page.currentTab === modelData.key
                onClicked: page.tabSelected(modelData.key)
            }
        }
    }

    PanelCard {
        objectName: "devicePanel"
        visible: !page.infoMode
        x: 76
        y: 15
        width: 310
        height: page.height - 15 - 20
        title: page.currentTabEntry !== null ? page.currentTabEntry.text : page.deviceName

        StackLayout {
            id: tabStack
            anchors.fill: parent
            currentIndex: Math.max(0, page.tabs.findIndex(t => t.key === page.currentTab))
        }
    }

    Item {
        objectName: "mainArea"
        visible: !page.infoMode
        x: 400
        width: page.width - 400
        height: page.height

        DeviceDrawing {
            kind: page.deviceKind
            visible: page.currentTab !== page.mainViewTab
            anchors.centerIn: parent
            boxWidth: 450
            boxHeight: 300
        }

        Item {
            id: mainViewHost
            objectName: "mainView"
            anchors.fill: parent
            visible: page.mainViewTab !== "" && page.currentTab === page.mainViewTab
        }

        // Bloco de bateria no canto superior direito, alinhado à esquerda e começando 282 px antes da borda da área.
        BatteryBlock {
            objectName: "mainBatteryBlock"
            x: parent.width - 282
            y: 2
            summary: page.batteryText
            percent: page.batteryPercent
        }
    }

    Item {
        id: infoHost
        objectName: "infoHost"
        visible: page.infoMode
        x: 0
        width: page.width
        height: page.height
    }
}
