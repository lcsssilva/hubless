#pragma once

#include "shared_kernel/Result.hpp"

#include <array>

namespace ghub::profiles {

// Acesso ao EQ do G733 para o módulo profiles. Só pode ser chamado na thread dona do EQ (a fila do
// controlador do audio_processing serializa o HostEqualizer; ADR-0016, ADR-0020).
class EqualizerSettingsPort {
public:
    virtual ~EqualizerSettingsPort() = default;

    // Curva em vigor (ou guardada, com o EQ desligado).
    [[nodiscard]] virtual std::array<int, 10> currentGains() const = 0;

    // Pede a curva. Ganho fora da faixa -> erro do dono, sem aplicar nada.
    [[nodiscard]] virtual shared_kernel::Result<void> requestGains(const std::array<int, 10>& gains) = 0;
};

} // namespace ghub::profiles
