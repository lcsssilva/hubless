#pragma once

#include "platform/hidpp/HidppCommandPolicy.hpp"

#include <cstdint>
#include <span>

namespace ghub::lighting::infrastructure {

// Lista fechada de pares (feature ID, função) que o módulo de iluminação pode enviar por HID++ 2.0
// (ADR-0013, INC-008 R3). Qualquer par fora da lista é recusado antes de qualquer write.
// Implementa a política injetada no canal compartilhado (ADR-0017); ignora os parâmetros.
class LightingHidppPolicy final : public platform::hidpp::HidppCommandPolicy {
public:
    [[nodiscard]] static bool isAllowed(std::uint16_t featureId, std::uint8_t function) noexcept;

    [[nodiscard]] bool isAllowed(std::uint16_t featureId, std::uint8_t function,
                                 std::span<const std::uint8_t> params) const noexcept override;

    // Features que o produto nunca aciona (lista explícita de bloqueio, ADR-0013 decisão 4).
    [[nodiscard]] static bool isExplicitlyBlocked(std::uint16_t featureId) noexcept;
};

} // namespace ghub::lighting::infrastructure
