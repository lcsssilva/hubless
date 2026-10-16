#pragma once

#include "platform/hidpp/HidppCommandPolicy.hpp"

#include <cstdint>
#include <span>

namespace ghub::headset::infrastructure {

// Lista fechada de comandos HID++ 2.0 que o módulo headset pode enviar ao G733 (ADR-0025, INC-015 parte 2 R11):
//   - Root fn0 (GetFeature) só com os 2 bytes do ID da feature consultada;
//   - 0x8310 fn0 (quantidade de bandas do EQ) e 0x8300 fn0 (leitura não interpretada), SEM parâmetros;
//   - nada mais: 0x8300 fn1 (escrita de sidetone), 0x8310 fn1+, 0x0001 e qualquer outra feature são recusados.
class G733StatusPolicy final : public platform::hidpp::HidppCommandPolicy {
public:
    [[nodiscard]] bool isAllowed(std::uint16_t featureId, std::uint8_t function,
                                 std::span<const std::uint8_t> params) const noexcept override;
};

} // namespace ghub::headset::infrastructure
