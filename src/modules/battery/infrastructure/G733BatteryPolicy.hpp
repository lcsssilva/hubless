#pragma once

#include "platform/hidpp/HidppCommandPolicy.hpp"

#include <cstdint>
#include <span>

namespace ghub::battery {

// Lista fechada de comandos HID++ 2.0 que o módulo battery pode enviar ao G733 (ADR-0026, INC-016 A5):
//   - Root fn0 (GetFeature) só para a feature 0x1f20 (os 2 bytes exatos do ID);
//   - 0x1f20 fn0 (leitura da medição de bateria), SEM parâmetros;
//   - nada mais.
class G733BatteryPolicy final : public platform::hidpp::HidppCommandPolicy {
public:
    [[nodiscard]] bool isAllowed(std::uint16_t featureId, std::uint8_t function,
                                 std::span<const std::uint8_t> params) const noexcept override;
};

} // namespace ghub::battery
