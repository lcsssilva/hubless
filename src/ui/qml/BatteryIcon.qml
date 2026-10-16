import QtQuick

// Ícone de bateria horizontal de 20×10 px (como no G HUB): contorno de 1 px, polo à direita e enchimento
// proporcional ao nível. `value` é a porcentagem 0–100; -1 (sem nível) esconde o ícone. Nível baixo (até 20%) usa
// a cor de erro.
Item {
    id: icon

    property int value: -1

    visible: value >= 0
    implicitWidth: 20
    implicitHeight: 10

    // Polo da bateria.
    Rectangle {
        x: 18
        y: 3
        width: 2
        height: 4
        color: Theme.foregroundMedium
    }

    Rectangle {
        id: shell
        width: 18
        height: 10
        radius: 1.5
        color: "transparent"
        border.width: 1
        border.color: Theme.foregroundMedium

        Rectangle {
            objectName: "batteryFill"
            x: 2
            y: 2
            height: parent.height - 4
            width: Math.round((parent.width - 4) * Math.max(0, Math.min(100, icon.value)) / 100)
            color: icon.value <= 20 ? Theme.error : Theme.foreground
        }
    }
}
