import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cabeçalho de seção de um painel (INC-014 parte 3, R23): MAIÚSCULAS, 13 px, espaçamento de letras de 0,5 e 60% de
// branco. Só tipografia; o texto vem de quem usa.
Label {
    objectName: "sectionTitle"

    Layout.topMargin: 6
    Layout.fillWidth: true
    wrapMode: Text.WordWrap
    font.pixelSize: 13
    font.weight: Font.Medium
    font.letterSpacing: 0.5
    font.capitalization: Font.AllUppercase
    color: Theme.foregroundMedium
}
