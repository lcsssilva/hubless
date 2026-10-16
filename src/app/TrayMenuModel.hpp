#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace ghub::app {

class AppSettings;

// Um item do menu da bandeja (INC-017 R3), sem nenhum tipo de widget: o adaptador SystemTrayIcon traduz em QAction.
struct TrayMenuEntry {
    enum class Kind { Device, Separator, Profile, NoProfiles, Open, Quit };

    Kind kind = Kind::Separator;
    QString text;
    bool enabled = true;
    // Só em Kind::Profile: nome do perfil aplicado ao clicar.
    QString profile;
};

class TrayMenuModel {
public:
    // Monta o menu: um item informativo (desabilitado) por dispositivo com nível de bateria conhecido
    // ("G915 — 80 %", com o apelido se houver), separador, os perfis (clicar aplica), separador, "Abrir", "Sair".
    // `devices` são os itens de BatteryViewModel::devices (`name`, `percent`).
    [[nodiscard]] static QList<TrayMenuEntry> build(const QVariantList& devices, const AppSettings& settings,
                                                    const QStringList& profileNames);
};

} // namespace ghub::app
