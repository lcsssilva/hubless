#pragma once

#include "modules/audio_processing/domain/EqualizerCurve.hpp"

#include <span>
#include <string_view>

namespace ghub::audio_processing {

// Predefinições do EQ e mapeamento Grave/Agudos (INC-018 R6/R7). Só entram predefinições cujos valores foram
// vistos na tela do G HUB; as demais não existem aqui.
class EqualizerPresets {
public:
    struct Preset {
        std::string_view name;
        EqualizerCurve curve;
    };

    // "Plano" e "Aumento de grave", nessa ordem.
    [[nodiscard]] static std::span<const Preset> all() noexcept;

    // EQ simples: Grave soma às bandas 32/64/125/250 Hz com pesos 1, 1, 0,75, 0,5; Agudos às 2k/4k/8k/16k com
    // pesos 0,5, 0,75, 1, 1. Arredonda meio-ponto para longe de zero e limita cada banda a -12..12 dB.
    [[nodiscard]] static EqualizerCurve fromBassTreble(int bassDb, int trebleDb) noexcept;
};

} // namespace ghub::audio_processing
