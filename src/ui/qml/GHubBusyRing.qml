import QtQuick

// Indicador de carregamento fino do G HUB (INC-014 parte 5): anel de 30 px com um arco azul que gira, visto no
// cartão "Baixando recursos…". Só gira enquanto `running` (e fica invisível parado), para não gastar quadros.
Item {
    id: ring
    objectName: "busyRing"

    property bool running: false
    property color color: Theme.primary
    property real rotationValue: 0

    width: 30
    height: 30
    visible: running

    Canvas {
        anchors.fill: parent
        rotation: ring.rotationValue
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.lineWidth = 3;
            ctx.lineCap = "round";
            ctx.strokeStyle = String(ring.color);
            ctx.beginPath();
            ctx.arc(width / 2, height / 2, width / 2 - 3, 0, Math.PI * 1.5);
            ctx.stroke();
        }
        Component.onCompleted: requestPaint()
    }

    NumberAnimation on rotationValue {
        running: ring.running && ring.visible
        from: 0
        to: 360
        duration: 900
        loops: Animation.Infinite
    }
}
