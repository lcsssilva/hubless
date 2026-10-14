#pragma once

#include "modules/battery/infrastructure/UniqueFd.hpp"

#include <QObject>
#include <QSocketNotifier>

#include <linux/netlink.h>
#include <sys/socket.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <span>

namespace ghub::battery {

struct ReceivedDatagram {
    std::size_t size;
    bool truncated;
    sockaddr_nl sender;
    socklen_t senderLength;
    bool overrun = false; // ENOBUFS: o kernel descartou uevents porque o buffer do socket encheu
};

// Lê um datagrama sem bloquear; nullopt quando não há mais dados (EAGAIN) ou em erro.
using DatagramReceiver = std::function<std::optional<ReceivedDatagram>(int fd, std::span<char> buffer)>;

// Receptor de produção: recvfrom(MSG_DONTWAIT | MSG_TRUNC) com o endereço do remetente.
[[nodiscard]] std::optional<ReceivedDatagram> receiveNetlinkDatagram(int fd, std::span<char> buffer);

// Emite batteryChanged() quando o kernel anuncia mudança em power_supply (ADR-0005).
// Vários datagramas lidos de uma vez geram um único sinal. Roda na thread do objeto.
class UeventBatteryMonitor final : public QObject {
    Q_OBJECT

public:
    explicit UeventBatteryMonitor(UniqueFd socket, DatagramReceiver receiver = receiveNetlinkDatagram,
                                  QObject* parent = nullptr);

signals:
    void batteryChanged();

private:
    void drain();

    UniqueFd socket_;
    DatagramReceiver receiver_;
    QSocketNotifier notifier_;
};

} // namespace ghub::battery
