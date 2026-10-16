#pragma once

#include "platform/hidpp/HidppCommandPolicy.hpp"

#include <cstdint>
#include <span>

namespace ghub::lighting::infrastructure {

// Lista fechada de pares (feature ID, função) que o módulo de iluminação pode enviar por HID++ 2.0
// (ADR-0013, INC-008 R3). Qualquer par fora da lista é recusado antes de qualquer write.
// Implementa a política injetada no canal compartilhado (ADR-0017). Confere os parâmetros onde o par sozinho não basta:
// leituras de estado (vazios), modo de jogo (INC-019: fn3 vazio, fn1 com até 16 códigos HID do catálogo) e 0x8100 fn3
// (`00 1..3`).
class LightingHidppPolicy final : public platform::hidpp::HidppCommandPolicy {
public:
    [[nodiscard]] static bool isAllowed(std::uint16_t featureId, std::uint8_t function) noexcept;

    [[nodiscard]] bool isAllowed(std::uint16_t featureId, std::uint8_t function,
                                 std::span<const std::uint8_t> params) const noexcept override;
};

} // namespace ghub::lighting::infrastructure
