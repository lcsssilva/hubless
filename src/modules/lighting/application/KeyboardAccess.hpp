#pragma once

#include "modules/device_access/api/DeviceAccess.hpp"
#include "modules/device_access/api/ExternalProcessProbe.hpp"
#include "shared_kernel/Result.hpp"

#include <chrono>

namespace ghub::lighting::application::detail {

// Regras de acesso ao teclado compartilhadas pelos casos de uso de escrita (interno ao módulo lighting).

// Prazo do lease por operação (ADR-0013 decisão 3).
inline constexpr std::chrono::seconds keyboardLeaseDuration{15};

// Devolve DeviceBusy se openrgb, solaar, piper ou ratbagd estiverem em execução (INC-008 R6); o HeadsetControl só
// fala com o G733 e não bloqueia. Falha da sonda é devolvida como está.
[[nodiscard]] shared_kernel::Result<void> ensureNoExternalOwner(device_access::ExternalProcessProbe& probe);

// Pedido de lease do teclado (mesmo dono e finalidade para todos os casos de uso de iluminação).
[[nodiscard]] shared_kernel::Result<device_access::LeaseRequest>
keyboardLeaseRequest(const device_access::PhysicalInterfaceId& interfaceId, std::string purpose);

} // namespace ghub::lighting::application::detail
