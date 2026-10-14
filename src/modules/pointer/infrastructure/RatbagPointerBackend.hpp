#pragma once

#include "modules/pointer/application/PointerBackend.hpp"

#include <QDBusConnection>
#include <QString>

#include <chrono>
#include <vector>

namespace ghub::pointer {

// Lê a configuração dos mouses pelo ratbagd (D-Bus), somente leitura (ADR-0011 §4). Só envia
// `org.freedesktop.DBus.Properties.Get` e `GetAll`, montados com QDBusMessage::createMethodCall
// (nunca QDBusInterface, que faz Introspect). Chamadas bloqueantes: use fora da thread de UI.
// Em produção: barramento de sistema, `org.freedesktop.ratbag1` e prazo de 30 s por chamada.
class RatbagPointerBackend final : public PointerBackend {
public:
    RatbagPointerBackend(QDBusConnection connection, QString service, std::chrono::milliseconds callTimeout);

    [[nodiscard]] shared_kernel::Result<std::vector<DiscoveredPointer>> readDevices() override;

private:
    QDBusConnection connection_;
    QString service_;
    std::chrono::milliseconds callTimeout_;
};

} // namespace ghub::pointer
