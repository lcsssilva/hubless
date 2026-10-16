#pragma once

#include "modules/battery/application/BatterySource.hpp"
#include "modules/device_access/api/DeviceAccess.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QTimer>

#include <chrono>
#include <functional>
#include <vector>

namespace ghub::battery {

// Bateria do G733 por HID++ próprio (ADR-0026) atrás do port BatterySource, com a mesma cadência e o mesmo contrato
// do adaptador do HeadsetControl: a leitura roda em thread de trabalho, o resultado fica em cache e readAll() nunca
// bloqueia; readingsChanged() avisa quando uma leitura termina. Cada leitura segura o DeviceLease da interface
// física (holder "battery.hidpp"); lease ocupado vira DeviceBusy sem tocar no dispositivo. O destrutor espera a
// leitura em andamento. `reader` (bloqueante, prazo próprio) e `registry` precisam sobreviver a este objeto.
class HidppBatterySource final : public QObject, public BatterySource {
    Q_OBJECT

public:
    using Reader = std::function<shared_kernel::Result<BatteryReading>()>;

    HidppBatterySource(Reader reader, device_access::AccessRegistry& registry,
                       device_access::PhysicalInterfaceId interfaceId, std::chrono::milliseconds pollInterval,
                       std::chrono::milliseconds followUpDelay = std::chrono::seconds{3}, QObject* parent = nullptr);
    ~HidppBatterySource() override;

    HidppBatterySource(const HidppBatterySource&) = delete;
    HidppBatterySource& operator=(const HidppBatterySource&) = delete;
    HidppBatterySource(HidppBatterySource&&) = delete;
    HidppBatterySource& operator=(HidppBatterySource&&) = delete;

    [[nodiscard]] shared_kernel::Result<std::vector<SourcedBatteryReading>> readAll() override;

    Q_INVOKABLE void requestUpdate();

public slots:
    // O headset avisou por conta própria (ligou/desligou): lê agora e uma vez mais após `followUpDelay`, quando o
    // nível já estabilizou. Rajadas de eventos reiniciam o atraso; requestUpdate ignora leitura em andamento.
    void onHeadsetEvent();

signals:
    void readingsChanged();

private:
    using Outcome = shared_kernel::Result<BatteryReading>;

    void onFinished();

    Reader reader_;
    device_access::AccessRegistry& registry_;
    device_access::PhysicalInterfaceId interfaceId_;
    QTimer pollTimer_;
    QTimer followUpTimer_;
    QFutureWatcher<Outcome> watcher_;
    std::vector<SourcedBatteryReading> cache_;
    bool running_ = false;
};

} // namespace ghub::battery
