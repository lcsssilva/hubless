#pragma once

#include "shared_kernel/Result.hpp"

#include <array>

namespace ghub::audio_processing {

// API pública do módulo `audio_processing` (ADR-0020): a curva do EQ do G733 vista por outros módulos.
// Chamar SOMENTE na thread dona do implementador.
class EqualizerCurveAccess {
public:
    virtual ~EqualizerCurveAccess() = default;

    // Curva desejada atual: 10 ganhos em dB, na ordem de EqualizerCurve::bandFrequenciesHz.
    [[nodiscard]] virtual std::array<int, 10> currentGains() const = 0;

    // Pede a curva. Ganho fora de -12..12 -> InvalidArgument e nada é enfileirado. Com o EQ ligado, entra em
    // vigor pela fila; desligado, só fica guardada. Nunca liga nem desliga o EQ.
    [[nodiscard]] virtual shared_kernel::Result<void> requestGains(const std::array<int, 10>& gainsDb) = 0;

protected:
    EqualizerCurveAccess() = default;
    EqualizerCurveAccess(const EqualizerCurveAccess&) = default;
    EqualizerCurveAccess& operator=(const EqualizerCurveAccess&) = default;
    EqualizerCurveAccess(EqualizerCurveAccess&&) = default;
    EqualizerCurveAccess& operator=(EqualizerCurveAccess&&) = default;
};

} // namespace ghub::audio_processing
