import QtQuick
import QtQuick.Controls
import QtQuick.Effects

// Dropdown no estilo do G HUB (REFERENCIA-ESTILO-GHUB.md §4.3): só o valor em MAIÚSCULAS e branco com um chevron
// pequeno, sem caixa nem fundo; a lista aberta é uma caixa `buttonSurface` de raio 4 e sombra, com no mínimo 240 px, que abre por cima do campo, com itens de passo
// 24 px (o item sob o cursor em branco, os demais em `dropdownItem`). O comportamento é o do ComboBox do Qt
// Quick Controls (model, currentIndex, activated, currentText, currentValue...).
ComboBox {
    id: control

    // Mostrado no lugar do valor enquanto nada foi escolhido (currentIndex -1).
    property string placeholderText: ""

    implicitHeight: 24
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4
    Accessible.name: control.currentIndex >= 0 ? control.displayText : control.placeholderText

    background: null

    contentItem: Label {
        objectName: "comboValue"
        text: control.currentIndex >= 0 ? control.displayText : control.placeholderText
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        rightPadding: 18
        font.pixelSize: 13
        font.weight: Font.Medium
        font.letterSpacing: 0.5
        font.capitalization: Font.AllUppercase
        color: Theme.foreground
    }

    // Chevron de 10×6 px feito de dois traços inclinados.
    indicator: Item {
        objectName: "comboChevron"
        x: control.width - width
        y: (control.height - height) / 2
        width: 10
        height: 6

        Rectangle {
            x: -0.5
            y: 2.5
            width: 6.5
            height: 1.5
            rotation: 40
            antialiasing: true
            color: Theme.foreground
        }

        Rectangle {
            x: 4
            y: 2.5
            width: 6.5
            height: 1.5
            rotation: -40
            antialiasing: true
            color: Theme.foreground
        }
    }

    delegate: ItemDelegate {
        id: entry
        objectName: "comboItem"
        required property int index
        opacity: entryHover.hovered || entry.highlighted || control.currentIndex === entry.index ? 1 : 0.4

        width: ListView.view ? ListView.view.width : 240
        height: 24
        padding: 0
        leftPadding: 12
        rightPadding: 12
        hoverEnabled: true
        // O cursor manda; o realce do teclado só vale enquanto o mouse não está sobre a lista.
        highlighted: control.highlightedIndex === entry.index && !listHover.hovered

        contentItem: Label {
            text: control.textAt(entry.index)
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 13
            font.weight: Font.Medium
            font.capitalization: Font.AllUppercase
            color: Theme.foreground
        }

        background: null

        HoverHandler {
            id: entryHover
        }
    }

    popup: Popup {
        x: -8
        y: -8
        width: Math.max(240, control.width)
        margins: 4
        padding: 8
        topPadding: 8
        bottomPadding: 8
        leftPadding: 0
        rightPadding: 0

        contentItem: ListView {
            clip: true
            implicitHeight: Math.min(contentHeight, 24 * 8)
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            interactive: contentHeight > height

            HoverHandler {
                id: listHover
            }
        }

        background: Rectangle {
            radius: Theme.radiusSmall
            color: Theme.buttonSurface
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: "#80000000"
                shadowBlur: 1.0
                shadowVerticalOffset: 0
                shadowHorizontalOffset: 0
            }
        }
    }
}
