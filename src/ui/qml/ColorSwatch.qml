import QtQuick
import QtQuick.Controls

// Amostra de cor circular de 20 px (REFERENCIA-ESTILO-GHUB.md §4.3, seletor de cor): disco da cor, anel branco de
// 2 px quando selecionada. Com `plus: true` vira o botão redondo "+" (cor personalizada). O nome da cor é o nome
// acessível e o texto do tooltip. Só apresenta e emite `clicked`.
AbstractButton {
    id: control

    property color swatchColor: "#ffffff"
    property string colorName: ""
    property bool selected: false
    property bool plus: false

    implicitWidth: 20
    implicitHeight: 20
    padding: 0
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4
    Accessible.name: colorName

    ToolTip.visible: hovered && colorName !== ""
    ToolTip.text: colorName
    ToolTip.delay: 400

    contentItem: Label {
        objectName: "swatchPlus"
        visible: control.plus
        text: "+"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 16
        color: Theme.foreground
    }

    background: Item {
        Rectangle {
            objectName: "swatchDisc"
            anchors.fill: parent
            radius: width / 2
            antialiasing: true
            color: control.plus ? "transparent" : control.swatchColor
            border.width: control.plus ? 1 : 0
            border.color: Theme.sliderFill
        }

        Rectangle {
            objectName: "swatchRing"
            anchors.fill: parent
            anchors.margins: -3
            radius: width / 2
            antialiasing: true
            visible: control.selected
            color: "transparent"
            border.width: 2
            border.color: Theme.foreground
        }
    }
}
