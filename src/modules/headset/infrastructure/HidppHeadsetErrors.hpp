#pragma once

#include "platform/hidpp/HidppError.hpp"
#include "shared_kernel/Result.hpp"

namespace ghub::headset::infrastructure {

// Erro HID++ -> erro de domínio, compartilhado pelas leituras e escritas do G733. Erro 0x05 do headset =
// "headset desligado ou fora de alcance" (pista da captura; ADR-0023).
[[nodiscard]] shared_kernel::DomainError toDomainError(const platform::hidpp::HidppError& error);

} // namespace ghub::headset::infrastructure
