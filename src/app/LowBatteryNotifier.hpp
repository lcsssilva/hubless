#pragma once

#include "modules/battery/domain/LowBatteryAlert.hpp"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <functional>

namespace ghub::app {

class AppSettings;

// Aviso de bateria fraca (INC-017 R1/R2, ADR-0028): aplica a regra pura `LowBatteryAlert` (uma por dispositivo)
// às leituras do BatteryViewModel e pede a notificação "<dispositivo>: bateria em N %". Só Qt Core: quem mostra
// a mensagem é injetado (`ShowMessage`, ligado ao QSystemTrayIcon::showMessage em main); sem bandeja ele devolve
// false e o aviso vai para o registro de diagnóstico. Liga/limiar vêm do AppSettings (desligado por padrão).
class LowBatteryNotifier final : public QObject {
    Q_OBJECT

public:
    using ShowMessage = std::function<bool(const QString& title, const QString& message)>;
    using LogMessage = std::function<void(const QString& message)>;

    // `settings` precisa sobreviver a este objeto.
    LowBatteryNotifier(AppSettings& settings, ShowMessage show, LogMessage log, QObject* parent = nullptr);

public slots:
    // Itens de `BatteryViewModel::devices`: `name`, `percent` (-1 sem nível) e `charging`.
    void onDevicesChanged(const QVariantList& devices);

private:
    void evaluate();

    AppSettings* settings_; // não nulo
    ShowMessage show_;
    LogMessage log_;
    QHash<QString, battery::LowBatteryAlert> alerts_;
    QVariantList lastDevices_;
};

} // namespace ghub::app
