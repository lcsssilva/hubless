#pragma once

#include "modules/battery/application/BatterySource.hpp"
#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/api/ExternalProcessProbe.hpp"

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

#include <chrono>
#include <optional>
#include <vector>

namespace ghub::battery {

// Bateria do G733 via `headsetcontrol -d 0x046d:0x0afe -b -o json` (ADR-0007). A consulta é
// assíncrona (QProcess) e o resultado fica em cache; readAll() nunca bloqueia. Durante cada consulta
// segura um DeviceLease da interface física informada (holder "battery.headsetcontrol"); lease ocupado
// vira erro DeviceBusy e o processo não é iniciado. O lease é liberado em finish() e no destrutor.
class HeadsetControlBatterySource final : public QObject, public BatterySource {
    Q_OBJECT

public:
    // `executable`: caminho absoluto do headsetcontrol (vazio = não encontrado). `probe` e `registry`
    // devem sobreviver a este objeto.
    HeadsetControlBatterySource(QString executable, device_access::ExternalProcessProbe& probe,
                                device_access::AccessRegistry& registry, device_access::PhysicalInterfaceId interfaceId,
                                std::chrono::milliseconds pollInterval, std::chrono::milliseconds timeout,
                                QObject* parent = nullptr);
    ~HeadsetControlBatterySource() override;

    HeadsetControlBatterySource(const HeadsetControlBatterySource&) = delete;
    HeadsetControlBatterySource& operator=(const HeadsetControlBatterySource&) = delete;
    HeadsetControlBatterySource(HeadsetControlBatterySource&&) = delete;
    HeadsetControlBatterySource& operator=(HeadsetControlBatterySource&&) = delete;

    [[nodiscard]] shared_kernel::Result<std::vector<SourcedBatteryReading>> readAll() override;

    Q_INVOKABLE void requestUpdate();

signals:
    void readingsChanged();

private:
    void startQuery();
    void finish(shared_kernel::Result<std::optional<BatteryReading>> outcome);
    void abort(shared_kernel::DomainError error);
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus status);

    QString executable_;
    device_access::ExternalProcessProbe& probe_;
    device_access::AccessRegistry& registry_;
    device_access::PhysicalInterfaceId interfaceId_;
    std::chrono::milliseconds timeout_;
    std::vector<SourcedBatteryReading> cache_;
    QProcess process_;
    QTimer pollTimer_;
    QTimer timeoutTimer_;
    QByteArray output_;
    bool running_ = false;
    std::optional<device_access::DeviceLease> lease_;
    // Resultado imposto (timeout/excesso de saída): vale quando o processo morto terminar.
    std::optional<shared_kernel::DomainError> forcedError_;
};

} // namespace ghub::battery
