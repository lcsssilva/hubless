#include "modules/battery/infrastructure/KernelUevent.hpp"

#include <cerrno>
#include <string>
#include <system_error>

namespace ghub::battery {

namespace {

// Valor da chave entre os campos "CHAVE=valor" separados por '\0'; vazio se ausente.
std::string_view field(std::string_view datagram, std::string_view key) noexcept
{
    while (!datagram.empty()) {
        const auto end = datagram.find('\0');
        const auto current = datagram.substr(0, end);
        if (current.size() > key.size() && current.starts_with(key) && current.substr(key.size(), 1) == "=") {
            return current.substr(key.size() + 1);
        }
        if (end == std::string_view::npos) {
            break;
        }
        datagram.remove_prefix(end + 1);
    }
    return {};
}

} // namespace

shared_kernel::Result<UniqueFd> openKernelUeventSocket()
{
    UniqueFd socket{::socket(AF_NETLINK, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT)};
    if (!socket.valid()) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   "socket(NETLINK_KOBJECT_UEVENT): " + std::system_category().message(errno));
    }
    sockaddr_nl address{};
    address.nl_family = AF_NETLINK;
    address.nl_groups = 1; // grupo dos uevents do kernel (kobject_uevent.c: .groups = 1, dst_group = 1)
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): API de sockets exige sockaddr*.
    if (::bind(socket.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        return shared_kernel::fail(shared_kernel::ErrorCode::BackendUnavailable,
                                   "bind(uevent): " + std::system_category().message(errno));
    }
    return socket;
}

bool isPowerSupplyUevent(std::string_view datagram) noexcept
{
    // Cabeçalho do kernel "<ação>@<devpath>" (mensagens do udev começam com "libudev").
    const auto headerEnd = datagram.find('\0');
    if (headerEnd == std::string_view::npos || !datagram.substr(0, headerEnd).contains('@')) {
        return false;
    }
    datagram.remove_prefix(headerEnd + 1);
    const auto action = field(datagram, "ACTION");
    const bool relevantAction = action == "add" || action == "remove" || action == "change";
    return relevantAction && field(datagram, "SUBSYSTEM") == "power_supply";
}

bool isFromKernel(const sockaddr_nl& sender, socklen_t senderLength) noexcept
{
    // O kernel envia uevents com portid 0 (kobject_uevent.c:299-303, :731-733).
    return senderLength == sizeof(sockaddr_nl) && sender.nl_family == AF_NETLINK && sender.nl_pid == 0;
}

} // namespace ghub::battery
