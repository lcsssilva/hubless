#include "modules/battery/infrastructure/UeventBatteryMonitor.hpp"

#include "modules/battery/infrastructure/KernelUevent.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <string_view>
#include <utility>

namespace ghub::battery {

namespace {

// Variáveis de uevent cabem em UEVENT_BUFFER_SIZE (2048, kobject.h:33) + cabeçalho ACTION@DEVPATH.
constexpr std::size_t datagramBufferBytes = 8192;

} // namespace

std::optional<ReceivedDatagram> receiveNetlinkDatagram(int fd, std::span<char> buffer)
{
    sockaddr_nl sender{};
    socklen_t senderLength = sizeof(sender);
    // MSG_TRUNC: o retorno é o tamanho real do datagrama, mesmo maior que o buffer.
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast): API de sockets exige sockaddr*.
    const auto size = ::recvfrom(fd, buffer.data(), buffer.size(), MSG_DONTWAIT | MSG_TRUNC,
                                 reinterpret_cast<sockaddr*>(&sender), &senderLength);
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
    if (size < 0 && errno == ENOBUFS) {
        return ReceivedDatagram{.size = 0, .truncated = false, .sender = {}, .senderLength = 0, .overrun = true};
    }
    if (size < 0) {
        return std::nullopt; // EAGAIN (nada pendente) ou erro: o QSocketNotifier avisa de novo se houver dados
    }
    const auto received = static_cast<std::size_t>(size);
    return ReceivedDatagram{
        .size = std::min(received, buffer.size()),
        .truncated = received > buffer.size(),
        .sender = sender,
        .senderLength = senderLength,
    };
}

UeventBatteryMonitor::UeventBatteryMonitor(UniqueFd socket, DatagramReceiver receiver, QObject* parent)
    : QObject(parent), socket_(std::move(socket)), receiver_(std::move(receiver)),
      notifier_(socket_.get(), QSocketNotifier::Read)
{
    connect(&notifier_, &QSocketNotifier::activated, this, &UeventBatteryMonitor::drain);
}

void UeventBatteryMonitor::drain()
{
    std::array<char, datagramBufferBytes> buffer{};
    bool changed = false;
    while (const auto datagram = receiver_(socket_.get(), buffer)) {
        if (datagram->overrun) {
            changed = true; // algum uevent perdido pode ter sido de power_supply
            continue;
        }
        if (datagram->truncated || !isFromKernel(datagram->sender, datagram->senderLength)) {
            continue;
        }
        changed = changed || isPowerSupplyUevent(std::string_view{buffer.data(), datagram->size});
    }
    if (changed) {
        emit batteryChanged();
    }
}

} // namespace ghub::battery
