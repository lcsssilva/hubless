import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cabeçalho de seção de um painel com o ícone "?" opcional ao lado (REFERENCIA-ESTILO-GHUB.md §4.3): o título é o
// SectionTitle (MAIÚSCULO, 13 px, secundário); `help` é o texto longo que antes ficava solto no painel. O título
// quebra de linha se faltar espaço e o "?" fica logo depois do texto.
Item {
    id: header

    property string text: ""
    property string help: ""
    // Nome do objeto do ícone (testes e acessibilidade); o padrão é "helpIcon".
    property string helpObjectName: "helpIcon"
    property color titleColor: Theme.foregroundMedium

    Layout.fillWidth: true
    Layout.topMargin: 6
    implicitHeight: Math.max(title.height, 20)

    SectionTitle {
        id: title
        objectName: "sectionTitle"
        Layout.topMargin: 0
        width: Math.min(implicitWidth, header.width - (header.help !== "" ? 28 : 0))
        color: header.titleColor
        text: header.text
    }

    HelpIcon {
        objectName: header.helpObjectName
        visible: header.help !== ""
        x: title.width + 8
        y: Math.round((title.height - height) / 2)
        text: header.help
    }
}
