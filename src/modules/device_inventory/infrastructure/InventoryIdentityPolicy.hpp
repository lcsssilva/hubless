#pragma once

#include "platform/hidpp/HidppCommandPolicy.hpp"

#include <cstdint>
#include <span>

namespace ghub::device_inventory {

// Lista fechada de pares (feature ID, função) que a leitura de identidade pode enviar por HID++ 2.0
// (ADR-0024, INC-015 R2): 0x0000 fn0; 0x0001 fn0, fn1; 0x0003 fn0, fn1; 0x0005 fn0, fn1, fn2. Tudo o mais é recusado
// antes de qualquer write — em especial a 0x0003 fn2 (pode ser o número de série), a 0x8100 e a 0x4522.
// Implementa a política injetada no canal compartilhado (ADR-0017); ignora os parâmetros.
class InventoryIdentityPolicy final : public platform::hidpp::HidppCommandPolicy {
public:
    [[nodiscard]] static bool isAllowed(std::uint16_t featureId, std::uint8_t function) noexcept;

    [[nodiscard]] bool isAllowed(std::uint16_t featureId, std::uint8_t function,
                                 std::span<const std::uint8_t> params) const noexcept override;
};

} // namespace ghub::device_inventory
