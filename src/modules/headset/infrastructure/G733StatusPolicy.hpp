#pragma once

#include "platform/hidpp/HidppCommandPolicy.hpp"

#include <cstdint>
#include <span>

namespace ghub::headset::infrastructure {

// Lista fechada de comandos HID++ 2.0 que o módulo headset pode enviar ao G733 (ADR-0025, ADR-0026, INC-016 A5):
//   - Root fn0 (GetFeature) só com os 2 bytes do ID da feature consultada;
//   - 0x8310 fn0 e 0x8300 fn0, SEM parâmetros (leituras);
//   - 0x8300 fn1 (sidetone): exatamente 1 byte, 0..100;
//   - 0x8070 fn3 (luzes): zona 0/1 e os quatro formatos de efeito das capturas, com período e brilho nas faixas;
//   - nada mais: 0x8070 fn7/fn8, 0x8310 fn1+, 0x8010, 0x1f20 (é do módulo battery) e qualquer outra são recusados.
class G733StatusPolicy final : public platform::hidpp::HidppCommandPolicy {
public:
    [[nodiscard]] bool isAllowed(std::uint16_t featureId, std::uint8_t function,
                                 std::span<const std::uint8_t> params) const noexcept override;
};

} // namespace ghub::headset::infrastructure
