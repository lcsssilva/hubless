#pragma once

#include <cstdint>

namespace ghub::lighting::infrastructure {

// Lista fechada de pares (feature ID, função) que o produto pode enviar por HID++ 2.0 (ADR-0013, INC-008 R3).
// Qualquer par fora da lista é recusado antes de qualquer write.
class HidppCommandPolicy {
public:
    [[nodiscard]] static bool isAllowed(std::uint16_t featureId, std::uint8_t function) noexcept;

    // Features que o produto nunca aciona (lista explícita de bloqueio, ADR-0013 decisão 4).
    [[nodiscard]] static bool isExplicitlyBlocked(std::uint16_t featureId) noexcept;
};

} // namespace ghub::lighting::infrastructure
