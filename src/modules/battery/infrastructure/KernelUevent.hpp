#pragma once

#include <linux/netlink.h>
#include <sys/socket.h>

#include "modules/battery/infrastructure/UniqueFd.hpp"
#include "shared_kernel/Result.hpp"

#include <string_view>

namespace ghub::battery {

// Socket NETLINK_KOBJECT_UEVENT no grupo 1 (uevents do kernel), não bloqueante, sem privilégio
// (NL_CFG_F_NONROOT_RECV, kobject_uevent.c:776-779). Falha → BackendUnavailable.
[[nodiscard]] shared_kernel::Result<UniqueFd> openKernelUeventSocket();

// Filtro de uevents do kernel (ADR-0005). Datagrama: "ACTION@DEVPATH\0" + "CHAVE=valor\0"...
// (kobject_uevent.c v7.2.9). Verdadeiro só para add/remove/change do subsistema power_supply.
[[nodiscard]] bool isPowerSupplyUevent(std::string_view datagram) noexcept;

// Remetente é o kernel: sockaddr_nl completo, família AF_NETLINK e nl_pid 0.
[[nodiscard]] bool isFromKernel(const sockaddr_nl& sender, socklen_t senderLength) noexcept;

} // namespace ghub::battery
