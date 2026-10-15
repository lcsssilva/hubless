import QtQuick

// Ícone de bateria de 8×16 px (medida da referência do G HUB): contorno de 1 px e enchimento proporcional ao nível.
// `value` é a porcentagem 0–100; -1 (sem nível) esconde o ícone. Nível baixo (até 20%) usa a cor de erro.
Item {
    id: icon

    property int value: -1

    visible: value >= 0
    implicitWidth: 8
    implicitHeight: 16

    // Polo da bateria.
    Rectangle {
        x: 2
        y: 0
        width: 4
        height: 2
        color: Theme.foregroundMedium
    }

    Rectangle {
        id: shell
        y: 2
        width: 8
        height: 14
        radius: 1.5
        color: "transparent"
        border.width: 1
        border.color: Theme.foregroundMedium

        Rectangle {
            objectName: "batteryFill"
            x: 2
            width: parent.width - 4
            height: Math.round((parent.height - 4) * Math.max(0, Math.min(100, icon.value)) / 100)
            y: parent.height - 2 - height
            color: icon.value <= 20 ? Theme.error : Theme.foreground
        }
    }
}
