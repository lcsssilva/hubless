#include "AutostartEntry.hpp"

#include <QChar>
#include <QString>

namespace ghub::app {

AutostartEntry defaultAutostartEntry()
{
    // Exec com --autostart: o app nasce do login e já vai direto para a bandeja (INC-013 R9).
    return AutostartEntry{.name = QStringLiteral("G HUB para Linux"),
                          .comment = QStringLiteral("Gerencia periféricos Logitech em segundo plano"),
                          .exec = QStringLiteral("ghub-linux --autostart"),
                          .icon = QStringLiteral("ghub-linux")};
}

QString renderAutostartEntry(const AutostartEntry& entry)
{
    // Ordem do exemplo da spec Desktop Entry (INC-013 R6): cabeçalho primeiro e, depois, as chaves que
    // definem a entrada. `Hidden=false` é o valor padrão da chave; `X-GNOME-Autostart-enabled` não é da
    // spec (é extensão do GNOME), mas registrá-la como `true` é inofensivo e deixa a intenção explícita.
    QString text;
    text.reserve(256);
    text += QStringLiteral("[Desktop Entry]\n");
    text += QStringLiteral("Type=Application\n");
    text += QStringLiteral("Version=1.0\n");
    text += QStringLiteral("Name=");
    text += entry.name;
    text += QLatin1Char('\n');
    text += QStringLiteral("Comment=");
    text += entry.comment;
    text += QLatin1Char('\n');
    text += QStringLiteral("Exec=");
    text += entry.exec;
    text += QLatin1Char('\n');
    text += QStringLiteral("Icon=");
    text += entry.icon;
    text += QLatin1Char('\n');
    text += QStringLiteral("Terminal=false\n");
    text += QStringLiteral("StartupNotify=false\n");
    text += QStringLiteral("Categories=Utility;\n");
    text += QStringLiteral("Hidden=false\n");
    text += QStringLiteral("X-GNOME-Autostart-enabled=true\n");
    return text;
}

} // namespace ghub::app
