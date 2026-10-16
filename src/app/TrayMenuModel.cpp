#include "TrayMenuModel.hpp"

#include "AppSettings.hpp"

#include <QVariantMap>

namespace ghub::app {

QList<TrayMenuEntry> TrayMenuModel::build(const QVariantList& devices, const AppSettings& settings,
                                          const QStringList& profileNames)
{
    using Kind = TrayMenuEntry::Kind;
    QList<TrayMenuEntry> entries;
    for (const QVariant& value : devices) {
        const QVariantMap device = value.toMap();
        const int percent = device.value(QStringLiteral("percent"), -1).toInt();
        if (percent < 0) {
            continue; // não detectado ou sem nível: nada a mostrar
        }
        entries.push_back({.kind = Kind::Device,
                           .text = QStringLiteral("%1 — %2 %")
                                       .arg(settings.displayName(device.value(QStringLiteral("name")).toString()))
                                       .arg(percent),
                           .enabled = false,
                           .profile = {}});
    }
    if (!entries.isEmpty()) {
        entries.push_back({.kind = Kind::Separator, .text = {}, .enabled = true, .profile = {}});
    }
    if (profileNames.isEmpty()) {
        entries.push_back(
            {.kind = Kind::NoProfiles, .text = QStringLiteral("Nenhum perfil salvo"), .enabled = false, .profile = {}});
    }
    for (const QString& name : profileNames) {
        entries.push_back({.kind = Kind::Profile, .text = name, .enabled = true, .profile = name});
    }
    entries.push_back({.kind = Kind::Separator, .text = {}, .enabled = true, .profile = {}});
    entries.push_back({.kind = Kind::Open, .text = QStringLiteral("Abrir"), .enabled = true, .profile = {}});
    entries.push_back({.kind = Kind::Quit, .text = QStringLiteral("Sair"), .enabled = true, .profile = {}});
    return entries;
}

} // namespace ghub::app
